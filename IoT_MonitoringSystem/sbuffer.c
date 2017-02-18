#define _GNU_SOURCE 
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <assert.h>
#include <time.h>
#include "sbuffer.h"

typedef struct sbuffer_node
{
  struct sbuffer_node * next;
  sbuffer_data_t * data;
} sbuffer_node_t;

struct sbuffer 
{
  sbuffer_node_t * head;
  sbuffer_node_t * tail;
  int buffer_size;
  pthread_mutex_t lock;
};

void pthread_err_handler( int err_code, char *msg, char *file_name, char line_nr )
{
	if ( 0 != err_code )
	{
		fprintf( stderr, "\n%s failed with error code %d in file %s at line %d\n", msg, err_code, file_name, line_nr );
	}
}

int sbuffer_init(sbuffer_t ** buffer)
{
  int presult;
  *buffer = malloc(sizeof(sbuffer_t));
  if (*buffer == NULL) return SBUFFER_FAILURE;
  (*buffer)->head = NULL;
  (*buffer)->tail = NULL;
  (*buffer)->buffer_size = 0;
  presult = pthread_mutex_init(&((*buffer)->lock), NULL);
  pthread_err_handler( presult, "pthread_mutex_init", __FILE__, __LINE__ );
  return SBUFFER_SUCCESS; 
}


int sbuffer_free(sbuffer_t ** buffer)
{
  int presult;
  presult = pthread_mutex_destroy( &((*buffer)->lock) );
  pthread_err_handler( presult, "pthread_mutex_destroy", __FILE__, __LINE__ );
  
  if ((buffer==NULL) || (*buffer==NULL)) 
  {
    return SBUFFER_FAILURE;
  } 
  while ( (*buffer)->head )
  {
    sbuffer_node_t * dummy = (*buffer)->head;
    (*buffer)->head = (*buffer)->head->next;
    free(dummy->data);
    free(dummy);
  }
  assert((*buffer)->buffer_size == 0);

  free(*buffer);
  *buffer = NULL;
  
  return SBUFFER_SUCCESS;		
}

/*
 * Removes the first data in 'buffer' (at the 'head') and returns this data as '*data'  
 * 'data' must point to allocated memory because this functions doesn't allocated memory
 * If 'buffer' is empty, the function doesn't block until new data becomes available but returns SBUFFER_NO_DATA
 * Returns SBUFFER_SUCCESS on success and SBUFFER_FAILURE if an error occured
 */
int sbuffer_remove(sbuffer_t * buffer,sbuffer_data_t * data)
{
  int presult;
  sbuffer_node_t * dummy;
  
  presult = pthread_mutex_lock( &(buffer->lock) );
  pthread_err_handler( presult, "pthread_mutex_lock", __FILE__, __LINE__ );
  
  if (buffer == NULL) return SBUFFER_FAILURE;
  if (buffer->head == NULL) return SBUFFER_NO_DATA;
  *data = *(buffer->head->data);
  dummy = buffer->head;
  if (buffer->head == buffer->tail) // buffer has only one node
  {
    buffer->head = buffer->tail = NULL; 
    buffer->buffer_size--;
  }
  else  // buffer has many nodes empty
  {
    buffer->head = buffer->head->next;
    buffer->buffer_size--;
  }
  free(dummy->data);
  free(dummy);
  
  presult = pthread_mutex_unlock( &(buffer->lock) );
  pthread_err_handler( presult, "pthread_mutex_unlock", __FILE__, __LINE__ );
  
  return SBUFFER_SUCCESS;
}


int sbuffer_remove_block(sbuffer_t * buffer,sbuffer_data_t * data, int timeout){
  int presult;
  sbuffer_node_t * dummy;
  time_t block_time;
  time_t current_time;
  
  if (buffer == NULL) return SBUFFER_FAILURE;
  
  if (buffer->head == NULL){
    int loop = 1;
    time(&block_time);
    while(loop){
      usleep(1000000);
      time(&current_time);
      if(sbuffer_size(buffer) != 0 )break;
      if(buffer->buffer_size ==0 && difftime(current_time, block_time) >= (double)timeout)return SBUFFER_NO_DATA;
    }
  }
  
  presult = pthread_mutex_lock( &(buffer->lock) );
  pthread_err_handler( presult, "pthread_mutex_lock", __FILE__, __LINE__ );
  
  *data = *(buffer->head->data);
  dummy = buffer->head;
  if (buffer->head == buffer->tail) // buffer has only one node
  {
    buffer->head = buffer->tail = NULL; 
    buffer->buffer_size--;
  }
  else  // buffer has many nodes empty
  {
    buffer->head = buffer->head->next;
    buffer->buffer_size--;
  }
  free(dummy->data);
  free(dummy);
  
  presult = pthread_mutex_unlock( &(buffer->lock) );
  pthread_err_handler( presult, "pthread_mutex_unlock", __FILE__, __LINE__ );
  
  return SBUFFER_SUCCESS;
}


/* Inserts the data in 'data' at the end of 'buffer' (at the 'tail')
 * Returns SBUFFER_SUCCESS on success and SBUFFER_FAILURE if an error occured
*/
int sbuffer_insert(sbuffer_t * buffer, sbuffer_data_t * data)
{
  int presult;
  sbuffer_node_t * dummy;
  
  presult = pthread_mutex_lock( &(buffer->lock) );
  pthread_err_handler( presult, "pthread_mutex_lock", __FILE__, __LINE__ );
  
  if (buffer == NULL) return SBUFFER_FAILURE;
  dummy = malloc(sizeof(sbuffer_node_t));
  if (dummy == NULL) return SBUFFER_FAILURE;
  dummy->data = malloc(sizeof(sbuffer_data_t));
  if (dummy->data == NULL)return SBUFFER_FAILURE;
  *(dummy->data) = *data;
  dummy->next = NULL;
  
  if (buffer->tail == NULL) // buffer empty (buffer->head should also be NULL
  {
    buffer->head = buffer->tail = dummy;
    buffer->buffer_size++;
  } 
  else // buffer not empty
  {
    buffer->tail->next = dummy;
    buffer->tail = buffer->tail->next; 
    buffer->buffer_size++;
  }
  
  presult = pthread_mutex_unlock( &(buffer->lock) );
  pthread_err_handler( presult, "pthread_mutex_unlock", __FILE__, __LINE__ );
  
  return SBUFFER_SUCCESS;
}

int sbuffer_size(sbuffer_t * buffer){

  return buffer->buffer_size;
}

sbuffer_data_t * sbuffer_get_element_at_index(sbuffer_t * buffer, int index){
  int count;
  sbuffer_node_t * dummy;
  if ((buffer==NULL) || (buffer->head==NULL)) 
  {
    return NULL;
  }
  for ( dummy = buffer->head, count = 0; dummy->next != NULL ; dummy = dummy->next, count++) 
  { 
    if (count >= index) return dummy->data;
  }  
  return dummy->data; 
}