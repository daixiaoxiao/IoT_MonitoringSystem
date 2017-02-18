#define _GNU_SOURCE 
/*-----------------------------------------------------------------------------
		include files
------------------------------------------------------------------------------*/
#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdlib.h> 
#include <stdio.h> 
#include <time.h>
#include <semaphore.h>
#include <assert.h>

#include "errmacros.h"
#include "config.h"
#include "sbuffer.h"
#include "datamgr.h"
#include "connmgr.h"
#include "sensor_db.h"

/*------------------------------------------------------------------------------
		definitions (defines, typedefs, ...)
------------------------------------------------------------------------------*/
#define FIFO_NAME  "logFifo" 
#define FILE_NAME  "gateway.log"     
#define MAX  100

/*------------------------------------------------------------------------------
		global variable declarations
------------------------------------------------------------------------------*/
sbuffer_t * fir_buffer;
sbuffer_t * sec_buffer;
FILE        *fp;
sem_t        fifo_sem;
pthread_mutex_t mutexsum;

/*------------------------------------------------------------------------------
		function declarations
------------------------------------------------------------------------------*/
void print_help                     (void);
void final_message               (void) ;
void run_log_process            (int exit_code);
void manage_threads           (int port);
void get_info_from_fifo         (char buf[MAX], FILE * fp_fifo, FILE * fp_log);
int   callback_func		      (void *data, int argc, char **argv, char **azColName); 

/*------------------------------------------------------------------------------
		implementation code
------------------------------------------------------------------------------*/
void *conn_mgr( void *port){
  int port_arg = *(int *)port;
  connmgr_listen(port_arg, &fir_buffer);
  connmgr_free();
  DEBUG_PRINT("conn_mgr exit!\n");
  pthread_exit( NULL );
}

void *data_mgr( void *id){
  FILE * fp_sensor_map = fopen("room_sensor.map", "r");
  FILE_OPEN_ERROR(fp_sensor_map);
  datamgr_parse_sensor_data(fp_sensor_map, &fir_buffer); 
  datamgr_free();
  DEBUG_PRINT("datamgr exit!\n");
  pthread_exit( NULL ); 
}

void *storage_mgr( void *id){
  DBCONN *db = init_connection(1);
  assert(db != NULL);
  storagemgr_parse_sensor_data( db, &sec_buffer);  
  disconnect(db);
  DEBUG_PRINT("storage_mgr exit!\n");
  pthread_exit( NULL );
}

int main( int argc, char *argv[] ){
  pid_t my_pid, child_pid; 
  
  atexit( final_message );
  my_pid = getpid();
  printf("Parent process (pid = %d) is started ...\n", my_pid);
  
  int server_port;
  
  child_pid = fork();
  SYSCALL_ERROR(child_pid);
  
  if ( child_pid == 0  )
  {  
    /* child's code */
    run_log_process(50);
  }
  else{
    /* parent’s code */
    DEBUG_PRINT("Parent process (pid = %d) has created child process (pid = %d)...\n", my_pid, child_pid);
    if (argc != 2)
    {
      print_help();
      exit(EXIT_SUCCESS);
    }
    else{
      server_port = atoi(argv[1]);
    }
    
    manage_threads(server_port);
  }
  
  int presult;
  
  presult = sbuffer_free( &fir_buffer );
  DEBUG_PRINT("free first buffer\n");
  SBUFFER_ERROR(presult);
  
  presult = sbuffer_free( &sec_buffer );
  DEBUG_PRINT("free second buffer\n");
  SBUFFER_ERROR(presult);
  
  exit(EXIT_SUCCESS);
}

void manage_threads(int port){
    int               presult;
    pthread_t     thread_connmgr     ;
    pthread_t     thread_datamgr      ;
    pthread_t     thread_storagemgr ;
    
    presult= sem_init(&fifo_sem, 0, 1);
    ERROR_HANDLER(presult);
    
    /* Create the FIFO if it does not exist */ 
    presult = mkfifo(FIFO_NAME, 0666);
    CHECK_MKFIFO(presult); 
    
    fp = fopen(FIFO_NAME, "w"); 
    DEBUG_PRINT("syncing with reader ok\n");
    FILE_OPEN_ERROR(fp);
    
    presult = sbuffer_init(&fir_buffer);
    SBUFFER_ERROR(presult);
    
    presult = sbuffer_init(&sec_buffer);
    SBUFFER_ERROR(presult);
    
    presult = pthread_create( &thread_connmgr, NULL, &conn_mgr, (void*) &port );
    ERROR_HANDLER(presult);
    
    presult = pthread_create( &thread_datamgr, NULL, &data_mgr, NULL );
    ERROR_HANDLER(presult);
    
    presult = pthread_create( &thread_storagemgr, NULL, &storage_mgr, NULL );
    ERROR_HANDLER(presult);
    
    presult= pthread_join(thread_connmgr, NULL);
    ERROR_HANDLER(presult);
    presult= pthread_join(thread_datamgr, NULL);
    ERROR_HANDLER(presult);
    presult= pthread_join(thread_storagemgr, NULL);
    ERROR_HANDLER(presult);
    
    presult = fclose( fp );
    FILE_CLOSE_ERROR(presult);
    
    presult = sem_destroy(&fifo_sem);
    ERROR_HANDLER(presult);
    
    pthread_exit(NULL);
    DEBUG_PRINT("main thread exit!\n");
}

void run_log_process(int exit_code){
  FILE *fp_logfile, *fp_gateway; 
  int result;
  char recv_buf[MAX]; 
  
  /* Create the FIFO if it does not exist */ 
  result = mkfifo(FIFO_NAME, 0666);
  CHECK_MKFIFO(result); 
  
  fp_logfile = fopen(FIFO_NAME, "r"); 
  DEBUG_PRINT("syncing with writer ok\n");
  FILE_OPEN_ERROR(fp_logfile);
  
  fp_gateway = fopen(FILE_NAME, "w"); 
  DEBUG_PRINT("open the file gateway.log\n");
  FILE_OPEN_ERROR(fp_gateway);
  
  /* Enter while loop to read fifo */
  get_info_from_fifo(recv_buf, fp_logfile, fp_gateway);
  
  result = fclose( fp_gateway);
  FILE_CLOSE_ERROR(result);
  
  result = fclose( fp_logfile);
  FILE_CLOSE_ERROR(result);
  
  DEBUG_PRINT("fifo reader exit!\n");
  exit(exit_code); 
}

void get_info_from_fifo(char buf[MAX], FILE * fp_fifo, FILE * fp_log){
  char *str_result, *send_buf;
  time_t timestamp;
  int result, sequence_num = 0;
  do 
  {
    str_result = fgets(buf, MAX, fp_fifo);
    if ( str_result != NULL )
    { 
      printf("Message received: %s", buf); 
      /* puts the message to gateway.log */
      ASPRINTF_ERROR(asprintf( &send_buf, "%d %ld %s\n",  sequence_num, (long int)(time(&timestamp)), buf));
      sequence_num++;
      
      result = fputs( send_buf, fp_log );
      FILE_PUTS_ERROR(result);
      DEBUG_PRINT("puts message %s to gateway.log\n", send_buf);
      FFLUSH_ERROR(fflush(fp_log));
      free( send_buf );
      sleep(1);
      }
  } while ( str_result != NULL ); 
}

void final_message(void) 
{
  pid_t pid = getpid();
  printf("Process %d is now exiting ...\n", pid);
}

void print_help(void)
{
  printf("Use this program with 2 command line options: \n");
  printf("\t%-15s : TCP server port number\n", "\'server port\'");
}

int callback_func(void *data, int argc, char **argv, char **azColName){
   int i;
   fprintf(stderr, "%s: ", (const char*)data);
   for(i=0; i<argc; i++){
      printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
   }
   printf("\n");
   return 0;
}