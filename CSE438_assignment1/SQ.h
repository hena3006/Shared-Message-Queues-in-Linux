#define QueueLength 10
typedef struct SharedQueue
{
    int first;
    int last;
    int valid_items;
    struct message *buffer[QueueLength];
} dataqueue;


struct message* rptr[100000];
void sq_create(dataqueue *queuename);
int isEmpty(dataqueue *queuename);
int sq_write(dataqueue *queuename,struct message* ptr);
int sq_read(dataqueue *queuename,int no2, struct message* rptr[]);
void printQueue(dataqueue *queuename);  //uncomment this line if you include printQueue function in the library file
void sq_delete(dataqueue* queuename);
