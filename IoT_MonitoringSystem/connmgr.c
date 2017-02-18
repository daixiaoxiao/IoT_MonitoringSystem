#define _GNU_SOURCE	
/*-----------------------------------------------------------------------------
		include files
------------------------------------------------------------------------------*/
#include <sys/types.h>
#include <sys/socket.h>
#include <poll.h>
#include <assert.h>
#include <time.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <semaphore.h>

#include "lib/tcpsock.h"
#include "lib/dplist.h"
#include "config.h"
#include "errmacros.h"
#include "sbuffer.h"

#ifndef  TIMEOUT
  #error "undefined TIMEOUT"
#endif

/*------------------------------------------------------------------------------
		global variable declarations
------------------------------------------------------------------------------*/
extern sem_t fifo_sem;
extern FILE *fp;

int       dplist_errno;
static  dplist_t * client_list = NULL;
static  struct pollfd * pollfd_ptr = NULL;
static  sbuffer_data_t * data_temp = NULL;

/*------------------------------------------------------------------------------
		definitions (defines, typedefs, ...)
------------------------------------------------------------------------------*/
typedef struct{
  int                   fd;
  tcpsock_t *      sock_ptr;
  sensor_data_t data;
  bool                 if_log_to_fifo;
}socket_node;

/*------------------------------------------------------------------------------
		function declarations
------------------------------------------------------------------------------*/
void           dpl_print                              (dplist_t * list);
void           sbuffer_print                       (sbuffer_t * ptr);
void *        connmgr_element_copy       (void * element); 	// Duplicate 'element'; If needed allocated new memory for the duplicated element.
void           connmgr_element_free        (void ** element);	// If needed, free memory allocated to element
int             connmgr_element_compare (void * x, void * y);  // Compare two element elements; returns -1 if x<y, 0 if x==y, or 1 if x>y 
tcpsock_t * get_socket_by_fd                 (dplist_t * list, int fd);
void *         get_client_by_fd                  (dplist_t * list, int fd);
void            log_to_fifo                           ( char * buf, sem_t sema, FILE * fp_t );
void            connmgr_free();

/*------------------------------------------------------------------------------
		implementation code
------------------------------------------------------------------------------*/
void connmgr_listen(int port_number, sbuffer_t ** buffer){
  
  tcpsock_t * server;
  tcpsock_t * client;
  int              i, bytes, socket_alive = 0;
  double       time_out;
  sensor_data_t data;
  time_t         current_time;
  char *         send_buf; 
  
#ifdef DEBUG
  FILE * fp_text;
#endif
 
#ifdef DEBUG // save sensor data also in text format for test purposes
  fp_text = fopen("sensor_data_recv_text", "w");
  FILE_OPEN_ERROR(fp_text);
#endif
  
  pollfd_ptr = malloc(sizeof(struct pollfd));
  assert(pollfd_ptr != NULL);
  
  //the pointer for insert data into sbuffer!
  data_temp = malloc(sizeof(sbuffer_data_t));   
  assert(data_temp != NULL);
  
  client_list = dpl_create(&connmgr_element_copy, &connmgr_element_free, &connmgr_element_compare);
  assert(client_list != NULL);
  
  printf("the main server is started\n");
  if (tcp_passive_open(&server,port_number)!=TCP_NO_ERROR) exit(EXIT_FAILURE);
  socket_alive++;
  
  if (tcp_get_sd(server, &(pollfd_ptr->fd)) != TCP_NO_ERROR)exit(EXIT_FAILURE); 
  pollfd_ptr->events = POLLIN;
  
  while(socket_alive){
    int result = poll( pollfd_ptr, socket_alive, TIMEOUT * 1000); 
    SYSCALL_ERROR( result );                                                      
    
    if(result == 0){
      printf("the server port Timeout, connmgr Exit!\n");
      break;
    }
    
    if ( pollfd_ptr[0].revents & POLLIN ) {
      if (tcp_wait_for_connection(server,&client)!=TCP_NO_ERROR) exit(EXIT_FAILURE);
      printf("Incoming client connection\n");
      socket_alive++;
      
      pollfd_ptr = realloc(pollfd_ptr, sizeof(struct pollfd) * socket_alive);
      if(pollfd_ptr == NULL)connmgr_free();
      assert(pollfd_ptr != NULL);
      
      if (tcp_get_sd(client, &(pollfd_ptr[socket_alive - 1].fd)) != TCP_NO_ERROR)exit(EXIT_FAILURE); 
      pollfd_ptr[socket_alive - 1].events = POLLIN;
      
      void * socket_node_ptr = malloc(sizeof(socket_node));
      assert(socket_node_ptr != NULL);
      
      ((socket_node *)socket_node_ptr)->fd = pollfd_ptr[socket_alive - 1].fd;
      ((socket_node *)socket_node_ptr)->sock_ptr = client;
      ((socket_node *)socket_node_ptr)->data.id = 0;
      ((socket_node *)socket_node_ptr)->data.value = 0;
      ((socket_node *)socket_node_ptr)->data.ts = 0;
      ((socket_node *)socket_node_ptr)->if_log_to_fifo = 0;
      
      client_list = dpl_insert_at_index( client_list, socket_node_ptr, dpl_size(client_list), true);
      free(socket_node_ptr);

#ifdef DEBUG
      dpl_print(client_list);
#endif
      continue;  
    }
    
    for(i = 1; i != socket_alive; i++){
      
      if(pollfd_ptr[i].revents & POLLIN){
	
	tcpsock_t * temp = get_socket_by_fd(client_list, pollfd_ptr[i].fd);
	assert(temp != NULL);
	socket_node * node_ptr_t =  (socket_node *)get_client_by_fd(client_list, pollfd_ptr[i].fd);
	assert(node_ptr_t != NULL);
        
	// read sensor ID
	bytes = sizeof(data.id);
        tcp_receive(temp,(void *)&data.id,&bytes);
	// read temperature
	bytes = sizeof(data.value);
        tcp_receive(temp,(void *)&data.value,&bytes);
	// read timestamp
	bytes = sizeof(data.ts);
	result = tcp_receive(temp,(void *)&data.ts,&bytes);
	if ((result==TCP_NO_ERROR) && bytes) 
	{
	  printf("sensor id =%" PRIu16 " - temperature = %g - timestamp = %ld\n", data.id, data.value, (long int)data.ts);
	  
 	  /* write current data to temperary memory*/ 
	  data_temp->sensor_data.id = data.id;
	  data_temp->sensor_data.value = data.value;
	  data_temp->sensor_data.ts = data.ts;
	  
	  if( sbuffer_insert( *buffer, data_temp) == SBUFFER_FAILURE){
            printf("writer thread insertion failure!\n");
	    exit(EXIT_FAILURE);
          }

#ifdef DEBUG
	  sbuffer_print( *buffer );
	  fprintf(fp_text,"%" PRIu16 " %g %ld\n", data.id , data.value, (long)data.ts);    
#endif 
	  
	  assert(node_ptr_t->fd == pollfd_ptr[i].fd);
	  node_ptr_t->data.id = data.id;
	  node_ptr_t->data.value = data.value;
	  node_ptr_t->data.ts = data.ts;
	  
	  if( node_ptr_t->if_log_to_fifo == 0 ){
	    //write output to FIFO
	    ASPRINTF_ERROR(asprintf( &send_buf, "A sensor node with %" PRIu16 " has opened a new connection\n", node_ptr_t->data.id));
	    
            log_to_fifo( send_buf, fifo_sem, fp );
	    node_ptr_t->if_log_to_fifo = 1;
	  }
	}
	
	time(&current_time);
	time_out = difftime(current_time, node_ptr_t->data.ts);
	if(time_out >= (double)TIMEOUT ){
	  if (tcp_close( &temp)!=TCP_NO_ERROR) exit(EXIT_FAILURE);
	  
	  //write output to FIFO
	  ASPRINTF_ERROR(asprintf( &send_buf, "A sensor node with %" PRIu16 " has closed the connection\n", node_ptr_t->data.id));
	  
	  log_to_fifo( send_buf, fifo_sem, fp );
	  
	  client_list = dpl_remove_element( client_list, node_ptr_t, true );
#ifdef DEBUG
	  dpl_print(client_list);
#endif
	  DEBUG_PRINT("TIMEOUT, Peer fd %d has closed connection, Close the socket.\n", pollfd_ptr[i].fd);
	  
	  pollfd_ptr[i].fd = -1;
	  pollfd_ptr[i].events = 0;
	}
      }
    }
  }
  if (tcp_close( &server )!=TCP_NO_ERROR) exit(EXIT_FAILURE);
#ifdef DEBUG
    fclose(fp_text);
#endif
}

void connmgr_free(){
  free(pollfd_ptr);
  free(data_temp);
  dpl_free(&client_list);
}

tcpsock_t * get_socket_by_fd(dplist_t * list, int fd){
  int i;
  for(i=0; i != dpl_size(list); i++){
    void * ptr = dpl_get_element_at_index( list, i );
    if ((((socket_node *)ptr)->fd) == fd){
      return (((socket_node *)ptr)->sock_ptr);
    }
  }
  return NULL;
}

void * get_client_by_fd(dplist_t * list, int fd){
  int i;
  for (i=0; i != dpl_size(list); i++){
    void * ptr = dpl_get_element_at_index( list, i );
    if ((((socket_node *)ptr)->fd) == fd){
      return (void *)ptr;
    }
  }
  return NULL;
}

/* write log_event to fifo */
void log_to_fifo( char * buf, sem_t sema, FILE * fp_t ){
  int presult;	  
  presult = sem_wait( &sema );
  ERROR_HANDLER(presult);
  
  if ( fputs( buf, fp_t ) == EOF )
  {
    fprintf( stderr, "Error writing data to fifo.\n");
    exit( EXIT_FAILURE );
  } 
  FFLUSH_ERROR(fflush(fp_t));
  DEBUG_PRINT("Message send to fifo: %s\n", buf);
  free( buf );
  
  presult = sem_post( &sema );
  ERROR_HANDLER(presult);
}

void dpl_print( dplist_t * list )
{
  int i, length;

  length = dpl_size(list);
  assert(dplist_errno == DPLIST_NO_ERROR);
  for ( i = 0; i < length; i++)    
  {
    void * element;
    element = dpl_get_element_at_index(list, i);
    assert(dplist_errno == DPLIST_NO_ERROR);
    printf("element at index %d = " "%" PRIu16 "\n", i, ((socket_node *)element)->data.id);
  }
}

void sbuffer_print(sbuffer_t * ptr){
  int i;
  for(i = 0; i != sbuffer_size(ptr); i++){
     sbuffer_data_t * data_ptr_t = sbuffer_get_element_at_index( ptr, i);
     assert(data_ptr_t != NULL);
     printf("sensor %" PRIu16 " is at index %d of the sbuffer\n",data_ptr_t->sensor_data.id, i);
  }
}

void * connmgr_element_copy(void * element){
  socket_node *p;
  p = malloc( sizeof( socket_node ));
  assert ( p != NULL );
  *p = *( socket_node *)element;
  return (void *)p;
}

void connmgr_element_free(void ** element){	// If needed, free memory allocated to element
   free(*element);
  *element = NULL;
}

int connmgr_element_compare(void * x, void * y){  // Compare two element elements; returns -1 if x<y, 0 if x==y, or 1 if x>y
   socket_node * a = (socket_node *)x;
   socket_node * b = (socket_node *)y;
   if ((*a).fd < (*b).fd){
      return -1;
  }
  else if ((*a).fd == (*b).fd ){
    return 0;
  }
  else{
    return 1;
  }
}