/* cpu.c
 * Main source for linux scheduler virtual machine.
 * Emulates a simple CPU running processes with a generic interrupt
 * for "IO."
 *
 * Requires a properly written schedule.c and schedule.h files 
 * with stubs filled out.
 */

#include "privatestructs.h"
#include "schedule.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "list.h" 
#include "macros.h"
 
 /* Static methods */
static void __init_sched();
static int taskEnd();
static void spawnChildren();
static int cycle();
static int interrupt();
static void runcpu();
static void killtask(struct task_struct **p);
static void shutdowncpu();
static void badshutdowncpu();
static void cleanuptask(struct thread_info *p);
static void forktask(struct thread_info *thread, struct task_struct *parent);

/* External Prototypes */
struct task_struct *createTask();
struct thread_info *createInfo(const char *name);
int readProfile(char *filename);

/* Macros
 * CLOCK_HZ - Sets the clock speed for the VM
 * The other macros are bookkeeping
 * macros for debugging output, and should
 * be self-exlainitory.
 */ 
#define CLOCK_HZ 500000
#define HZ_TO_MS (1000 / HZ)
#define MS_TO_TICKS(ms) (CLOCK_HZ / 1000 * (ms)) 
#define TICKS_TO_MS(tick) ((long long)((tick) / (long double)CLOCK_HZ * (1000)))

#define LEVEL1(p, q) OUTPUT("", p, q)
#define LEVEL2(p, q) OUTPUT("\t", p, q)
#define LEVEL3(p, q) OUTPUT("\t\t", p, q)
#define OUTPUT(o, p, q) (printf("%s%s/%d/%lldms - %s\n", (o), (p)->thread_info->processName, (p)->time_slice, TICKS_TO_MS(clocktick), (q)))
#define ALERT(p) (printf("###-%s-###\n", (p)))
 
/* Globals 
 * jiffies - A jiffy represents the smallest unit of time that can occur
 *			 between schedule ticks
 * clocktick - The number of cycles the clock has run
 * timer - A "hardware" timer that fires for schedule ticks
 * processID - Next Process ID value
 * rq - A poiner to the runqueue
 * idle - Pointer to the idle task
 * init - Pointer to the init task
 * current - A pointer to the current process in the CPU
 */
long long jiffies = 0;
long long clocktick = 0;
long long timer = 0;
unsigned int processID = 0;
extern struct runqueue *rq;
struct task_struct *idle = NULL;
struct task_struct *init = NULL;
extern struct task_struct *current;

/* Control Data
 * cycletime - The delay between cycles in the VM
 * ranSeed - A random seed for the VM
 * intTimer - The general "IO" interrupt, runs from a timer that is
 *			  is randomly set when a process goes to sleep
 * intWaitTimer - A process timer to dictate when an interactive process
 *				  will go and wait on IO
 * endtime - The time in ms when the VM will shutdown (approximately)
 */
long cycletime = 10;
int ranSeed = 42;
long long intTimer = -1;
long long intWaitTimer = -1;
long endtime = 1;
 
/*-------------- INTERRUPT DATA ---------------*/
/* This section has data specific to our
 * machines "interrupts."
 */
/* Return Values */
#define RESCHEDULE		1

/* This is an IO wait queue */
struct waitlist
{
	struct task_struct *task;
	struct list_head list;
};
 
 /* Wait Queues
  * This is the wait queue for our "IO"
  */
struct list_head intwaitlist;

/*---------------APPLICATION LOGIC------------------*/

/* main
 * Takes a profile to load and run
 */ 
int main(int argc, char *argv[])
{
	if(argc < 2)
	{
		printf("Virtual Scheduler\nUsage: vsch [filename]\n");
		return(1);
	}

	/* Initialize CPU and scheduler */
	__init_sched();

	/* Read in the profile */
	if(!readProfile(argv[1]))
		goto ERROR;
		
	/* Set the random seed we read in */
	srand(ranSeed);
	
	/* Schedule the idle task to "prep" the scheduler */
	ALERT("Starting CPU");
	schedule();
	/* Set first schedule tick timer */
	timer = MS_TO_TICKS(HZ_TO_MS);
	/* Start the CPU */
	runcpu();
	
	/* Clean up from the CPU */
	ALERT("Shutting Down CPU");
	shutdowncpu();
	
	return(0);
	
ERROR:
	/* Cleanup from an error */
	badshutdowncpu();
	shutdowncpu();
	return(1);
}

/* ---------------------- VM FUNCTIONS -------------------*/
/* runcpu
 * This is the primary application loop that simulates
 * our "CPU". This code also controls the checking
 * of interrupts.
 */
static void runcpu()
{
	long long lastMS = 0;
	
	do
	{
		/* Run a single cycle of our application */
		if(cycle())
			goto END_CYCLE;
	INTERRUPT:
		/* Check for interrupts
		 * This routine checks for fired
		 * IO routines or timer ticks
		 */
		switch(interrupt())
		{
			/* Current task signaled for 
			 * reschedule
			 */
			case RESCHEDULE:
				clocktick++;
				schedule();
				goto END_CYCLE;
			break;
		}
		
		/* We've completed a clock cycle,
		 * add a tick.
		 */
		clocktick++;
	END_CYCLE:
	
		/* Check for ending conditions for our simulation */
		if(!init->thread_info->kill && TICKS_TO_MS(clocktick) >= endtime)
		{
			ALERT("Sending Kill Message");
			//isEnding = 1;
			init->thread_info->kill = 1;
		}
		
		/* Flush output and sleep */
		fflush(stdout);
		
		if(TICKS_TO_MS(clocktick) > lastMS)
		{
			lastMS = TICKS_TO_MS(clocktick);
			
			if(cycletime > 0)
				usleep(cycletime);
		}

	}while(rq->nr_running);
}

/* cycle
 * Controls process logic, spawning and sleeping,
 * as well as new process creation and process
 * death.
 */
static int cycle()
{
	struct waitlist *tempwaitlist;
	
	/* Check to see if the task is ending */
	if(taskEnd())
		return(0);

	/* Run logic based on task type */
	switch(current->thread_info->thread_type)
	{
		case INIT:
		break;
		
		/* Interactive tasks need to check to see
		 * if they are going to wait on IO
		 */
		case INTERACTIVE:
			/* Tick the timer for sleeping */
			if(intWaitTimer > 0)
				intWaitTimer--;
				
			/* When timer expires, sleep! */
			if(intWaitTimer == 0)
			{
				intWaitTimer--;
				LEVEL2(current, "Going to Sleep");
				
				/* Add task to wait queue */
				tempwaitlist = (struct waitlist*)malloc(sizeof(struct waitlist));
				INIT_LIST_HEAD(&tempwaitlist->list);
				tempwaitlist->task = current;
				list_add_tail(&tempwaitlist->list, &intwaitlist);
				
				/* Deactivate the task and remove it from the 
				 * scheduler.
				 */
				deactivate_task(current);
				
				
				/* We need to be rescheduled! */
				current->need_reschedule = 1;
			}
		break;
		
		case NONINTERACTIVE:
		break;
	}
	
	/* If no other task has slept on IO, 
	 * set a random time for the next "IO"
	 * event.
	 */
	if(intTimer < 0)
		intTimer = MS_TO_TICKS(rand() % 1000 + 50);
	
	/* Create any children */
	spawnChildren();
	
	return(0);
}

/* interrupt
 * Checks our computers interrupts
 */
static int interrupt()
{	
		struct list_head *listcur, *listnext;
		struct waitlist *tempwaitlist;
		
	/*----------SCHEDULE TICK TIMER-------------*/
		/* Decrement the timer */
		if(timer > 0)
			timer--;
		
		/* Timer Tick! Run the scheduler */
		if(timer <= 0)
		{
			jiffies++;
			timer = MS_TO_TICKS(HZ_TO_MS);
			if(current->array != NULL)
				scheduler_tick(current);
		}

	/*-------IO EVENT TIMER----------*/
		/* Decrement the timer */
		if(intTimer > 0)
			intTimer--;
		
		/* Timer tick! */
		if(intTimer == 0)
		{
			intTimer--;
			listcur = intwaitlist.next;
			
			ALERT("An Interrupt has fired!");
			
			/* Check the IO waitlist to see if 
			 * there are processes sleeping.
			 */
			while(listcur != &intwaitlist)
			{
				tempwaitlist = list_entry(listcur, struct waitlist, list);
				LEVEL2(tempwaitlist->task, "Waking Up from Sleep");
				activate_task(tempwaitlist->task);
				listnext = listcur->next;
				list_del(listcur);
				free(tempwaitlist);
				listcur = listnext;
			}
			
			/* Notify that we need to reschedule! */
			current->need_reschedule = 1;
		}
		
		/* If a task needs rescheduling, alert! */
		if(current->need_reschedule)
			return(RESCHEDULE);
		
	return(0);
}

/*------------------ SYSTEM CALLS --------------------*/
/* context_switch
 * This performs a "context switch" for the
 * scheduler.
 */
void context_switch(struct task_struct *next)
{
	LEVEL1(next, "Switching Process In");
	
	if(TICKS_TO_MS(clocktick) == 8790)
		printf("BREAK\n");
	
	/* If this is an interactive task,
	 * set random chance for sleep.
	 */
	if(next->thread_info->thread_type == INTERACTIVE)
		intWaitTimer = MS_TO_TICKS(rand() % (next->time_slice * HZ / 1000 + 100) + 5);
	
	/* Set new task as current */
	current = next;
}

/* sched_clock
 * Returns the current time (approx)
 * to the scheduler based on
 * jiffies.
 */
unsigned long long sched_clock()
{
	return(JIFFIES_TO_NS(jiffies));
}

/*-------------------Local Methods-------------------*/

/* __init_sched
 * A pre-initialization function. Sets up
 * Initial tasks and runqueue for scheduler
 * before calling user's function to 
 * setup custom queues.
 */
static void __init_sched()
{
	struct task_struct *task;

	/* Create Init Task */
	task = createTask();
	task->thread_info = createInfo("Init");
	task->thread_info->thread_type = INIT;
	task->thread_info->kill_time = -1;

	INIT_LIST_HEAD(&task->run_list);
	INIT_LIST_HEAD(&task->thread_info->list);
	//task->time_slice = task_timeslice(task);

	/* Assign to global pointer for Config */
	init = task;

	/* Initialize Runqueue */
	rq = (struct runqueue*)malloc(sizeof(struct runqueue));
	rq->curr = NULL;
	rq->nr_running = 0;
	rq->nr_switches = 0;
	rq->best_expired_prio = MAX_PRIO;
	rq->expired_timestamp = 0;
	
	/* Initialize Scheduler */
	initschedule(rq, init);
	
	/* Create Idle Task */
	idle = createTask();
	idle->thread_info = createInfo("IDLE");
	processID--;
	current = idle;
	
	/* Prepare List heads */
	INIT_LIST_HEAD(&intwaitlist);
}

/* forktask
 * Creates data structures for a new process being spawned
 * from a parent. Finally, it submits the task to the
 * scheduler.
 */
static void forktask(struct thread_info *thread, struct task_struct *parent)
{
	struct task_struct *task;
	char str[1024];
	
	task = createTask();
	task->thread_info = thread;
	task->thread_info->id = processID++;
	task->thread_info->children = 0;
	task->thread_info->kill = 0;
	
	/* Assigne Thread Name */
	if(parent->thread_info->parent != NULL)
		sprintf(str, "%s:(%s:%d)", parent->thread_info->processName, task->thread_info->processName, task->thread_info->id);
	else
		sprintf(str, "(%s:%d)", task->thread_info->processName, task->thread_info->id);

	free(task->thread_info->processName);
	task->thread_info->processName = (char*)malloc(strlen(str) + 1);
	memcpy(task->thread_info->processName, str, strlen(str) + 1);

	/* If the parent is not INIT, display parent info */
	if(thread->parent != NULL)
		thread->parent->children++;
	
	/* Set NICE value */
	task->static_prio = NICE_TO_PRIO(task->thread_info->niceValue);
	
	/* Alert Creation */
	printf("###-Process: %s has been created-###\n", thread->processName);
	/* Fork process in Scheduler */
	sched_fork(task);
	/* Wake up the task */
	wake_up_new_task(task);
	/* Signal need for schedule call */
	current->need_reschedule = 1;
}

/* taskEnd
 * Checks for an exit signal for the current 
 * running task.
 */
static int taskEnd()
{
	/* Check to see if the time for this process to end
	 * has passed.
	 */
	 if(current->thread_info->kill ||
	    (current->thread_info->parent != NULL && current->thread_info->parent->kill) ||
	    (current->thread_info->kill_time >= 0 && TICKS_TO_MS(clocktick) >= current->thread_info->kill_time)
	   )
	{
		if(!current->thread_info->kill)
			current->thread_info->kill = 1;
		
		if(current->thread_info->children == 0)
		{
			deactivate_task(current);
			killtask(&current);
			return(1);
		}
	}
	
	return(0);
}

/* spawnChildren
 * Spanws children for processes
 */
static void spawnChildren()
{
	struct list_head *child, *next;;
	struct thread_info *temp;

	/* Make sure the current process can spawn */
	if(current->thread_info->spawns)
	{
		/* Run through the list of children to be spawned */
		child = &current->thread_info->list;
		child = child->next;
		while(child != &current->thread_info->list)
		{
			/* If it is time to spawn a child,
			 * spawn that child.
			 */
			temp = list_entry(child, struct thread_info, clist);
			if(MS_TO_TICKS(temp->spawn_time) <= clocktick) 
			{
				forktask(temp, current);
				next = child;
				child = child->next;
				list_del(next);
			}
			else	
				child = child->next;
		}
	}
}

/* killtask
 * Kills the current running task and 
 * removes it from the scheduler.
 */
static void killtask(struct task_struct **p)
{
	struct task_struct *j = *p;

	printf("###-Process: %s is going down-###\n", j->thread_info->processName); 

	/* If task has a parent, decrement the parent's
	 * count of running children.
	 */
	if(j->thread_info->parent != NULL)
		j->thread_info->parent->children--;
	
	/* Free data structures */
	free(j->thread_info->processName);
	free(j->thread_info);
	free(j);
	
	/* Set the idle task in place
	 * of the current one so the
	 * scheduler works correctly.
	 */
	*p = idle;
	j = *p;
	
	/* If there are still tasks to be run, run them. */
	if(rq->nr_running != 0)
		j->need_reschedule = 1;
}

/* shutdowncpu
 * Releases data structures during a normal
 * shutdown.
 */
static void shutdowncpu()
{
	free(idle->thread_info->processName);
	free(idle->thread_info);
	free(idle);
	
	/* Shuts down the scheduler */
	killschedule();
	/* Free the runqueue */
	free(rq);
}

/* badshutdowncpu
 * Frees data structures left in memory when
 * an error occurs.
 */
static void badshutdowncpu()
{
	cleanuptask(init->thread_info);
	
	if(init->thread_info->processName != NULL)
		free(init->thread_info->processName);
	free(init->thread_info);
	free(init);
}

/* cleanuptask
 * A recursive helper function
 * for badshutdowncpu which
 * recursively frees child tasks
 */
static void cleanuptask(struct thread_info *p)
{
	struct list_head *child, *next;
	struct thread_info *temp;

	child = &p->list;
	child = child->next;
	while(child != NULL && child != &p->list)
	{
		temp = list_entry(child, struct thread_info, clist);
		next = child;
		child = child->next;
		list_del(next);
		
		/* Clean up task children */
		cleanuptask(temp);
		
		/* Clean up this task */
		if(temp->processName != NULL)
			free(temp->processName);
		free(temp);
	}
}