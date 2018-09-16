#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <malloc.h>
#define QueueLength 10				//define queuelength

typedef struct SharedQueue			//define dataqueue struct
{
    int first;
    int last;
    int valid_items;
    struct message* buffer[QueueLength];
} dataqueue;

void sq_create(dataqueue* queuename)		//create functions creates dataqueue variable and initializes every element in the buffer to 0
{
    int i;
    queuename->first  =  0;
    queuename->last   =  0;
    queuename->valid_items  =  0;
    for(i=0; i<QueueLength; i++)
    {
        queuename->buffer[i] = 0;
    }
    return;
}

int isEmpty(dataqueue* queuename)		//this function checks whether the queue is empty or not
{
    if(queuename->valid_items==0)
        return(1);
    else
        return(0);
}

int sq_write(dataqueue* queuename,void* ptr)	//function to write the pointer to the last element in the dataqueue specified
{
    	
    if(queuename->valid_items>=QueueLength)
    {
        return -1;
    }
    else
    {        
        queuename->valid_items++;
        queuename->buffer[queuename->last] = ptr;
        queuename->last = (queuename->last+1)%QueueLength;
        return 0;
    }  
    
}

int sq_read(dataqueue* queuename,int no2, void* rptr[])	//function to read the first element from the dataqueue specified and store it in array rptr[no2]
{   
   
    if(isEmpty(queuename))
    {
        return(-1);
    }
    else
    {   
        rptr[no2] = queuename->buffer[queuename->first];
        queuename->first=(queuename->first+1)%QueueLength;
        queuename->valid_items--;
        return(0);	
    }
    
}

//The following function works, but is not needed here. If you include this function, then include stdio.h file at the top.

void printQueue(dataqueue *queuename)		//function to print the queue to aid in debugging the code
{   
    int aux, aux1;
    aux  = queuename->first;
    aux1 = queuename->valid_items;
    if(aux1 == 0)
    {
    printf("Queue is Empty.");
    }
    while(aux1>0)
    {
        printf("Element #%d = %p\n", aux, queuename->buffer[aux]);
        aux=(aux+1)%QueueLength;
        aux1--;
    }
    printf("\n");
    return;
}

void sq_delete(dataqueue* queuename)		//this function is used to free the memory. Since the memory for dataqueue is not allocated using malloc, it will automatically get freed when the program 							stops execution, hence the function is not a necessity here
{  
  //free(queuename->buffer);
  //printf("dataqueue will be free after program termination.\n");
  return;
}

