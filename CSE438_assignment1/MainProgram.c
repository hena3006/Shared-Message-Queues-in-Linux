#define _GNU_SOURCE				//include required library files
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>
#include "rdtsc.h"				//rdstc is a header file which helps read the time stamp counter
#include "SQ.h"					//include library header file
#include <math.h>
#include <sched.h>
#include <semaphore.h>
#define MOUSEFILE "/dev/input/event6"		//MOUSEFILE declared. Event number may change on different machines. Write cat /proc/bus/input/devices to see the event number of mouse/touchpad and edit
#define BASE_PERIOD 1000			//base period for periodic threads
#define CPU_FREQ 2700000000			//CPU_FREQ may change on different machines

sem_t m;					//semaphore used for aperiodic threads
pthread_mutex_t gptr_mutex = PTHREAD_MUTEX_INITIALIZER; //define mutex "gptr" which will control and help synchronize all threads 

struct message					//general structure for all messages
{
	int msgID;
    	int srcID;
    	unsigned long long queueTime;
    	unsigned long long dequeueTime;
    	unsigned long int QueueingTime;
    	double pi;
};

struct message* gptr;				// struct message* to store the pointer returned when struct message variable is declared
struct message* rptr[100000];			//received struct message pointers stored in this array

static int flag = 0;				//flag set equal to 1 when double click detected
static int GlobalMessageID = 1;			//unique increasing message id for every message created
static int GlobalMessageCounter = 0;		//message counter which is incremented by one whenever any message is dequeued
static int no2 = 0; 				//rptr[] array indice

struct PeriodicFunctionParameters		//arguments structure for periodic threads
{  
    	dataqueue* queuename;
    	int sourceID;
    	long sleeptime;
};

struct AperiodicFunctionParameters		//arguments structure for aperiodic threads
{  
    	dataqueue* queuename;
    	int sourceID;
};	

struct ReceiverFunctionParameters		//arguments structure for receiver thread
{
    	dataqueue* queuename1;
    	dataqueue* queuename2;
    	long sleeptime;
};

float calculatepi(void)				//calculates value of pi using a special algorithm
{
   	double number, k;       		// Number of iterations and control variable
   	double s = 1;      			//Signal for the next iteration
   	double pi = 0;
   	number = rand()%(50-10+1) + 10;      	//generates a random number between 10 and 40

   	for(k = 1; k <= (number * 2); k += 2)
	{
     		pi = pi + s * (4 / k);
     		s = -s;
   	}
   	return(pi);
}

void* PeriodicFunction(void* parameters)							//general function called by all periodic threads
{ 												//the next 5 lines force the threads to run on cpu0  
    	cpu_set_t cpuset;			
    	int cpu = 0;
    	CPU_ZERO(&cpuset);
    	CPU_SET( cpu, &cpuset);
    	sched_setaffinity(0, sizeof(cpuset), &cpuset);
  
    	struct PeriodicFunctionParameters* p = (struct PeriodicFunctionParameters*) parameters;	//copy parameters into local pointer p    
    	struct timespec deadline;								//stuct timespec variable 
    	clock_gettime(CLOCK_MONOTONIC, &deadline);						//gets current time and stores it in struct timespec variable "deadline"	
    	while(flag != 1)									//all periodic threads run until this flag is set to 0
    	{ 
      		pthread_mutex_lock (&gptr_mutex);						//mutex gptr locked
      		gptr = (struct message*)malloc(sizeof(struct message));				//dynamically allocate memory to struct message variable and store the pointer in "gptr"
      		double pi = calculatepi();							//calls calculatepi() function to compute pi
      		int write_return = sq_write(p->queuename, gptr);				//call sq_create to write the message pointer to dataqueue
        
      		if(write_return == 0)								//if sq_write is successfull, enter details into the strut message variable
      		{	
         		gptr->queueTime = rdtsc();						//rdtsc enters the current counter value into queueTime	
         		gptr->msgID = GlobalMessageID;            
         		gptr->srcID = p->sourceID;
         		gptr->pi = pi;    
         		GlobalMessageID++;         
         		pthread_mutex_unlock (&gptr_mutex); 					//writing done, free mutex gptr, so that other waiting threads can wrie to dataqueues
         
      		}
      		deadline.tv_nsec += p->sleeptime;						//this block calculates absolute time to wake up corresponding thread after sleep
      		if(deadline.tv_nsec >= 1000000000)
      		{
         		deadline.tv_nsec -= 1000000000;
         		deadline.tv_sec++;
      		}
      		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &deadline, 0);			//makes the thread sleep for period specified in arguments
    	}
    	return 0;
}

void* AperiodicFunction(void* parameters)							//general function called by aperiodic threads
{   												//the next 5 lines force the threads to run on cpu0
    	cpu_set_t cpuset;																
    	int cpu = 0;
    	CPU_ZERO(&cpuset);
    	CPU_SET( cpu, &cpuset);
    	sched_setaffinity(0, sizeof(cpuset), &cpuset);
   
    	sem_wait(&m);										//semaphore used to synchronize the left and right click events
    	struct AperiodicFunctionParameters* p = (struct AperiodicFunctionParameters*) parameters;
    	int fd;											//reading code for mouse/touchpad begins
    	struct input_event ie;
    	fd = open(MOUSEFILE, 0);
	
    	if(fd == -1)
    	{
		perror("opening device");
		exit(EXIT_FAILURE);
    	}

    	unsigned long long a[2];								//array of 2 elements to store time of current and previous left clicks	
    
    	while(flag != 1)									//aperiodic function reads mouse events until flag is set to 1
    	{        
		read(fd, &ie, sizeof(struct input_event));					//read mouse
		if((p->sourceID) == 5 && ie.code == 272 && ie.value == 1 )			//read code, value and sourceID. If souceID is 5, value and code match left click,enqueue message to 													dataqueue1.
		{	
			a[1] = rdtsc();								//store rdtsc in a[1]
			
		 	if((float)(a[1] - a[0])/2700000000 <= 0.3)				//check if difference between two mouse left clicks is left than 500ms
		 	{
				//printf("It was a double click.\n"); 
				flag =1;							//set flag to 1 when double click detected
				break;
			}
			
		 	else
		 	{
		  		//printf("left button pressed\n");
		  		pthread_mutex_lock (&gptr_mutex);				//lock mutex and enqueue message using the same method
               	  		gptr = (struct message*)malloc(sizeof(struct message));
       		  		double pi= calculatepi();
       		  		int write_return = sq_write(p->queuename, gptr);
        
                  		if(write_return == 0)
               	  		{	
        				gptr->queueTime = rdtsc();	
      	   				gptr->msgID = GlobalMessageID;           
      	   				gptr->srcID = p->sourceID;
      	   				gptr->pi = pi;    
      	   				GlobalMessageID++;         
      	   				pthread_mutex_unlock (&gptr_mutex);        
                  		}
	         	}
		 	a[0] = a[1];								//copy a[1] to a[0] and set a[1] to 0 for next mouseclick 
		 	a[1] = 0;	
		}
        	else if((p->sourceID) == 6 && ie.code == 273 && ie.value == 1 )			
		{
			//printf("right button pressed\n");					//read code, value and sourceID. If souceID is 6, value and code match right click,enqueue message to 													dataqueue2.
			pthread_mutex_lock (&gptr_mutex);					//lock mutex and enqueue message using the same method
               		gptr = (struct message*)malloc(sizeof(struct message));
      
               		double pi= calculatepi();
               		int write_return = sq_write(p->queuename, gptr);
        
               		if(write_return == 0)
               		{	
       		   		gptr->queueTime = rdtsc();	
       		   		gptr->msgID = GlobalMessageID;           
       		   		gptr->srcID = p->sourceID;
       		   		gptr->pi = pi;    
       		   		GlobalMessageID++;         
       		   		pthread_mutex_unlock (&gptr_mutex);       
      	                }
		}
    		sem_post(&m);									//post semaphore so that other aperiodic thread waiting can access the dataqueue and enqueue
    	}
    return 0;
}

void* ReceiverFunction(void* parameter)								//general function called by aperiodic threads
{   												//the next 5 lines force the threads to run on cpu0
    	cpu_set_t cpuset;
    	int cpu = 0;
    	CPU_ZERO(&cpuset);
    	CPU_SET( cpu, &cpuset);
    	sched_setaffinity(0, sizeof(cpuset), &cpuset);
      
    	struct ReceiverFunctionParameters* q = (struct ReceiverFunctionParameters*) parameter; 
    	int read_return;
    
    	struct timespec deadline;								//defines struct timespec variable to set wakeup times
    	clock_gettime(CLOCK_MONOTONIC, &deadline);
 
    	while(flag != 1)									//see if flag is not equal to 1
    	{
   
    		while((q->queuename1)->valid_items != 0)					//read all items from dataqueue1
    		{
    			read_return = sq_read(q->queuename1, no2, rptr);			//call sq_read to read pointers from dataqueue1 and store in array rptr[]
			if(read_return == 0)
			{
	   			GlobalMessageCounter++;
	   			(rptr[no2])->dequeueTime = rdtsc();
	   			(rptr[no2])->QueueingTime = (((rptr[no2])->dequeueTime) - ((rptr[no2])->queueTime));
	   			//printf("  %d              \t %d    \t %d          \t %.7f s \t %.4f\n", (rptr[no2])->msgID, (rptr[no2])->srcID,GlobalMessageCounter, (float)(rptr[no2])->QueueingTime/(float)CPU_FREQ, (rptr[no2])->pi); 
	   			no2++; 	
			} 	  	
    		}
 
    		while((q->queuename2)->valid_items != 0)					//read all items from dataqueue2
    		{
    			read_return = sq_read(q->queuename2, no2, rptr);			//calls sq_read to read pointers from dataqueue2 and store in array rptr[]
			if(read_return == 0)
			{
	   			GlobalMessageCounter++;
	   			(rptr[no2])->dequeueTime = rdtsc();
	   			(rptr[no2])->QueueingTime = (((rptr[no2])->dequeueTime) - ((rptr[no2])->queueTime));
	   			//printf("  %d              \t %d    \t %d          \t %.7f s \t %.4f\n", (rptr[no2])->msgID, (rptr[no2])->srcID,GlobalMessageCounter, (float)(rptr[no2])->QueueingTime/(float)CPU_FREQ, (rptr[no2])->pi); 
	   			no2++; 	
			} 	  	
    		}

    		deadline.tv_nsec += q->sleeptime;						//this block calculates absolute time to wake up corresponding thread after sleep
    		if(deadline.tv_nsec >= 1000000000)
    		{
        		deadline.tv_nsec -= 1000000000;
        		deadline.tv_sec++;
    		}
    		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &deadline, 0);      
    	}

    	while((q->queuename1)->valid_items != 0)						//the next two while loops read dataqueue1 and 2 respectively, again, even after flag is set to 1,just to 													ensure no messages are left on the queues afterperiodic threads and aperiodic threads exit
    	{
    		read_return = sq_read(q->queuename1, no2, rptr);
		if(read_return == 0)
		{
	   		GlobalMessageCounter++;
	   		(rptr[no2])->dequeueTime = rdtsc();
	   		(rptr[no2])->QueueingTime = (((rptr[no2])->dequeueTime) - ((rptr[no2])->queueTime));
	   		//printf("  %d              \t %d    \t %d          \t %.7f s \t %.4f\n", (rptr[no2])->msgID, (rptr[no2])->srcID,GlobalMessageCounter, (float)(rptr[no2])->QueueingTime/(float)CPU_FREQ, (rptr[no2])->pi ); 
	   	no2++; 	
		} 	  	
    	}
 
    	while((q->queuename2)->valid_items != 0)
    	{
    		read_return = sq_read(q->queuename2, no2, rptr);
		if(read_return == 0)
		{
	   		GlobalMessageCounter++;
	   		(rptr[no2])->dequeueTime = rdtsc();
	   		(rptr[no2])->QueueingTime = (((rptr[no2])->dequeueTime) - ((rptr[no2])->queueTime));
	   		//printf("  %d              \t %d    \t %d          \t %.7f s \t %.4f\n", (rptr[no2])->msgID, (rptr[no2])->srcID,GlobalMessageCounter, (float)(rptr[no2])->QueueingTime/(float)CPU_FREQ, (rptr[no2])->pi ); 
	   		no2++; 	
		} 	  	
    	}
    	return 0;
}



int main()
{   
    	sem_init(&m, 0, 1);					//initialize semaphore
       
    	dataqueue dataqueue1;					//create dataqueue1
    	sq_create(&dataqueue1);			
    	dataqueue dataqueue2;					//create dataqueue2    
    	sq_create(&dataqueue2);
    
    	pthread_attr_t ptattr;					//pthread attribute for 2700000000periodic threads
    	pthread_attr_t aptattr;					//pthread attribute for aperiodic threads
    	pthread_attr_t rtattr;					//pthread attribute for receiver thread
    
    	pthread_t periodic1_id;					//respective pthread ids for all threads
    	pthread_t periodic2_id;
    	pthread_t periodic3_id;
    	pthread_t periodic4_id;
    	pthread_t aperiodic1_id;
    	pthread_t aperiodic2_id;
    	pthread_t receiver_id;

    	const int p_period_multiplier[4] = {12,32,18,28};	//array containing period multipliers for periodic threads
    	const int r_period_multiplier[1] = {40};		//array containing period multiplier for receiver thread

    	int ptprio = 30;					//set priority value for periodic thread
    	struct sched_param param1;				//create struct sched_param variable	
    
    	pthread_attr_init (&ptattr);				//initialized with default attributes      
    	pthread_attr_getschedparam (&ptattr, &param1);		//safe to get existing scheduling param    
    	param1.sched_priority = ptprio;				//set the priority; others are unchanged    
    	pthread_attr_setschedpolicy(&ptattr, SCHED_FIFO);	//set the scheduling policy   
    	pthread_attr_setschedparam (&ptattr, &param1);		//setting the new scheduling param

    	int aptprio = 50;					//do it for aperiodic thread and receiver thread	
    	struct sched_param param2;
    	pthread_attr_init (&aptattr); 
    	pthread_attr_getschedparam (&aptattr, &param2);
    	param2.sched_priority = aptprio; 
    	pthread_attr_setschedpolicy(&aptattr, SCHED_FIFO); 
    	pthread_attr_setschedparam (&ptattr, &param2);

    	int rtprio = 20;
    	struct sched_param param3;
    	pthread_attr_init (&rtattr); 
    	pthread_attr_getschedparam (&rtattr, &param3);
    	param3.sched_priority = rtprio;
    	pthread_attr_setschedpolicy(&rtattr, SCHED_FIFO);
    	pthread_attr_setschedparam (&rtattr, &param3);
    
 	
    	struct PeriodicFunctionParameters p1; 			//create and set parameters for periodic thread 1   
    	p1.queuename = &dataqueue1;
    	p1.sourceID = 1;
    	p1.sleeptime = p_period_multiplier[0]*1000*BASE_PERIOD;

    	struct PeriodicFunctionParameters p2;			//create and set parameters for periodic thread 2    
    	p2.queuename = &dataqueue1;
    	p2.sourceID = 2;
    	p2.sleeptime = p_period_multiplier[1]*1000*BASE_PERIOD;	
   
    	struct PeriodicFunctionParameters p3;    		//create and set parameters for periodic thread 3
    	p3.queuename = &dataqueue2;
    	p3.sourceID = 3;
    	p3.sleeptime = p_period_multiplier[2]*1000*BASE_PERIOD;	

    	struct PeriodicFunctionParameters p4;    		//create and set parameters for periodic thread 4
    	p4.queuename = &dataqueue2;
    	p4.sourceID = 4;
    	p4.sleeptime = p_period_multiplier[3]*1000*BASE_PERIOD;	

    	struct AperiodicFunctionParameters ap1;			//create and set parameters for aperiodic thread 5
    	ap1.queuename = &dataqueue1;
    	ap1.sourceID = 5;
   
    	struct AperiodicFunctionParameters ap2;			//create and set parameters for aperiodic thread 6
    	ap2.queuename = &dataqueue2;
    	ap2.sourceID = 6;  

    	struct ReceiverFunctionParameters r1;			//create and set parameters for receiver thread  
    	r1.queuename1 = &dataqueue1;    
    	r1.queuename2 = &dataqueue2;
    	r1.sleeptime = r_period_multiplier[0]*BASE_PERIOD*1000;    
    
    	//printf("GlobalMessageID \t srcID \t MessageCount\t QueueingTime\t PiValue\n\n");
   
    	pthread_create (&periodic1_id, &ptattr, &PeriodicFunction, &p1);		//create periodic thread 1
    	pthread_create (&periodic2_id, &ptattr, &PeriodicFunction, &p2); 		//create periodic thread 2
    	pthread_create (&periodic3_id, &ptattr, &PeriodicFunction, &p3);		//create periodic thread 3
    	pthread_create (&periodic4_id, &ptattr, &PeriodicFunction, &p4); 		//create periodic thread 4
    	pthread_create (&receiver_id, &rtattr, &ReceiverFunction, &r1); 		//create receiver thread
    
    	int fd;										//the following code monitors mouse clicks.
	struct input_event ie;
	fd = open(MOUSEFILE, 0);
	
	if(fd == -1)
	{
		perror("opening device");
		exit(EXIT_FAILURE);
	}
    	int counter=0;									
    	int left_counter = 0;
    	int right_counter = 0;  

    	while(counter <=1)									//while loop designed in such a way that it runs until one left and one right clicks are detected
    	{	
        	if(flag ==1) break;
		read(fd, &ie, sizeof(struct input_event));

            	if(left_counter<=0 && ie.code == 272 && ie.value == 1 )				//first left click detected.
            	{ 
	      		counter++;	
	       		left_counter++;
	       		//printf("left button pressed\n");
	       		pthread_create (&aperiodic1_id, &aptattr, &AperiodicFunction, &ap1);	//create aperiodic thread 1	
	    	}
            	if(right_counter <=0 && ie.code == 273 && ie.value == 1)			//first right click detected
            	{
               		counter++;
               		right_counter++;
	       		//printf("right button pressed\n"); 
	       		pthread_create (&aperiodic2_id, &aptattr, &AperiodicFunction, &ap2);	//create aperiodic thread 2
	    	}        
    	}
    
    	pthread_join (periodic1_id, NULL);							//join all periodic threads
    	pthread_join (periodic2_id, NULL);
    	pthread_join (periodic3_id, NULL);
    	pthread_join (periodic4_id, NULL);
    	//pthread_join (aperiodic1_id, NULL);							//no neae to join aperiodic threads, because they define the event termination 
    	//pthread_join (aperiodic2_id, NULL);    
    	pthread_join (receiver_id, NULL);							//join the receiver thread
    
    	//printQueue(&dataqueue1);    
    	//printQueue(&dataqueue1);
	
    	unsigned long int sum = 0;								//simple program to find the mean of all queueing times
    	float mean;
    	int counter2;
    	for(counter2 = 0; counter2 < no2; counter2++)
    	{
		sum += (rptr[counter2])->QueueingTime;
    	}
    	mean = (float)sum/(float)no2;
        printf("Total number of messages sent are %d.\n",no2); 
    	printf("Total number of messages received and dequeued are : %d.\n", GlobalMessageCounter);	//prints the total no of messages received at the reciever end
    	printf("Mean time spent on queue is %.4f s.\n", mean/(float)CPU_FREQ);			//prints the mean queueing time of all the messages

    	float difference = 0;										//program to find the standard deviation of all queueing times
    	float dsquares = 0;

    	for(counter2=0; counter2 < no2; counter2++)
    	{
    		difference =((float)rptr[0]->QueueingTime/(float)CPU_FREQ) - mean/(float)CPU_FREQ;    
    		dsquares += difference*difference;
    	}
    	dsquares = dsquares/ (float)no2;
    	dsquares = sqrt(dsquares);   
    	printf("Standard Deviation of times spent by messages on queues is %.4f s\n", dsquares);
    	sq_delete(&dataqueue1);										//calls sq_delete to free memory
    	sq_delete(&dataqueue2);
    
    	return 0;
}													//exit from program


