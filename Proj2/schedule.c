/* schedule.c
 * This file contains the primary logic for the 
 * scheduler.
 */
//CS 411 Project 2
//Matt Thomas, Sarah Clisby, Matt Martinson, Ian Crawford
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

    newrq->nr_running = 0;
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
	if( task->time_slice <= 0 )
            {
	    task->time_slice = NEWTASKSLICE;
            task->first_time_slice = NEWTASKSLICE;           
	    }	
	else if( task->time_slice < rem_time )
		{
 		    next = task;
                    rem_time = task->time_slice;
 	        //printf( "small\n" );
                next->need_reschedule = 0;
		}
        }
    //if( next != NULL && next != rq->curr )
	//{            	
	    rq->nr_switches++;
            context_switch( next );
            rq->curr = next;
            next->need_reschedule = 0;
	//}
}


/* enqueue_task
 * Enqeueus a task in the passed sched_array
 */
void enqueue_task(struct task_struct *p, struct sched_array *array)
{
//    printf( "Calling enqueue_task\n" );
    list_add_tail( &p->run_list, &array->array );
    p->array = array;
    rq->nr_running++;
}

/* dequeue_task
 * Removes a task from the passed sched_array
 */
void dequeue_task(struct task_struct *p, struct sched_array *array)
{
//    printf( "Calling dequue_task\n" );

    list_del( &p->run_list );
    p->array = NULL;   
    rq->nr_running--;
}

/* sched_fork
 * Sets up schedule info for a newly forked task
 */
void sched_fork(struct task_struct *p)
{
//    printf( "forking\n" );	
    int new_slice = rq->curr->first_time_slice / 2;
    rq->curr->first_time_slice = new_slice;
    rq->curr->time_slice = new_slice;
    p->first_time_slice = new_slice;
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
  // 	p->time_slice = NEWTASKSLICE;
  //      p->first_time_slice = NEWTASKSLICE;
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
    __activate_task( p );
    if( current->time_slice < p->time_slice )
    {
        p->need_reschedule = 1;
    }
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
    if( p->time_slice < rq->curr->time_slice )
    {   
	p->need_reschedule = 1;
    }
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
