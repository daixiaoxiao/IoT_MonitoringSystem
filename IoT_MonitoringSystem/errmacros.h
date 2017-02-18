#ifndef __errmacros_h__
#define __errmacros_h__

#include <errno.h>

#ifdef DEBUG
#define DEBUG_PRINT(...) 							                     \
		do {					  					                     \
		  printf("In %s - function %s at line %d: ", __FILE__, __func__, __LINE__);	\
		  printf(__VA_ARGS__);								\
		} while(0)
#else
  #define DEBUG_PRINT(...) (void)0
#endif

#define ERROR_HANDLER(err) 									\
		  do {												\
			 if ( (err) != 0 )								\
				  {												\
					  printf("In %s - function %s at line %d: ", __FILE__, __func__, __LINE__);	\
					  perror("Error happens");		\
					  exit( EXIT_FAILURE );					\
				  }												\
		  } while(0)
		  
#define SBUFFER_ERROR(err) 									\
		  do {												\
			 if ( (err) != 0 )								\
				  {												\
					  printf("In %s - function %s at line %d: ", __FILE__, __func__, __LINE__);	\
					  printf("Sbuffer operation error");		\
					  exit( EXIT_FAILURE );					\
				  }												\
		  } while(0)
		  

#define SYSCALL_ERROR(err) 									\
		do {												\
			if ( (err) == -1 )								\
			{												\
				perror("Error executing syscall");			\
				exit( EXIT_FAILURE );						\
			}												\
		} while(0)
		
#define CHECK_MKFIFO(err) 									\
		do {												\
			if ( (err) == -1 )								\
			{												\
				if ( errno != EEXIST )						\
				{											\
					perror("Error executing mkfifo");		\
					exit( EXIT_FAILURE );					\
				}											\
			}												\
		} while(0)
		
#define FILE_OPEN_ERROR(fp) 								\
		do {												\
			if ( (fp) == NULL )								\
			{												\
				perror("File open failed");					\
				exit( EXIT_FAILURE );						\
			}												\
		} while(0)

#define FILE_CLOSE_ERROR(err) 								\
		do {												\
			if ( (err) == -1 )								\
			{												\
				perror("File close failed");				\
				exit( EXIT_FAILURE );						\
			}												\
		} while(0)
		
#define FILE_SCANF_ERROR(err) 								\
		do {												\
			if ( (err) == -1 )								\
			{												\
				perror("File scanf failed");				\
				exit( EXIT_FAILURE );						\
			}												\
		} while(0)
		
#define FILE_PUTS_ERROR(err) 								\
		do {												\
			if ( (err) == -1 )								\
			{												\
				perror("File fputs failed");				\
				exit( EXIT_FAILURE );						\
			}												\
 		} while(0)

#define ASPRINTF_ERROR(err) 								\
		do {												\
			if ( (err) == -1 )								\
			{												\
				perror("asprintf failed");					\
				exit( EXIT_FAILURE );						\
			}												\
		} while(0)

#define FFLUSH_ERROR(err) 								\
		do {												\
			if ( (err) == EOF )								\
			{												\
				perror("fflush failed");					\
				exit( EXIT_FAILURE );						\
			}												\
		} while(0)

#endif
