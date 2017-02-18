#define _GNU_SOURCE
/*-----------------------------------------------------------------------------
		include files
------------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h> 
#include <string.h>
#include <semaphore.h>
#include <assert.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>

#include "sensor_db.h"
#include "connmgr.h"
#include "config.h"

/*------------------------------------------------------------------------------
		definitions (defines, typedefs, ...)
------------------------------------------------------------------------------*/
#define LOOP_TIME 5

/*------------------------------------------------------------------------------
		global variable declarations
------------------------------------------------------------------------------*/
extern sem_t fifo_sem;
extern FILE  *fp; 

extern void   log_to_fifo( char * buf, sem_t sema, FILE * fp_t );

/*------------------------------------------------------------------------------
		implementation code
------------------------------------------------------------------------------*/
/*
 * Reads continiously all data from the shared buffer data structure and stores this into the database
 * When *buffer becomes NULL the method finishes. This method will NOT automatically disconnect from the db
 */
void storagemgr_parse_sensor_data(DBCONN * conn, sbuffer_t ** buffer){
  char * send_buf;
  if(conn == NULL){
    #ifdef DEBUG
    fprintf(stderr, "Connection lost:\n");
    #endif
    ASPRINTF_ERROR(asprintf( &send_buf, "Connection to SQL server lost"));
    log_to_fifo( send_buf, fifo_sem, fp );
    
    conn = retry_connection();
  }
  sbuffer_data_t * data_ptr = malloc(sizeof(sbuffer_data_t));
  assert(data_ptr != NULL);

  int loop = 1;
  while( loop ){
    int state =  sbuffer_remove_block( *buffer, data_ptr, TIMEOUT);
    if(state == SBUFFER_NO_DATA)break;
    else if (state == SBUFFER_FAILURE)ERROR_HANDLER(state); 
    else{
      int flag = insert_sensor( conn, data_ptr->sensor_data.id, data_ptr->sensor_data.value, data_ptr->sensor_data.ts);
      if(flag == -1){
	DEBUG_PRINT("An error occured during insert_sensor.\n");
	exit(EXIT_FAILURE);
      }
      DEBUG_PRINT("Insert_sensor successed.\n");
      /*force a context switch*/ 
      usleep(500000);
    }
  }
  free(data_ptr);
}

/*
 * Make a connection to the database server
 * Create (open) a database with name DB_NAME having 1 table named TABLE_NAME  
 * If the table existed, clear up the existing data if clear_up_flag is set to 1
 * Return the connection for success, NULL if an error occurs
 */
DBCONN * init_connection(char clear_up_flag){
   sqlite3 *db;
   char *zErrMsg = 0;
   int rc, loop = LOOP_TIME;
   char * sql, *send_buf;
   
   while( sqlite3_open("DB_NAME", &db)){
      usleep(100000);
      fprintf(stderr, "Can't open database: %s, Still try %d times\n", sqlite3_errmsg(db), loop);
      loop--;
      if(loop == 0){
	#ifdef DEBUG
	fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
	#endif
	ASPRINTF_ERROR(asprintf( &send_buf, "Unable to connect to SQL server"));
	log_to_fifo( send_buf, fifo_sem, fp );
	exit(0);
      }
   }
   #ifdef DEBUG
   fprintf(stderr, "Opened database successfully\n");
   #endif
   ASPRINTF_ERROR(asprintf( &send_buf, "Connection to SQL server established.\n"));
   log_to_fifo( send_buf, fifo_sem, fp );
   
   if(clear_up_flag == 1){
             sql = "DROP TABLE IF EXISTS TABLE_NAME;"   
                        "CREATE TABLE TABLE_NAME(" \
		      "id INTEGER PRIMARY KEY      AUTOINCREMENT, " \
		      "sensor_id             INT                      NOT NULL,"\
		      "sensor_value       DECIMAL(4,2)     NOT NULL, " \
		      "timestamp           TIMESTAMP        NOT NULL);"; 
  }
  else{ 
                sql =  "CREATE TABLE TABLE_NAME(" \
			  "id INT PRIMARY KEY      AUTOINCREMENT, " \
			  "sensor_id             INT                      NOT NULL,"\
			  "sensor_value       DECIMAL(4,2)     NOT NULL, " \
			  "timestamp           TIMESTAMP        NOT NULL);"; 
  }		
      /* Execute SQL statement */
   rc = sqlite3_exec(db, sql, 0, 0, &zErrMsg);
   if( rc != SQLITE_OK ){
      fprintf(stderr, "SQL error: %s\n", zErrMsg);
      sqlite3_free(zErrMsg);
      return NULL;
   }else{
      const char *str1 = "SensorData";
      ASPRINTF_ERROR(asprintf( &send_buf, "New table %s created.\n", str1));
      log_to_fifo( send_buf, fifo_sem, fp );
   }
  return db;
}

/* 
 * reconnect to the SQL once the connection is lost
 */
DBCONN * retry_connection(void){
   sqlite3 *db;
   char * send_buf;
   int loop = LOOP_TIME;
   
   while( sqlite3_open("DB_NAME", &db)){
      usleep(100000);
      fprintf(stderr, "Can't re_open database: %s, Still try %d times\n", sqlite3_errmsg(db), loop);
      loop--;
      if(loop == 0){
	ASPRINTF_ERROR(asprintf( &send_buf, "Unable to connect to SQL server"));
	log_to_fifo( send_buf, fifo_sem, fp );
	exit(0);
      }
   }
   #ifdef DEBUG
   fprintf(stderr, "Re_opened database successfully\n");
   #endif
   ASPRINTF_ERROR(asprintf( &send_buf, "Connection to SQL server established."));
   log_to_fifo( send_buf, fifo_sem, fp );
   
   return db;
}

/*
 * Disconnect from the database server
 */
void disconnect(DBCONN *conn){
  sqlite3_close(conn);
}

/*
 * Write an INSERT query to insert a single sensor measurement
 * Return zero for success, and non-zero if an error occurs
 */
int insert_sensor(DBCONN * conn, sensor_id_t id, sensor_value_t value, sensor_ts_t ts){
  int rc;  
  sqlite3_stmt *stmt; 
  char * send_buf;
  
  if(conn == NULL){
    #ifdef DEBUG
    fprintf(stderr, "Connection lost:\n");
    #endif
    ASPRINTF_ERROR(asprintf( &send_buf, "Connection to SQL server lost"));
    log_to_fifo( send_buf, fifo_sem, fp );
    
    conn = retry_connection();
  }
  
  /* Create SQL statement */
  char * sql = NULL;
  int i = asprintf(&sql, "INSERT INTO TABLE_NAME (sensor_id,sensor_value,timestamp) values (%hd, %f, %ld);", id, value, (long int)ts);
  if(i == -1){
    fprintf(stdout, "There is an error occured on asprintf\n");
    return -1;
  }
  sqlite3_prepare_v2(conn, sql, strlen(sql), &stmt, NULL);
    
   /* Execute SQL statement */
   rc = sqlite3_step(stmt);
   if( rc != SQLITE_DONE ){
      fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(conn));
      free(sql);
      return -1;
   }else{
      fprintf(stdout, "Mysqlite Records created successfully\n");
   }
   free(sql);
   sqlite3_finalize(stmt);
   return 0;
}

/*
 * Write an INSERT query to insert all sensor measurements available in the file 'sensor_data'
 * Return zero for success, and non-zero if an error occurs
 */
int insert_sensor_from_file(DBCONN * conn, FILE * sensor_data){
   char *zErrMsg = 0;
   int rc;  
   char * sql = NULL;
   sqlite3_stmt *stmt; 
   
   sensor_id_t sensor_ID;
   sensor_value_t Value;
   sensor_ts_t Ts;
   while (!feof(sensor_data)){
    int i = fread(&sensor_ID,sizeof(uint16_t),1,sensor_data);

    int j = fread(&Value,sizeof(double),1,sensor_data);

    int k = fread(&Ts,sizeof(time_t),1,sensor_data);

    if ( i == 1  && j == 1 && k == 1) {
      /* Create SQL statement */

      int p = asprintf(&sql, "INSERT INTO TABLE_NAME (sensor_id,sensor_value,timestamp) values (%hd, %f, %ld);", sensor_ID, Value, (long int)Ts);
      if(p == -1){
      fprintf(stdout, "There is an error occured on asprintf\n");
      return -1;
      }
      sqlite3_prepare_v2(conn, sql, strlen(sql), &stmt, NULL);
      /* Execute SQL statement */
      rc = sqlite3_step(stmt);
      if( rc != SQLITE_DONE ){
	  fprintf(stderr, "SQL error: %s\n", zErrMsg);
	  sqlite3_free(zErrMsg);
	  return -1;
      }
  //     else{
  // 	//fprintf(stdout, "Records created successfully\n");
  //     }
    }
   else{
       fprintf(stdout, "fread reach end of file or failed\n");
       fclose(sensor_data);
       break;
   }
  }
  free(sql);
  sqlite3_finalize(stmt);
  return 0;
}

/*
  * Write a SELECT query to select all sensor measurements in the table 
  * The callback function is applied to every row in the result
  * Return zero for success, and non-zero if an error occurs
  */
int find_sensor_all(DBCONN * conn, callback_t f){
   char *zErrMsg = 0;
   int rc;
   char *sql;
   const char* data = "Callback function called";
   /* Create SQL statement */
   sql = "SELECT * from TABLE_NAME";

   /* Execute SQL statement */
   rc = sqlite3_exec(conn, sql, f, (void*)data, &zErrMsg);
   if( rc != SQLITE_OK ){
      fprintf(stderr, "SQL error: %s\n", zErrMsg);
      sqlite3_free(zErrMsg);
      return -1;
   }else{
      fprintf(stdout, "Operation done successfully\n");
   }
   return 0;
}

/*
 * Write a SELECT query to return all sensor measurements having a temperature of 'value'
 * The callback function is applied to every row in the result
 * Return zero for success, and non-zero if an error occurs
 */
int find_sensor_by_value(DBCONN * conn, sensor_value_t value, callback_t f){
   char *zErrMsg = 0;
   int rc;
   char *sql;
   const char* data = "Callback function called";
   
   int i = asprintf(&sql, "SELECT * from TABLE_NAME WHERE sensor_value = ( %f );", value);
   if(i == -1){
      fprintf(stdout, "There is an error occured on asprintf\n");
      return -1;
   }
    
   /* Execute SQL statement */
   rc = sqlite3_exec(conn, sql, f, (void*)data, &zErrMsg);
   if( rc != SQLITE_OK){
      fprintf(stderr, "SQL error: %s\n", zErrMsg);
      sqlite3_free(zErrMsg);
      return -1;
   }else{
      fprintf(stdout, "Operation done successfully\n");
   }
   free(sql);
   return 0;
}

/*
 * Write a SELECT query to return all sensor measurements of which the temperature exceeds 'value'
 * The callback function is applied to every row in the result
 * Return zero for success, and non-zero if an error occurs
 */
int find_sensor_exceed_value(DBCONN * conn, sensor_value_t value, callback_t f){
   char *zErrMsg = 0;
   int rc;
   char *sql;
   const char* data = "Callback function called";
   /* Create SQL statement */
   int i = asprintf(&sql, "SELECT * from TABLE_NAME WHERE sensor_value > ( %f );", value);
   if(i == -1){
      fprintf(stdout, "There is an error occured on asprintf\n");
      return -1;
   }

   /* Execute SQL statement */
   rc = sqlite3_exec(conn, sql, f, (void*)data, &zErrMsg);
   if( rc != SQLITE_OK ){
      fprintf(stderr, "SQL error: %s\n", zErrMsg);
      sqlite3_free(zErrMsg);
      return -1;
   }else{
      fprintf(stdout, "Operation done successfully\n");
   }
   free(sql);
   return 0;
}

/*
 * Write a SELECT query to return all sensor measurements having a timestamp 'ts'
 * The callback function is applied to every row in the result
 * Return zero for success, and non-zero if an error occurs
 */
int find_sensor_by_timestamp(DBCONN * conn, sensor_ts_t ts, callback_t f){
   char *zErrMsg = 0;
   int rc;
   char *sql;
   const char* data = "Callback function called";
   /* Create SQL statement */
   int i = asprintf(&sql, "SELECT * from TABLE_NAME WHERE timestamp = ( %ld );", (long int)ts);
   if(i == -1){
      fprintf(stdout, "There is an error occured on asprintf\n");
      return -1;
   }

   /* Execute SQL statement */
   rc = sqlite3_exec(conn, sql, f, (void*)data, &zErrMsg);
   if( rc != SQLITE_OK ){
      fprintf(stderr, "SQL error: %s\n", zErrMsg);
      sqlite3_free(zErrMsg);
      return -1;
   }else{
      fprintf(stdout, "Operation done successfully\n");
   }
   free(sql);
   return 0;
}


/*
 * Write a SELECT query to return all sensor measurements recorded after timestamp 'ts'
 * The callback function is applied to every row in the result
 * return zero for success, and non-zero if an error occurs
 */
int find_sensor_after_timestamp(DBCONN * conn, sensor_ts_t ts, callback_t f){
   char *zErrMsg = 0;
   int rc;
   char *sql;
   const char* data = "Callback function called";
  
   /* Create SQL statement */ 
   int i = asprintf(&sql, "SELECT * from TABLE_NAME WHERE timestamp > ( %ld );", (long int)ts);
   if(i == -1){
      fprintf(stdout, "There is an error occured on asprintf\n");
      return -1;
   }

   /* Execute SQL statement */
   rc = sqlite3_exec(conn, sql, f, (void*)data, &zErrMsg);
   if( rc != SQLITE_OK ){
      fprintf(stderr, "SQL error: %s\n", zErrMsg);
      sqlite3_free(zErrMsg);
      return -1;
   }else{
      fprintf(stdout, "Operation done successfully\n");
   }
   free(sql);
   return 0;
}