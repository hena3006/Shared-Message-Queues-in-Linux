#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/string.h>
#include <linux/device.h>
#include <linux/jiffies.h>
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/time.h>
//#include <semaphore.h>

#define QUEUE_LENGTH 10

#define DEVICE_NAME "MYDRIVER"

static dev_t queue_id;

/*static __inline__ unsigned long long tsc(void)
{
	unsigned hi, lo;
	__asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
	return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}*/


struct message
{
	int msgID;
    	int srcID;
    	unsigned long long queueTime;
    	unsigned long long dequeueTime;
    	unsigned long int QueueingTime;
    	double pi;
};

struct class *class1;

typedef struct SharedQueue
{
    	int first;
    	int last;
    	int valid_items;
    	struct message* buffer[QUEUE_LENGTH];
} dataqueue;

static struct dev
{
	struct cdev char_device;
	struct SharedQueue q;
	char *name;
	//struct semaphore hold;
}* my_device[2];

int my_open(struct inode *i, struct file *f)
{
	struct dev *devp;
	devp = container_of(i->i_cdev, struct dev, char_device);
	f->private_data = devp;
	printk(KERN_INFO"\n%s is opening\n", devp->name);
	return 0;
}

int my_close(struct inode *i, struct file *f)
{
	struct dev *devp=f->private_data;
	printk(KERN_INFO"\n%s is closing\n", devp->name);
	return 0;
}

ssize_t sq_write(struct file *f,const char *m1, size_t count, loff_t *offp)
{	
	struct dev *devp = f->private_data;
	if(devp->q.valid_items == QUEUE_LENGTH)
	{
		printk("Queue of %s is full\n", devp->name);
		return -1;
	}

        devp->q.buffer[devp->q.last] = (struct message*)kmalloc(sizeof(struct message), GFP_KERNEL);
	
	if(copy_from_user(devp->q.buffer[devp->q.last],m1,sizeof(struct message)))
		printk("Can't write in %s\n", devp->name);        

	devp->q.valid_items++;
	devp->q.last = (devp->q.last + 1)%QUEUE_LENGTH;
        return 0;

}

ssize_t sq_read(struct file *f, char *m1, size_t count, loff_t *ppos)
{
	int bytes;
	struct dev *devp = f->private_data;
	if(devp->q.valid_items == 0)
	{
		printk("Queue of %s is empty\n", devp->name);
		return -1;
	}
	
	bytes = copy_to_user(m1,devp->q.buffer[devp->q.first], sizeof(struct message));
	if(bytes)
		printk("Can't read %d bytes from %s\n", bytes, devp->name);
	kfree(devp->q.buffer[devp->q.first]); 
	devp->q.first=(devp->q.first + 1)%QUEUE_LENGTH;
        devp->q.valid_items--;
	return 0;
}

static struct file_operations my_operations = {
	.owner = THIS_MODULE,
	.open = my_open,
	.release = my_close,
	.write = sq_write,
	.read = sq_read, 
};

int __init mydriver_init(void)
{
	int i,k,err;
	printk(KERN_ALERT "initializing\n");
	if(alloc_chrdev_region(&queue_id,0,2,DEVICE_NAME)<0)
	{
		printk(KERN_DEBUG "Can't register device\n");
	}
	if((class1=class_create(THIS_MODULE,DEVICE_NAME)) == NULL)
	{
		unregister_chrdev_region(queue_id, 2);
	}

	for(k=0;k<2;k++)
	{
		my_device[k] = (struct dev*) kmalloc(sizeof(struct dev), GFP_KERNEL);
		if (!my_device[k])
		{
			printk("Bad kmalloc\n");
			return -ENOMEM;
		}
		my_device[k]->q.first=0;
		my_device[k]->q.last=0;
		my_device[k]->q.valid_items=0;
	
		for(i=0; i<QUEUE_LENGTH; i++)
    		{
        		my_device[k]->q.buffer[i] = 0;
    		}
	}
	my_device[0]->name="dataqueue1";
	my_device[1]->name="dataqueue2";
	
	for(k=0;k<2;k++)
	{
		if(device_create(class1, NULL, MKDEV(MAJOR(queue_id), MINOR(queue_id)+k), NULL, "dataqueue%d", k+1) == NULL)
		{
			class_destroy(class1);
			unregister_chrdev_region(queue_id, 2);
			printk(KERN_DEBUG "Can't register input device\n");
			return -1;
		}
	}
	for(k=0;k<2;k++)
	{
		cdev_init(&(my_device[k]->char_device), &my_operations);
		my_device[k]->char_device.owner = THIS_MODULE;
		err=cdev_add(&my_device[k]->char_device,MKDEV(MAJOR(queue_id), MINOR(queue_id)+k), 1);
		if(err)
		{
			printk("Bad cdev\n");
			return err;
		}
	}
	printk(KERN_INFO"queues created as devices");
	return 0;
}

void __exit mydriver_exit(void)
{
	int k;
	for(k=0;k<2;k++)
	{
		cdev_del(&my_device[k]->char_device);
		device_destroy(class1, MKDEV(MAJOR(queue_id), MINOR(queue_id)+k));
		kfree(my_device[k]);
	}
	class_destroy(class1);
	unregister_chrdev_region(queue_id,2);
}

module_init(mydriver_init);
module_exit(mydriver_exit);
MODULE_LICENSE("GPL v2");

	





