#ifndef _DPLIST_H_
#define _DPLIST_H_

#include <stdbool.h>  

extern int dplist_errno;

/*
 * definition of error codes
 * */
#define DPLIST_NO_ERROR 0
#define DPLIST_MEMORY_ERROR 1 // error due to mem alloc failure
//#define DPLIST_EMPTY_ERROR 2  //error due to an operation that can't be executed on an empty list
#define DPLIST_INVALID_ERROR 3 //error due to a list operation applied on a NULL list 

//typedef enum {false = 0, true = 1} bool; // or use C99 #include <stdbool.h>  

typedef struct dplist dplist_t; // slist_t is a struct containing at least a head pointer to the start of the list; later function pointers to element_copy, element_compare, element_free, etc. will be added

typedef struct dplist_node dplist_node_t;

dplist_t * dpl_create (// callback functions
			  void * (*element_copy)(void * element), 	// Duplicate 'element'; If needed allocated new memory for the duplicated element.
			  void (*element_free)(void ** element),	// If needed, free memory allocated to element
			  int (*element_compare)(void * x, void * y)	// Compare two element elements; returns -1 if x<y, 0 if x==y, or 1 if x>y 
			);
// Returns a pointer to a newly-allocated and initialized list.
// Returns NULL if memory allocation failed and list_errno is set to SP_LIST_MEMORY_ERROR 

void dpl_free(dplist_t ** list);
// Every list node of the list needs to be deleted (free memory)
// Element_free() is called on every element in the list to free memory
// The list itself also needs to be deleted (free all memory)
// '*list' must be set to NULL.
// If 'list' or '*list' is NULL, list_errno is set to SP_LIST_INVALID_ERROR.

dplist_t * dpl_insert_at_index(dplist_t * list, void * element, int index, bool insert_copy);
// Inserts a new list node containing an 'element' in the list at position 'index'  and returns a pointer to the new list.
// If insert_copy == true : use element_copy() to make a copy of 'element' and use the copy in the new list node
// If insert_copy == false : insert 'element' in the new list node without taking a copy of 'element' with element_copy() 
// Remark: the first list node has index 0.
// If 'index' is 0 or negative, the list node is inserted at the start of 'list'. 
// If 'index' is bigger than the number of elements in the list, the list node is inserted at the end of thelist.
// Return NULL if 'list' is NULL and list_errno is set to SP_LIST_INVALID_ERROR 
// Return NULL if memory allocation failed and list_errno is set to SP_LIST_MEMORY_ERROR 

dplist_t * dpl_remove_at_index( dplist_t * list, int index, bool free_element);
// Removes the list node at index 'index' from the list. 
// If free_element == true : call element_free() on the element of the list node to remove
// If free_element == false : don't call element_free() on the element of the list node to remove
// The list node itself should always be freed
// If 'index' is 0 or negative, the first list node is removed. 
// If 'index' is bigger than the number of elements in the list, the last list node is removed.
// If the list is empty, return the unmodifed list 
// Return NULL if 'list' is NULL and list_errno is set to SP_LIST_INVALID_ERROR

int dpl_size( dplist_t * list );
// Returns the number of elements in the list.
// Return 0 if 'list' is NULL and list_errno is set to SP_LIST_INVALID_ERROR

dplist_node_t * dpl_get_reference_at_index( dplist_t * list, int index );
// Returns a reference to the list node with index 'index' in the list. 
// If 'index' is 0 or negative, a reference to the first list node is returned. 
// If 'index' is bigger than the number of list nodes in the list, a reference to the last list node is returned. 
// If the list is empty, NULL is returned.
// Return NULL if 'list' is NULL and list_errno is set to SP_LIST_INVALID_ERROR

void * dpl_get_element_at_index( dplist_t * list, int index );
// Returns the list element contained in the list node with index 'index' in the list.
// Remark: return is not returning a copy of the element with index 'index', i.e. 'element_copy()' is not used. 
// If 'index' is 0 or negative, the element of the first list node is returned. 
// If 'index' is bigger than the number of elements in the list, the element of the last list node is returned.
// If the list is empty, (void *)NULL is returned.
// Return NULL if 'list' is NULL and list_errno is set to SP_LIST_INVALID_ERROR

int dpl_get_index_of_element( dplist_t * list, void * element );
// Returns an index to the first list node in the list containing 'element'.
// Use 'element_compare()' to search 'element' in the list
// A match is found when 'element_compare()' returns 0
// If 'element' is not found in the list, -1 is returned.
// Return NULL if 'list' is NULL and list_errno is set to SP_LIST_INVALID_ERROR



// ---- list navigation operators ----//
  
dplist_node_t * dpl_get_first_reference( dplist_t * list );
// Returns a reference to the first list node of the list. 
// If the list is empty, NULL is returned.
// Return NULL if 'list' is NULL and list_errno is set to SP_LIST_INVALID_ERROR

dplist_node_t * dpl_get_last_reference( dplist_t * list );
// Returns a reference to the last list node of the list. 
// If the list is empty, NULL is returned.
// Return NULL if 'list' is NULL and list_errno is set to SP_LIST_INVALID_ERROR

dplist_node_t * dpl_get_next_reference( dplist_t * list, dplist_node_t * reference );
// Returns a reference to the next list node of the list node with reference 'reference' in the list. 
// If the list is empty, NULL is returned
// If 'reference' is NULL, NULL is returned.
// If 'reference' is not an existing reference in the list, NULL is returned.
// Return NULL if 'list' is NULL and list_errno is set to SP_LIST_INVALID_ERROR

dplist_node_t * dpl_get_previous_reference( dplist_t * list, dplist_node_t * reference );
// Returns a reference to the previous list node of the list node with reference 'reference' in 'list'. 
// If the list is empty, NULL is returned.
// If 'reference' is NULL, a reference to the last list node in the list is returned.
// If 'reference' is not an existing reference in the list, NULL is returned.
// Return NULL if 'list' is NULL and list_errno is set to SP_LIST_INVALID_ERROR  

// ---- search & find operators ----//  
  
void * dpl_get_element_at_reference( dplist_t * list, dplist_node_t * reference );
// Returns the element contained in the list node with reference 'reference' in the list. 
// If the list is empty, NULL is returned. 
// If 'reference' is NULL, the element of the last element is returned.
// If 'reference' is not an existing reference in the list, NULL is returned.
// Return NULL if 'list' is NULL and list_errno is set to SP_LIST_INVALID_ERROR

dplist_node_t * dpl_get_reference_of_element( dplist_t * list, void * element );
// Returns a reference to the first list node in the list containing 'element'. 
// If the list is empty, NULL is returned. 
// If 'element' is not found in the list, NULL is returned.
// Return NULL if 'list' is NULL and list_errno is set to SP_LIST_INVALID_ERROR

int dpl_get_index_of_reference( dplist_t * list, dplist_node_t * reference );
// Returns the index of the list node in the list with reference 'reference'. 
// If the list is empty, -1 is returned. 
// If 'reference' is NULL, the index of the last element is returned.
// If 'reference' is not an existing reference in the list, -1 is returned.
// Return NULL if 'list' is NULL and list_errno is set to SP_LIST_INVALID_ERROR

  
// ---- extra insert & remove operators ----//

dplist_t * dpl_insert_at_reference( dplist_t * list, void * element, dplist_node_t * reference, bool insert_copy );
// Inserts a new list node containing an 'element' in the list at position 'reference'  and returns a pointer to the new list.
// If insert_copy == true : use element_copy() to make a copy of 'element' and use the copy in the new list node
// If insert_copy == false : insert 'element' in the new list node without taking a copy of 'element' with element_copy() 
// If 'reference' is NULL, the element is inserted at the end of 'list'.
// If 'reference' is not an existing reference in the list, 'list' is returned.
// Return NULL if 'list' is NULL and list_errno is set to SP_LIST_INVALID_ERROR 
// Return NULL if memory allocation failed and list_errno is set to SP_LIST_MEMORY_ERROR 

dplist_t * dpl_insert_sorted( dplist_t * list, void * element, bool insert_copy );
// Inserts a new list node containing 'element' in the sorted list and returns a pointer to the new list. 
// The list must be sorted before calling this function. 
// The sorting is done in ascending order according to a comparison function.  
// If two members compare as equal, their order in the sorted array is undefined.
// If insert_copy == true : use element_copy() to make a copy of 'element' and use the copy in the new list node
// If insert_copy == false : insert 'element' in the new list node without taking a copy of 'element' with element_copy() 
// Return NULL if 'list' is NULL and list_errno is set to SP_LIST_INVALID_ERROR

dplist_t * dpl_remove_at_reference( dplist_t * list, dplist_node_t * reference, bool free_element );
// Removes the list node with reference 'reference' in the list. 
// If free_element == true : call element_free() on the element of the list node to remove
// If free_element == false : don't call element_free() on the element of the list node to remove
// The list node itself should always be freed
// If 'reference' is NULL, the last list node is removed.
// If 'reference' is not an existing reference in the list, 'list' is returned.
// If the list is empty, return list and list_errno is set to LIST_EMPTY_ERROR
// Return NULL if 'list' is NULL and list_errno is set to SP_LIST_INVALID_ERROR

dplist_t * dpl_remove_element( dplist_t * list, void * element, bool free_element );
// Finds the first list node in the list that contains 'element' and removes the list node from 'list'. 
// If free_element == true : call element_free() on the element of the list node to remove
// If free_element == false : don't call element_free() on the element of the list node to remove
// If 'element' is not found in 'list', the unmodified 'list' is returned.
// Return NULL if 'list' is NULL and list_errno is set to SP_LIST_INVALID_ERROR   
  
// ---- you can add your extra operators here ----//


#endif  // _DPLIST_H_

