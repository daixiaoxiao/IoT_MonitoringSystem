#define _GNU_SOURCE 
/*-----------------------------------------------------------------------------
		include files
------------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <semaphore.h>
#include <time.h>

#include "lib/dplist.h"
#include "datamgr.h"
#include "errmacros.h"
#include "connmgr.h"
#include "sbuffer.h"

/*------------------------------------------------------------------------------
		global variable declarations
------------------------------------------------------------------------------*/
extern sem_t                fifo_sem;
extern FILE *               fp;
extern sbuffer_t *        sec_buffer;
static   dplist_t *          sensor_avg_list = NULL;
int        dplist_errno;

extern void                  log_to_fifo( char * buf, sem_t sema, FILE * fp_t );
extern void                  sbuffer_print(sbuffer_t * ptr);
/*------------------------------------------------------------------------------
		definitions (defines, typedefs, ...)
------------------------------------------------------------------------------*/
typedef struct{
  sensor_id_t         sensor_id;
  sensor_id_t         room_id;
  sensor_value_t   running_avg;
  sensor_ts_t         timestamp;
  
  sensor_value_t    buf[RUN_AVG_LENGTH];
  int                       buf_size;
}sensor_node_t;

/*------------------------------------------------------------------------------
		function declarations
------------------------------------------------------------------------------*/
void      datamgr_dpl_print             (dplist_t * list);
void *   datamgr_element_copy      (void * element); 	// Duplicate 'element'; If needed allocated new memory for the duplicated element.
void      datamgr_element_free       (void ** element);	// If needed, free memory allocated to element
int        datamgr_element_compare(void * x, void * y);  // Compare two element elements; returns -1 if x<y, 0 if x==y, or 1 if x>y 

sensor_node_t * search_list             (sensor_id_t sensor_id);  //search sensor data in the list
void                   read_sensor_map   (FILE * fp_sensor_map); //read_sensor_map
void                   read_sensor_data   (sbuffer_t * sbuffer_ptr_t); //read_sensor_data
void                   log_message           (sensor_value_t temp, sensor_id_t room); // output the log_message
sensor_value_t   count_avg               (sensor_node_t * ptr_t);  //caculate the running_avg
void                   match_with_sensor_data(sensor_node_t * ptr, sbuffer_data_t * data_ptr);
/*------------------------------------------------------------------------------
		implementation code
------------------------------------------------------------------------------*/
void datamgr_dpl_print( dplist_t * list )
{
  int i, length;

  length = dpl_size(list);
  assert(dplist_errno == DPLIST_NO_ERROR);
  for ( i = 0; i < length; i++)    
  {
    void * element;
    element = dpl_get_element_at_index(list, i);
    assert(dplist_errno == DPLIST_NO_ERROR);
    printf("element at index %d = " "%" PRIu16 "\n", i, ((sensor_node_t *)element)->sensor_id);
  }
}
					
void datamgr_parse_sensor_data(FILE * fp_sensor_map, sbuffer_t ** buffer){
  
  read_sensor_map(fp_sensor_map);

  read_sensor_data(*buffer);
}

void read_sensor_map(FILE * fp_sensor_map){
  sensor_id_t room_ID;
  sensor_id_t sensor_ID;
  
  sensor_avg_list = dpl_create(&datamgr_element_copy, &datamgr_element_free, &datamgr_element_compare);
  assert(sensor_avg_list != NULL);
  
  sensor_node_t * sensor_ptr = malloc(sizeof(sensor_node_t));
  assert(sensor_ptr != NULL);
  
  while( !feof(fp_sensor_map) ){
    int j = fscanf(fp_sensor_map,  "%16hu %16hu", &room_ID, &sensor_ID);
    if(j == 2){
      DEBUG_PRINT("%hu %hu \n", room_ID, sensor_ID);
      sensor_ptr->room_id = room_ID;
      sensor_ptr->sensor_id = sensor_ID;
      sensor_ptr->running_avg = 0;
      sensor_ptr->timestamp = 0;
      
      sensor_ptr->buf_size = 0;
      
      sensor_avg_list = dpl_insert_at_index( sensor_avg_list, sensor_ptr, dpl_size(sensor_avg_list), true);
      assert(sensor_avg_list != NULL);
    }
  }
  free(sensor_ptr);
  fclose(fp_sensor_map);
}

void read_sensor_data(sbuffer_t * sbuffer_ptr_t){
  bool loop = true;

  sbuffer_data_t * data_ptr = malloc(sizeof(sbuffer_data_t));
  assert(data_ptr != NULL);
  
  while( loop ){
    int flag = sbuffer_remove_block(sbuffer_ptr_t, data_ptr, TIMEOUT);
    if(flag == SBUFFER_SUCCESS){
      //insert into second sbuffer
      if( sbuffer_insert( sec_buffer, data_ptr) == SBUFFER_FAILURE){
	printf("second sbuffer insertion failure!\n");
	exit(EXIT_FAILURE);
      }

      sensor_node_t * ptr = search_list(data_ptr->sensor_data.id);
      
      match_with_sensor_data( ptr, data_ptr);
      
      /* force the context switch*/ 
      usleep(500000);
    }
    else if (flag == SBUFFER_FAILURE)SBUFFER_ERROR(flag);
    else{
      break;
    }
  }
  
  free(data_ptr);
}

void match_with_sensor_data(sensor_node_t * ptr, sbuffer_data_t * data_ptr){
  char * send_buf;
  if(ptr == NULL){
    DEBUG_PRINT("invalid sensor node ID %" PRIu16 "\n", data_ptr->sensor_data.id);
    ASPRINTF_ERROR(asprintf( &send_buf, "Received sensor data with invalid sensor node ID %" PRIu16 "\n", data_ptr->sensor_data.id));
    log_to_fifo( send_buf, fifo_sem, fp );
  }
  else{
    //update the temperature running_avg and timestamp
    assert(ptr->sensor_id == data_ptr->sensor_data.id);
    if(ptr->buf_size < RUN_AVG_LENGTH){
      ptr->timestamp = data_ptr->sensor_data.ts;
      ptr->running_avg = 0;
      
      ptr->buf[ptr->buf_size] = data_ptr->sensor_data.value;
      ptr->buf_size++;
      if(ptr->buf_size == RUN_AVG_LENGTH){
	ptr->running_avg = count_avg( ptr );
	log_message( ptr->running_avg, ptr->sensor_id);
      }
    }
    else{
      //move every element one cell forward and call the datamgr_get_avg function
      ptr->timestamp = data_ptr->sensor_data.ts;
      int j = 0;
      while(j < RUN_AVG_LENGTH - 1){
	ptr->buf[j] = ptr->buf[j+1];
	j++;
      }
      ptr->buf[RUN_AVG_LENGTH - 1] = data_ptr->sensor_data.value;
      
      ptr->running_avg = count_avg( ptr );
      log_message(ptr->running_avg, ptr->sensor_id);
    }
  }
}

sensor_node_t * search_list(sensor_id_t sensor_id){
  int i;
  for(i = 0; i != dpl_size(sensor_avg_list); i++){
    sensor_node_t * ptr = (sensor_node_t *) dpl_get_element_at_index( sensor_avg_list, i );
    if(ptr->sensor_id == sensor_id)return ptr;
  }
  return NULL;
}

uint16_t datamgr_get_room_id(sensor_id_t sensor_id){
  int i;
  for(i = 0; i != dpl_size(sensor_avg_list); i++){
    sensor_node_t * ptr = (sensor_node_t *) dpl_get_element_at_index( sensor_avg_list, i );
    if(ptr->sensor_id == sensor_id)return ptr->room_id;
  }
  return -1;
}

void log_message(sensor_value_t temp, sensor_id_t sensor_id){
  char * send_buf;
  if(temp > SET_MAX_TEMP){
    ASPRINTF_ERROR(asprintf( &send_buf, "The sensor node with %" PRIu16 " reports it's too hot (running avg temperature = %g)\n", sensor_id, temp));
    log_to_fifo( send_buf, fifo_sem, fp );
  }
  if(temp < SET_MIN_TEMP){
    ASPRINTF_ERROR(asprintf( &send_buf, "The sensor node with %" PRIu16 " reports it's too cold (running avg temperature = %g)\n", sensor_id, temp));
    log_to_fifo( send_buf, fifo_sem, fp );
  }
}

sensor_value_t count_avg(sensor_node_t * ptr_t){
    int i;
    double sum = 0;  
    for(i = 0; i != RUN_AVG_LENGTH; i++){
      sum += ptr_t->buf[i];
    }
    return sum/RUN_AVG_LENGTH;
}

sensor_value_t datamgr_get_avg(sensor_id_t sensor_id){
   sensor_node_t * ptr = search_list(sensor_id);
   if( ptr == NULL )return -1;
   return ptr->running_avg;
}

time_t datamgr_get_last_modified(sensor_id_t sensor_id){
   sensor_node_t * ptr = search_list(sensor_id);
   if( ptr == NULL )return -1;
   return ptr->timestamp; 
}

int datamgr_get_total_sensors(){
  return dpl_size(sensor_avg_list);
}

void datamgr_free(){
  dpl_free(&sensor_avg_list);
}

void * datamgr_element_copy(void * element){ 	// Duplicate 'element'; If needed allocated new memory for the duplicated element.
  sensor_node_t *p;
  p = malloc( sizeof( sensor_node_t ));
  assert ( p != NULL );
  *p = *( sensor_node_t *)element;
  return (void *)p;
}

void datamgr_element_free(void ** element){	// If needed, free memory allocated to element
  free(*element);
  *element = NULL;
}

int datamgr_element_compare(void * x, void * y){ // Compare two element elements; returns -1 if x<y, 0 if x==y, or 1 if x>y 
  sensor_node_t * a = (sensor_node_t *)x;
  sensor_node_t * b = (sensor_node_t *)y;
  if (a->room_id < b->room_id){
    return -1;
  }
  else if (a->room_id == b->room_id  && a->sensor_id == b->sensor_id){
    return 0;
  }
  else{
    return 1;
  }
}