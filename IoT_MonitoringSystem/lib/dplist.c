#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "dplist.h"

#ifdef DEBUG
	#define DEBUG_PRINTF(...) 									\
		do {											\
			printf("\nIn %s - function %s at line %d: ", __FILE__, __func__, __LINE__);	\
			printf(__VA_ARGS__);								\
		} while(0)
#else
	#define DEBUG_PRINTF(...) (void)0
#endif


#define DPLIST_ERR_HANDLER(condition,dplist_errno_value,...)\
	do {						\
		if ((condition))			\
		{					\
		  dplist_errno = dplist_errno_value;	\
		  DEBUG_PRINTF(#condition "failed");	\
		  return __VA_ARGS__;			\
		}					\
		dplist_errno = DPLIST_NO_ERROR;			\
	} while(0)
	
/*
 * The real definition of struct list / struct node
 */

struct dplist_node {
  dplist_node_t *prev, *next;
  void * element;
};

struct dplist {
  dplist_node_t *head;
  int size;
  void * (*element_copy)(void * src_element);			  
  void (*element_free)(void ** element);
  int (*element_compare)(void * x, void * y);
};
	
dplist_t *dpl_create (// callback functions
			  void * (*element_copy)(void * src_element),
			  void (*element_free)(void ** element),
			  int (*element_compare)(void * x, void * y)
			  )
{
  dplist_t *list;
  list = malloc(sizeof(struct dplist));
  DPLIST_ERR_HANDLER((list==NULL),DPLIST_MEMORY_ERROR,NULL);
  list->head = NULL;  
  list->size = 0;
  list->element_copy = element_copy;
  list->element_free = element_free;
  list->element_compare = element_compare;  
  return list;
}

void dpl_free(dplist_t ** list)
{
  DPLIST_ERR_HANDLER((list==NULL || (*list)==NULL),DPLIST_INVALID_ERROR,(void)NULL);
  if((*list)->head == NULL){
    free(*list);
    *list = NULL;
  }
  else if(dpl_size(*list) == 1){
    (*list)->element_free(&((*list)->head->element));
    free((*list)->head);
    free(*list);
    *list = NULL;
  }
  else{
    while((*list)->head->next != NULL){
      dplist_node_t * ref_at_index = (*list)->head;
      (*list)->head = (*list)->head->next;
      (*list)->head->prev = NULL;
      (*list)->element_free(&(ref_at_index->element));
      free(ref_at_index);
    }
    (*list)->element_free(&((*list)->head->element));
    free((*list)->head);
    free(*list); 
    *list = NULL;
  }
}

dplist_t * dpl_insert_at_index(dplist_t * list, void * element, int index, bool insert_copy)
{
  dplist_node_t * ref_at_index, * list_node;
  DPLIST_ERR_HANDLER((list==NULL),DPLIST_INVALID_ERROR,list);
  list_node = malloc(sizeof(dplist_node_t));
  DPLIST_ERR_HANDLER((list_node==NULL),DPLIST_MEMORY_ERROR,NULL);
  
  if(insert_copy == true){
    list_node->element = list->element_copy(element);
  }
  else{
    list_node->element = element;
  }
  
  if (list->head == NULL)  
  { // covers case 1 
    list_node->prev = NULL;
    list_node->next = NULL;
    list->head = list_node;
    list->size++;
  } else if (index <= 0)  
  { // covers case 2  
    list_node->prev = NULL;
    list_node->next = list->head;
    list->head->prev = list_node;
    list->head = list_node;
    list->size++;
  } else 
  {
    ref_at_index = dpl_get_reference_at_index(list, index);  
    assert( ref_at_index != NULL);
    if (index < dpl_size(list))
    { // covers case 4
      list_node->prev = ref_at_index->prev;
      list_node->next = ref_at_index;
      ref_at_index->prev->next = list_node;
      ref_at_index->prev = list_node;
      list->size++;
    } else
    { // covers case 3 
      assert(ref_at_index->next == NULL);
      list_node->next = NULL;
      list_node->prev = ref_at_index;
      ref_at_index->next = list_node;
      list->size++;
    }
  }
  return list;
}

dplist_t * dpl_remove_at_index( dplist_t * list, int index, bool free_element)
{
  dplist_node_t * ref_at_index;
  DPLIST_ERR_HANDLER((list==NULL),DPLIST_INVALID_ERROR,list);
  if(list->size == 0){
    return list;
  }
  if(dpl_size(list) == 1){
    if(free_element == true){
      list->element_free(&(list->head->element));
    }

    free(list->head);
    list->head = NULL;
    list->size--;
  }
  else if(index <= 0){
      ref_at_index = list->head;
      list->head = ref_at_index->next;
      list->head->prev = NULL;
      if(free_element == true){
        list->element_free(&(ref_at_index->element));
      }

      free(ref_at_index);
      list->size--;
  }
  else{
     ref_at_index = dpl_get_reference_at_index(list, index); 
    if(ref_at_index->next != NULL){
      ref_at_index->prev->next = ref_at_index->next; 
      ref_at_index->next->prev = ref_at_index->prev;
      if(free_element == true){
        list->element_free(&(ref_at_index->element));
      }

      free(ref_at_index);
      list->size--;
    }
    else{
      ref_at_index->prev->next = NULL;
      if(free_element == true){
        list->element_free(&(ref_at_index->element));
      }

      free(ref_at_index);
      list->size--;
    }
  }
  return list;
}

int dpl_size( dplist_t * list )
{
  DPLIST_ERR_HANDLER((list==NULL),DPLIST_INVALID_ERROR,0);
  return list->size;
}

dplist_node_t * dpl_get_reference_at_index( dplist_t * list, int index )
{
  int count;
  dplist_node_t * dummy;
  DPLIST_ERR_HANDLER((list==NULL),DPLIST_INVALID_ERROR,NULL);
  if (list->head == NULL ) return NULL;
  for ( dummy = list->head, count = 0; dummy->next != NULL ; dummy = dummy->next, count++) 
  { 
    if (count >= index) return dummy;
  }  
  return dummy; 
}

void * dpl_get_element_at_index( dplist_t * list, int index )
{
  DPLIST_ERR_HANDLER((list==NULL),DPLIST_INVALID_ERROR,NULL);
  if(list->head == NULL) return 0;   //why couldn't return NULL
  else{
    dplist_node_t * ref_at_index;
    ref_at_index = dpl_get_reference_at_index(list, index); 
    assert( ref_at_index != NULL);
    return ref_at_index->element;
  }
}

int dpl_get_index_of_element( dplist_t * list, void * element )
{
  int count;
  dplist_node_t * dummy;
  DPLIST_ERR_HANDLER((list==NULL),DPLIST_INVALID_ERROR,0);
  if (list->head == NULL ) return 0;  //why couldn't return NULL
  for ( dummy = list->head, count = 0; dummy->next != NULL ; dummy = dummy->next, count++) 
  { 
    if ( list->element_compare(dummy->element, element) == 0) return count;
  }  
  if(list->element_compare(dummy->element, element) == 0) return count;
  return -1; 
}

// HERE STARTS THE EXTRA SET OF OPERATORS //

// ---- list navigation operators ----//
  
dplist_node_t * dpl_get_first_reference( dplist_t * list )
{
  DPLIST_ERR_HANDLER((list==NULL),DPLIST_INVALID_ERROR,NULL);
  if (list->head == NULL ) return NULL;
  return list->head;
}

dplist_node_t * dpl_get_last_reference( dplist_t * list )
{
  dplist_node_t * dummy;
  DPLIST_ERR_HANDLER((list==NULL),DPLIST_INVALID_ERROR,NULL);
  if (list->head == NULL ) return NULL;
  dummy = dpl_get_reference_at_index( list, list->size );
  return dummy;
}

dplist_node_t * dpl_get_next_reference( dplist_t * list, dplist_node_t * reference )
{
  DPLIST_ERR_HANDLER((list==NULL),DPLIST_INVALID_ERROR,NULL);
  if (list->head == NULL || reference == NULL) return NULL;
  int i = dpl_get_index_of_reference( list, reference );
  if(i == -1) return NULL;
  else if (reference->next == NULL){
    return NULL;
  }
  else{
  return reference->next;  // the last one doesn't compare with reference, but the result always return NULL
  }
}

dplist_node_t * dpl_get_previous_reference( dplist_t * list, dplist_node_t * reference )
{
  DPLIST_ERR_HANDLER((list==NULL),DPLIST_INVALID_ERROR,NULL);
  if (list->head == NULL ) return NULL;
  if (reference == NULL){
    dplist_node_t * dummy = dpl_get_reference_at_index( list, list->size );
    return dummy;
  }
  int i = dpl_get_index_of_reference( list, reference );
  if(i == -1) return NULL;
  else if(reference->prev == NULL){
    return NULL;
  }
  else{
    return reference->prev;
  }
}

// ---- search & find operators ----//  
  
void * dpl_get_element_at_reference( dplist_t * list, dplist_node_t * reference )
{
  DPLIST_ERR_HANDLER((list==NULL),DPLIST_INVALID_ERROR,(void *)NULL);
  if (list->head == NULL ) return NULL;
  if (reference == NULL){
    dplist_node_t * dummy = dpl_get_reference_at_index( list, list->size );
    return dummy->element;
  }
  int i = dpl_get_index_of_reference( list, reference );
  if(i == -1) return NULL;
  return reference->element;
}

dplist_node_t * dpl_get_reference_of_element( dplist_t * list, void * element )
{
  dplist_node_t * dummy;
  DPLIST_ERR_HANDLER((list==NULL),DPLIST_INVALID_ERROR,NULL);
  if (list->head == NULL ) return NULL;  
  for ( dummy = list->head; dummy->next != NULL ; dummy = dummy->next) 
  { 
    if ( list->element_compare(dummy->element, element) == 0) return dummy;
  }  
  if(list->element_compare(dummy->element, element) == 0) return dummy;
  return NULL; 
}

int dpl_get_index_of_reference( dplist_t * list, dplist_node_t * reference )
{ 
  dplist_node_t * dummy;
  int count;
  DPLIST_ERR_HANDLER((list==NULL),DPLIST_INVALID_ERROR,0);
  if (list->head == NULL ) return -1;
  if (reference == NULL){
    return list->size - 1;
  }
  for (dummy = list->head, count = 0; dummy->next != NULL ; dummy = dummy->next, count++)
  {
    if (dummy == reference) return count;
  }
  if (dummy == reference) return count;
  return -1;
}
  
// ---- extra insert & remove operators ----//

dplist_t * dpl_insert_at_reference( dplist_t * list, void * element, dplist_node_t * reference, bool insert_copy )
{
  dplist_node_t * ref_at_index, * list_node;
  DPLIST_ERR_HANDLER((list==NULL),DPLIST_INVALID_ERROR,NULL);
  list_node = malloc(sizeof(dplist_node_t));
  DPLIST_ERR_HANDLER((list_node==NULL),DPLIST_MEMORY_ERROR,NULL);
  
  if(insert_copy == true){
    list_node->element = list->element_copy(element);
  }
  else{
    list_node->element = element;
  }
  
  if (list->head == NULL){
     list->head = list_node;
     list_node->prev = NULL;
     list_node->next = NULL;
     list->size++;
     return list;
  }
  else{
    int i = dpl_get_index_of_reference( list, reference );
    if(i == -1) return list;
    else if (reference == NULL){
      ref_at_index = dpl_get_reference_at_index(list, list->size);
      assert( ref_at_index->next == NULL);
      
      list_node->next = NULL;
      list_node->prev = ref_at_index;
      ref_at_index->next = list_node;
      list->size++;
    }
    else if (i == 0){
      assert(reference->prev == NULL);
      list_node->prev = NULL;
      list_node->next = reference;
      list->head = list_node;
      reference->prev = list->head;
      list->size++;
    }
    else{
      assert(reference->prev != NULL);
      list_node->prev = reference->prev;
      list_node->next = reference;
      reference->prev->next = list_node;
      reference->prev = list_node;
      list->size++;
    }
  }
  return list;
}

dplist_t * dpl_insert_sorted( dplist_t * list, void * element, bool insert_copy )
{
  dplist_node_t * ref_at_index, * list_node;
  DPLIST_ERR_HANDLER((list==NULL),DPLIST_INVALID_ERROR,NULL);
  list_node = malloc(sizeof(dplist_node_t));
  DPLIST_ERR_HANDLER((list_node==NULL),DPLIST_MEMORY_ERROR,NULL);
  
  if(insert_copy == true){
    list_node->element = list->element_copy(element);
  }
  else{
    list_node->element = element;
  }
  
  if (list->head == NULL){
    list->head = list_node;
    list_node->prev = NULL;
    list_node->next = NULL;
    list->size++;
    return list;
  }
  int i = list->element_compare(list->head->element, element);
  if (i == 1 || i == 0){
    list_node->prev = NULL;
    list_node->next = list->head;
    list->head = list_node;
    list_node->next->prev = list_node;
    list->size++;
    return list;
  }
  else if ( list->size == 1 && i == -1){
    list_node->next = NULL;
    list_node->prev = list->head;
    list->head->next = list_node;
    list->size++;
    return list;
  }
  else{
    for (ref_at_index = list->head->next; ref_at_index->next != NULL; ref_at_index = ref_at_index->next){
      int j = list->element_compare(ref_at_index->element, element);
      if (j == 0 || j == 1){
	list_node->prev = ref_at_index->prev;
	list_node->next = ref_at_index;
	ref_at_index->prev->next = list_node;
	ref_at_index->prev = list_node;
	list->size++;
	return list;
      }
    }
    int k = list->element_compare(ref_at_index->element, element);
    if (k == 0 || k == 1){
      list_node->prev = ref_at_index->prev;
      list_node->next = ref_at_index;
      ref_at_index->prev->next = list_node;
      ref_at_index->prev = list_node;
      list->size++;
      return list;
    }
    else{
      assert(k == -1 && ref_at_index->next == NULL);
      list_node->next = NULL;
      list_node->prev = ref_at_index;
      ref_at_index->next = list_node;
      list->size++;
      return list;
    }
  }
}

dplist_t * dpl_remove_at_reference( dplist_t * list, dplist_node_t * reference, bool free_element )
{
  DPLIST_ERR_HANDLER((list==NULL),DPLIST_INVALID_ERROR,NULL);
  if (list->head == NULL ) return list;
  if (reference == NULL){
    dplist_node_t * dummy;
    dummy = dpl_get_reference_at_index( list, list->size );
    if(dummy->prev == NULL){
      if(free_element == true){
      list->element_free(&(dummy->element));
         }
      free(list->head);
      list->head = NULL;
      list->size--;
      }
    else{  
      
      dummy->prev->next = NULL;
      if(free_element == true){
	list->element_free(&(dummy->element));
      }
      free(dummy);
      dummy = NULL;
      list->size--;
      
    }
  }
  
  else{
    int i = dpl_get_index_of_reference( list, reference );
    if(i == -1) return list;
    else if (list->size == 1){
      assert( reference->next == NULL);
      if(free_element == true){
	list->element_free(&(list->head->element));
      }
      free(list->head);
      list->head = NULL;
      list->size--;
    }
    else if(i == 0){
	assert(reference->prev == NULL);
	list->head = reference->next;
	list->head->prev = NULL;
	if(free_element == true){
	  list->element_free(&(reference->element));
	}
	free(reference);
	reference = NULL;
	list->size--;
    }
    else if( i > 0 && i < list->size - 1 ){
	assert(reference->next != NULL); 
	reference->prev->next = reference->next; 
	reference->next->prev = reference->prev;
	if(free_element == true){
	  list->element_free(&(reference->element));
	}
	free(reference);
	reference = NULL;
	list->size--;
      }
    else{
	assert(reference->next == NULL);
	reference->prev->next = NULL;
	if(free_element == true){
	  list->element_free(&(reference->element));
	}
	free(reference);
	list->size--;
      }
  }
  return list;
}

dplist_t * dpl_remove_element( dplist_t * list, void * element, bool free_element )
{
  dplist_node_t * ref_at_element;
  DPLIST_ERR_HANDLER((list==NULL),DPLIST_INVALID_ERROR,NULL);
  if (list->head == NULL ) return NULL;
  ref_at_element = dpl_get_reference_of_element(  list, element );
  if ( ref_at_element == NULL) return list;
  
  else if(dpl_size(list) == 1){
    assert(ref_at_element->next == NULL && ref_at_element->prev == NULL);
    if(free_element == true){
      list->element_free(&(list->head->element));
    }
    free(list->head);
    list->head = NULL;
    list->size--;
  }
  else if(ref_at_element->prev == NULL){
      assert(ref_at_element->next != NULL);
      list->head = ref_at_element->next;
      list->head->prev = NULL;
      if(free_element == true){
        list->element_free(&(ref_at_element->element));
      }
      free(ref_at_element);
      list->size--;
  }
  else if( ref_at_element->next != NULL ){
      ref_at_element->prev->next = ref_at_element->next; 
      ref_at_element->next->prev = ref_at_element->prev;
      if(free_element == true){
        list->element_free(&(ref_at_element->element));
      }
      free(ref_at_element);
      list->size--;
    }
  else{
      assert(ref_at_element->next == NULL);
      ref_at_element->prev->next = NULL;
      if(free_element == true){
        list->element_free(&(ref_at_element->element));
      }
      free(ref_at_element);
      list->size--;
    }
  return list;
}
  
// ---- you can add your extra operators here ----//



