#define _GNU_SOURCE				//include required library files
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>
#include "rdtsc.h"				//rdstc is a header file which helps read the time stamp counter				
#include <math.h>
#include <sched.h>
#include <semaphore.h>
//#include <linux/module.h>
#include <linux/kernel.h>
//#include <linux/init.h>
#define DATAQUEUE1 "/dev/dataqueue1"
#define DATAQUEUE2 "/dev/dataqueue2"
#define MOUSEFILE "/dev/input/event6"		//MOUSEFILE declared. Event number may change on different machines. Write cat /proc/bus/input/devices to see the event number of mouse/touchpad and edit	
#define BASE_PERIOD 1000			//base period for periodic threads	
#define CPU_FREQ 2.7e9				//CPU_FREQ may change on different machines

sem_t left;					//semaphore initialization for left and right click			
sem_t right;			

static int flag = 0;				//flag set equal to 1 when double click detected	
static int GlobalMessageID = 1;			//unique increasing message id for every message created
static int GlobalMessageCounter = 0;		//message counter which is incremented by one whenever any message is dequeued		
static int no2 = 0; 				//rptr[] array indice

pthread_mutex_t gptr_mutex = PTHREAD_MUTEX_INITIALIZER;		//define mutex "gptr" which will control and help synchronize all threads 

struct message							//general structure for all messages
{
	int msgID;
    	int srcID;
    	unsigned long long queueTime;
    	unsigned long long dequeueTime;
    	unsigned long int QueueingTime;
    	double pi;
};

struct message* gptr;				// struct message* to store the pointer returned when struct message variable is declared
struct message* rptr;				//received struct message pointers stored in this array	

unsigned long long QueueingTime[10000];
int indice = 0;

struct PeriodicFunctionParameters		//arguments structure for periodic threads		
{
    	int sourceID;
    	long sleeptime;
};

struct AperiodicFunctionParameters		//arguments structure for aperiodic threads		
{
    	int sourceID;
};

struct ReceiverFunctionParameters		//arguments structure for receiver threads		
{
    	long sleeptime;
};

float calculatepi(void)				//calculates value of pi using a special algorithm			
{
   	double number, k; 			// Number of iterations and control variable      		
   	double s = 1;   			//Signal for the next iteration  			
   	double pi = 0;
   	number = rand()%(50-10+1) + 10;  	//generates a random number between 10 and 40    	

   	for(k = 1; k <= (number * 2); k += 2)
	{
     		pi = pi + s * (4 / k);
     		s = -s;
   	}
   	return(pi);
}


void* PeriodicFunction(void* parameters)	//general function called by all periodic							
{  
    	struct PeriodicFunctionParameters* p = (struct PeriodicFunctionParameters*) parameters;	//copy parameters into local pointer p
    	struct timespec deadline;				//stuct timespec variable							
    	clock_gettime(CLOCK_MONOTONIC, &deadline);		//gets current time and stores it in struct timespec variable "deadline"	
	int fd1,fd2,k,j;
	while(flag !=1)						//all periodic threads run until this flag is set to 0
	{
		if(p->sourceID<3)				//for Periodic Threads 1 and 2
		{   	
      		pthread_mutex_lock (&gptr_mutex);    			//mutex gptr locked  		
		fd1 = open(DATAQUEUE1, O_RDWR);				//open DATAQUEUE1 device
        	if(fd1==-1)
		{
			printf("device not found");
		}
      		gptr = (struct message*)malloc(sizeof(struct message));		//dynamically allocate memory to struct message variable and store the pointer in "gptr"
		gptr->queueTime = rdtsc();	//rdtsc enters the current counter value into queueTime						
        	gptr->msgID = GlobalMessageID;            
        	gptr->srcID = p->sourceID;
        	gptr->pi = calculatepi();
		//printf("gptr : %p\n", gptr);			
      			
		k = write(fd1, gptr, sizeof(struct message));   //call write function to write the message pointer to dataqueue of the driver
	
		if(k<0)
		{
			//errno = EINVAL;
			//printf("error in writing\n");
		}        	
		
      		else			//if write is successfull increment GlobalMessageID++						
      		{       	    
        		GlobalMessageID++;         
        							
        	}
		close(fd1);		//close DATAQUEUE1 device			
      		pthread_mutex_unlock (&gptr_mutex); 			//writing done, free mutex gptr, so that other waiting threads can wrie to dataqueues
      		if(deadline.tv_nsec >= 1000000000)			//this block calculates absolute time to wake up corresponding thread after sleep
      		{
         		deadline.tv_nsec -= 1000000000;
         		deadline.tv_sec++;
      		}
      		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &deadline, 0);		//makes the thread sleep for period specified in arguments	
    		}
	
		else if(p->sourceID>=3)					//for Periodic Threads 3 and 4, the function works exactly the same 
		{    	
      		pthread_mutex_lock (&gptr_mutex);      		
		fd2 = open(DATAQUEUE2, O_RDWR);
        	if(fd2==-1)
		{
			printf("device not found");
		}
      		gptr = (struct message*)malloc(sizeof(struct message));	
		gptr->queueTime = rdtsc();						
        	gptr->msgID = GlobalMessageID;            
        	gptr->srcID = p->sourceID;
        	gptr->pi = calculatepi();
		//printf("gptr : %p\n", gptr);			
      			
		j = write(fd2, gptr, sizeof(struct message));
	
		if(j<0)
		{
			//errno = EINVAL;
			//printf("error in writing\n");
		}        	
		
      		else								
      		{       	    
        		GlobalMessageID++;         
        							
        	}
		close(fd2);				
      		pthread_mutex_unlock (&gptr_mutex); 
      		if(deadline.tv_nsec >= 1000000000)
      		{
         		deadline.tv_nsec -= 1000000000;
         		deadline.tv_sec++;
      		}
      		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &deadline, 0);
    		}
	}
    	return 0;
}

void* ReceiverFunction(void* parameter)			//general function called by aperiodic threads							
{       
	
	int fd1, fd2, ret1=0, ret2=0;
    	struct ReceiverFunctionParameters* q = (struct ReceiverFunctionParameters*) parameter;    
    	struct timespec deadline;			//defines struct timespec variable to set wakeup times						
    	clock_gettime(CLOCK_MONOTONIC, &deadline);
 
    	while(flag != 1)									
    	{	
		pthread_mutex_lock (&gptr_mutex);
		fd1 = open(DATAQUEUE1, O_RDWR);		//opens device DATAQUEUE1
        	if(fd1==-1)
		{
			printf("device not found");
		}
		while(ret1>=0){		
		rptr = (struct message*)malloc(sizeof(struct message));
		ret1 = read(fd1,(void*) rptr, sizeof(struct message));
		if(ret1<0)
		{
			//printf("error in reading\n");
		}
		if(!ret1)			//prints the message structure after calling the read function of the driver till dataqueue1 is empty
		{	
			printf("rptr : %p\n", rptr);
			GlobalMessageCounter++;
		   	(rptr)->dequeueTime = rdtsc();
		   	(rptr)->QueueingTime = (((rptr)->dequeueTime) - ((rptr)->queueTime));
			QueueingTime[indice]= (rptr)->QueueingTime;
		   	printf("  %d              \t %d    \t %d          \t %.7f s \t %.4f\n", (rptr)->msgID, (rptr)->srcID,GlobalMessageCounter, (float)(rptr)->QueueingTime/(float)CPU_FREQ, (rptr)->pi); 
		   	no2++; 
			indice++;
		} }
		close(fd1); 					//closes device DATAQUEUE1

		fd2 = open(DATAQUEUE2, O_RDWR);			//opens device DATAQUEUE2
        	if(fd2==-1)
		{
			printf("device not found");
		}
		while(ret2>=0){		
		rptr = (struct message*)malloc(sizeof(struct message));
		ret2 = read(fd2,(void*) rptr, sizeof(struct message));
		if(ret2<0)
		{
			//printf("error in reading\n");
		}
		if(!ret2)		//prints the message structure after calling the read function of the driver till dataqueue2 is empty
		{	
			//printf("rptr : %p\n", rptr);
			GlobalMessageCounter++;
		   	(rptr)->dequeueTime = rdtsc();
		   	(rptr)->QueueingTime = (((rptr)->dequeueTime) - ((rptr)->queueTime));
			QueueingTime[indice]= (rptr)->QueueingTime;
		   	printf("  %d              \t %d    \t %d          \t %.7f s \t %.4f\n", (rptr)->msgID, (rptr)->srcID,GlobalMessageCounter, (float)(rptr)->QueueingTime/(float)CPU_FREQ, (rptr)->pi); 
		   	no2++; 
			indice++;
		} }
		close(fd2);				//closes device DATAQUEUE2
		pthread_mutex_unlock (&gptr_mutex);
   		printf("reading done here\n");

    		deadline.tv_nsec += q->sleeptime;	//this block calculates absolute time to wake up corresponding thread after sleep					
    		if(deadline.tv_nsec >= 1000000000)
    		{
        		deadline.tv_nsec -= 1000000000;
        		deadline.tv_sec++;
    		}
    		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &deadline, 0);    
		ret1=0, ret2=0;  
		//fd1=fd2=0;
    	}

	ret1=0, ret2=0;
	
    	pthread_mutex_lock (&gptr_mutex);
	fd1 = open(DATAQUEUE1, O_RDWR);			//opens device DATAQUEUE1 and same logic as above, it is just done to check that the DATAQUEUE1 is empty
        	if(fd1==-1)
		{
			printf("device not found");
		}
		while(ret1>=0){		
		rptr = (struct message*)malloc(sizeof(struct message));
		ret1 = read(fd1,(void*) rptr, sizeof(struct message));
		if(ret1<0)
		{
			printf("error in reading\n");
		}
		if(!ret1)
		{	
			printf("rptr : %p\n", rptr);
			GlobalMessageCounter++;
		   	(rptr)->dequeueTime = rdtsc();
		   	(rptr)->QueueingTime = (((rptr)->dequeueTime) - ((rptr)->queueTime));
			QueueingTime[indice]= (rptr)->QueueingTime;
		   	printf("  %d              \t %d    \t %d          \t %.7f s \t %.4f\n", (rptr)->msgID, (rptr)->srcID,GlobalMessageCounter, (float)(rptr)->QueueingTime/(float)CPU_FREQ, (rptr)->pi); 
		   	no2++; 
			indice++;
		} }
		close(fd1); 			//closes DATAQUEUE1

		fd2 = open(DATAQUEUE2, O_RDWR);		//opens device DATAQUEUE2 and same logic as above, it is just done to check that the DATAQUEUE2 is empty
        	if(fd2==-1)
		{
			printf("device not found");
		}
		while(ret2>=0){		
		rptr = (struct message*)malloc(sizeof(struct message));
		ret2 = read(fd2,(void*) rptr, sizeof(struct message));
		if(ret2<0)
		{
			printf("error in reading\n");
		}
		if(!ret2)
		{	
			//printf("rptr : %p\n", rptr);
			GlobalMessageCounter++;
		   	(rptr)->dequeueTime = rdtsc();
		   	(rptr)->QueueingTime = (((rptr)->dequeueTime) - ((rptr)->queueTime));
			QueueingTime[indice]= (rptr)->QueueingTime;
		   	printf("  %d              \t %d    \t %d          \t %.7f s \t %.4f\n", (rptr)->msgID, (rptr)->srcID,GlobalMessageCounter, (float)(rptr)->QueueingTime/(float)CPU_FREQ, (rptr)->pi); 
		   	no2++; 
			indice++;
		} }
		close(fd2);			//closes DATAQUEUE2
	pthread_mutex_unlock (&gptr_mutex);
		return 0;
}


void* leftClick(void* parameters)		//general function called by leftClick threads			
{   	
	while(flag != 1) {
        struct AperiodicFunctionParameters* p = (struct AperiodicFunctionParameters*) parameters;
	
    	sem_wait(&left);			//waits here till left click is detected and thread is to be created
	//printf("inside left click function\n");
	pthread_mutex_lock (&gptr_mutex);
	int fd,k;
	fd = open(DATAQUEUE1, O_RDWR);		//opens device DATAQUEUE1
       	if(fd==-1)
	{
		printf("device not found");
	}
      	gptr = (struct message*)malloc(sizeof(struct message));	
	gptr->queueTime = rdtsc();						
        gptr->msgID = GlobalMessageID;            
        gptr->srcID = p->sourceID;
        gptr->pi = calculatepi();
	printf("gptr : %p\n", gptr);			
      			
	k = write(fd, gptr, sizeof(struct message));	//
	
	if(k<0)
	{
		//errno = EINVAL;
		printf("error in writing\n");
	}        	
	
      	else								
      	{       	    
        	GlobalMessageID++;         
        						
        }
	close(fd);			//closes device DATAQUEUE1
	pthread_mutex_unlock (&gptr_mutex); 
	}
	
        return 0;	
}

void* rightClick(void* parameters)		//general function called by rightClick threads and follows same logic as leftClick threads
{   	
	while (flag != 1) {
        struct AperiodicFunctionParameters* p = (struct AperiodicFunctionParameters*) parameters;
        
    	sem_wait(&right);
	printf("inside right click function\n");
	pthread_mutex_lock (&gptr_mutex);
	int fd, k;
	fd = open(DATAQUEUE2, O_RDWR);
       	if(fd==-1)
	{
		printf("device not found");
	}
      	gptr = (struct message*)malloc(sizeof(struct message));	
	gptr->queueTime = rdtsc();						
        gptr->msgID = GlobalMessageID;            
        gptr->srcID = p->sourceID;
        gptr->pi = calculatepi();
	//printf("gptr : %p\n", gptr);			
      			
	k = write(fd, gptr, sizeof(struct message));
	
	if(k<0)
	{
		//errno = EINVAL;
		printf("error in writing\n");
	}        	
	
      	else								
      	{       	    
        	GlobalMessageID++;         
        						
        }
	close(fd);
	pthread_mutex_unlock (&gptr_mutex); 
        //i++;
	}
	return 0;	
}

int main()
{	
	sem_init(&left, 0, 1);			//initialize semaphore left				
	sem_init(&right, 0, 1);			//initialize semaphore right
	sem_wait(&left);
	sem_wait(&right);
	
	pthread_attr_t ptattr;			//pthread attribute for periodic threads
	pthread_attr_t aptattr;			//pthread attribute for aperiodic threads
	pthread_attr_t rtattr;			//pthread attribute for receiver threads

	pthread_t periodic1_id;			//respective pthread ids for all threads
	pthread_t periodic2_id;
    	pthread_t periodic3_id;
    	pthread_t periodic4_id;	
	pthread_t aperiodic1_id;
    	pthread_t aperiodic2_id;
	pthread_t receiver_id;
	
	const int p_period_multiplier[4] = {12,32,18,28};	//array containing period multipliers for periodic threads
	const int r_period_multiplier[1] = {40};		//create struct sched_param variable

	int ptprio = 30;					//set priority value for periodic thread				
    	struct sched_param param1;    
    	pthread_attr_init (&ptattr);				//initialized with default attributes 				      
    	pthread_attr_getschedparam (&ptattr, &param1);		//safe to get existing scheduling param		   
    	param1.sched_priority = ptprio;				//set the priority; others are unchanged				    
    	pthread_attr_setschedpolicy(&ptattr, SCHED_FIFO);	//set the scheduling policy 	  
    	pthread_attr_setschedparam (&ptattr, &param1);		//setting the new scheduling param

	int aptprio = 50;				//do it for aperiodic thread and receiver thread						
    	struct sched_param param2;			
    	pthread_attr_init (&aptattr); 
    	pthread_attr_getschedparam (&aptattr, &param2);
    	param2.sched_priority = aptprio; 
    	pthread_attr_setschedpolicy(&aptattr, SCHED_FIFO); 
    	pthread_attr_setschedparam (&aptattr, &param2);

	int rtprio = 20;				//set priority value for receiver thread
    	struct sched_param param3;
    	pthread_attr_init (&rtattr); 
    	pthread_attr_getschedparam (&rtattr, &param3);
    	param3.sched_priority = rtprio;
    	pthread_attr_setschedpolicy(&rtattr, SCHED_FIFO);
    	pthread_attr_setschedparam (&rtattr, &param3);

	struct PeriodicFunctionParameters p1;		//create and set parameters for periodic thread 1  
    	p1.sourceID = 1;
    	p1.sleeptime = p_period_multiplier[0]*1000*BASE_PERIOD;

	struct PeriodicFunctionParameters p2;		//create and set parameters for periodic thread 2
    	p2.sourceID = 2;
    	p2.sleeptime = p_period_multiplier[1]*1000*BASE_PERIOD;	
   
    	struct PeriodicFunctionParameters p3;		//create and set parameters for periodic thread 3
    	p3.sourceID = 3;
    	p3.sleeptime = p_period_multiplier[2]*1000*BASE_PERIOD;	

    	struct PeriodicFunctionParameters p4;		//create and set parameters for periodic thread 4
    	p4.sourceID = 4;
    	p4.sleeptime = p_period_multiplier[3]*1000*BASE_PERIOD;

	struct AperiodicFunctionParameters ap1;		//create and set parameters for aperiodic thread 1 left click  
    	ap1.sourceID = 5;
   
    	struct AperiodicFunctionParameters ap2;		//create and set parameters for aperiodic thread 2 right click
    	ap2.sourceID = 6;	

	struct ReceiverFunctionParameters r1;		//create and set parameters for receiver thread 
    	r1.sleeptime = r_period_multiplier[0]*1000*BASE_PERIOD;

	pthread_create (&periodic1_id, &ptattr, &PeriodicFunction, &p1);	//create threads
	pthread_create (&periodic2_id, &ptattr, &PeriodicFunction, &p2); 		
    	pthread_create (&periodic3_id, &ptattr, &PeriodicFunction, &p3);		
    	pthread_create (&periodic4_id, &ptattr, &PeriodicFunction, &p4);
	pthread_create (&receiver_id, &rtattr, &ReceiverFunction, &r1);
	pthread_create (&aperiodic1_id, &aptattr, &leftClick, &ap1);
	pthread_create (&aperiodic2_id, &aptattr, &rightClick, &ap2);
	

	int fd;									
	struct input_event ie;
	fd = open(MOUSEFILE, 0);		//open mouse event
	
	if(fd == -1)
	{
		perror("opening device");
		exit(EXIT_FAILURE);
	}
    	
	unsigned long long a[2];
	while(flag != 1)
	{
		read(fd, &ie, sizeof(struct input_event));		//reads the mouse
		if(ie.code == 272 && ie.value == 1 )			//for left click detect	 													
		{
			a[1] = rdtsc();

			if((float)(a[1] - a[0])/(float)2.7e9 <= 0.5)	//caculates time between consecutive left click				
		 	{ 
				printf("double click detected\n");				
				flag =1;							
				break;				//breaks if double click is detected
			}
			else
			{
				//printf("left key pressed\n");
				sem_post(&left);
				a[0] = a[1];
	       		}
											
		 	
		}


        	else if(ie.code == 273 && ie.value == 1 )		//for right click detect			
		{	
			printf("right key pressed\n");		
		 	a[0] = 0;									
			sem_post(&right);
		}
	}


	pthread_join (periodic1_id, NULL);
	pthread_join (periodic2_id, NULL);
    	pthread_join (periodic3_id, NULL);
    	pthread_join (periodic4_id, NULL);
	pthread_join (receiver_id, NULL);
	

	unsigned long int sum = 0;			//simple program to find the mean of all queueing times								
    	float mean;
    	int counter2;
    	for(counter2 = 0; counter2 < no2; counter2++)
    	{
		sum += QueueingTime[counter2];
    	}
    	mean = (float)sum/(float)no2;
        printf("Total number of messages sent are %d.\n",no2); 
    	printf("Total number of messages received and dequeued are : %d.\n", GlobalMessageCounter);	
    	printf("Mean time spent on queue is %.4f s.\n", mean/(float)CPU_FREQ);			

    	float difference = 0;				//program to find the standard deviation of all queueing times									
    	float dsquares = 0;

    	for(counter2=0; counter2 < no2; counter2++)
    	{
    		difference =(QueueingTime[counter2]/(float)CPU_FREQ) - mean/(float)CPU_FREQ;    
    		dsquares += difference*difference;
    	}
    	dsquares = dsquares/ (float)no2;
    	dsquares = sqrt(dsquares);   
    	printf("Standard Deviation of times spent by messages on queues is %.4f s\n", dsquares);	

	return 0;				//exit program

}















