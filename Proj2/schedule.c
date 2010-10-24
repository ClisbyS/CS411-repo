/* schedule.c
 * This file contains the primary logic for the 
 * scheduler.
 */
#include "schedule.h"
#include "macros.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NEWTASKSLICE (NS_TO_JIFFIES(100000000))

/* Local Globals
 * rq - This is a pointer to the runqueue that the scheduler uses.
 * current - A pointer to the current running task.
 */
struct runqueue *rq;
struct task_struct *current;

/* External Globals
 * jiffies - A discrete unit of time used for scheduling.
 *			 There are HZ jiffies in a second, (HZ is 
 *			 declared in macros.h), and is usually
 *			 1 or 10 milliseconds.
 */
extern long long jiffies;


/*-----------------Initilization/Shutdown Code-------------------*/
/* This code is not used by the scheduler, but by the virtual machine
 * to setup and destroy the scheduler cleanly.
 */
 
 /* initscheduler
  * Sets up and allocates memory for the scheduler, as well
  * as sets initial values. This function should also
  * set the initial effective priority for the "seed" task 
  * and enqueu it in the scheduler.
  * INPUT:
  * newrq - A pointer to an allocated rq to assign to your
  *			local rq.
  * seedTask - A pointer to a task to seed the scheduler and start
  * the simulation.
  */

void initschedule(struct runqueue *newrq, struct task_struct *seedTask)
{
    printf( "Begin init.\n" );
    struct sched_array* actual;

    newrq->nr_running = 1;
    newrq->nr_switches = 0;

    actual = malloc( 2 *  sizeof( struct sched_array ) );
    newrq->arrays[ 0 ] = actual[ 0 ];

    newrq->active = &newrq->arrays[ 0 ];
    INIT_LIST_HEAD( &newrq->arrays[ 0 ].array );

    enqueue_task( seedTask, newrq->active );

    seedTask->need_reschedule = 1;

    seedTask->first_time_slice = NEWTASKSLICE;
    seedTask->time_slice = NEWTASKSLICE;

    rq = newrq;
}

/* killschedule
 * This function should free any memory that 
 * was allocated when setting up the runqueu.
 * It SHOULD NOT free the runqueue itself.
 */
void killschedule()
{
    printf( "Begin Kill.\n" );

    //free( rq->arrays );

}

/*-------------Scheduler Code Goes Below------------*/
/* This is the beginning of the actual scheduling logic */

/* schedule
 * Gets the next task with the shortest runtime(time slice) remaining
 */
void schedule()
{
    //printf( "Calling schedule\n" );

    struct list_head* pos = NULL;

    struct task_struct *task = NULL;
    struct task_struct *next = NULL;
    
    unsigned int rem_time = -1;
  
    list_for_each( pos, &rq->active->array )
        {
        task = list_entry( pos, struct task_struct, run_list );
        if( task->need_reschedule )
	    {
   	   	if( task->time_slice <= 0 )
		{
		    task->time_slice = task->first_time_slice;
                    task->need_reschedule = 1;
		}
		else if( task->time_slice < rem_time )
		{
 		    next = task;
                    rem_time = task->time_slice;
		}
	    }
        }
    if( next != NULL && next != current )
	{	
	    rq->nr_switches++;
            context_switch( next );
            rq->curr = next;
            next->need_reschedule = 0;
	}
}


/* enqueue_task
 * Enqeueus a task in the passed sched_array
 */
void enqueue_task(struct task_struct *p, struct sched_array *array)
{
    //printf( "Calling enqueue_task\n" );
    list_add( &p->run_list, &array->array );
    p->array = array;
    rq->nr_running++;
}

/* dequeue_task
 * Removes a task from the passed sched_array
 */
void dequeue_task(struct task_struct *p, struct sched_array *array)
{
    //printf( "Calling dequue_task\n" );

    list_del( &p->run_list );
    p->array = NULL;   
    rq->nr_running--;
}

/* sched_fork
 * Sets up schedule info for a newly forked task
 */
void sched_fork(struct task_struct *p)
{	
    //printf( "Calling sched_fork\n" );
    //printf( "%d\n", sched_clock() );
    p->first_time_slice = current->first_time_slice / 2;
    p->time_slice = p->first_time_slice;
}

/* scheduler_tick
 * Updates information and priority
 * for the task that is currently running.
 */
void scheduler_tick(struct task_struct *p)
{	
    p->time_slice--;
    if( p->time_slice <= 0 )
    {
        p->need_reschedule = 1;
    }
}

/* wake_up_new_task
 * Prepares information for a task
 * that is waking up for the first time
 * (being created).
 * Also handles preemption, e.g. decides 
 * whether or not the current task should
 * call scheduler to allow for this one to run
 */
void wake_up_new_task(struct task_struct *p)
{	

    enqueue_task( p, rq->active );
    p->need_reschedule = 1;
}

/* __activate_task
 * Activates the task in the scheduler
 * by adding it to the active array.
 */
void __activate_task(struct task_struct *p)
{
    enqueue_task( p, rq->active );
}

/* activate_task
 * Activates a task that is being woken-up
 * from sleeping.
 */
void activate_task(struct task_struct *p)
{	
    //printf( "Calling activate_task\n" );
    __activate_task( p );
    p->need_reschedule = 1;
}

/* deactivate_task
 * Removes a running task from the scheduler to
 * put it to sleep.
 */
void deactivate_task(struct task_struct *p)
{
    //printf( "Calling deactivate_task\n" );
    p->need_reschedule = 0;
    dequeue_task( p , rq->active);
    //printf( "deactive done\n" );
}
