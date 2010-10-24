#ifndef SCHEDULE_H
#define SCHEDULE_H

#include "macros.h"
#include "list.h"

struct thread_info;

/* ------------- This is modified by the programmer ------------ */
/* sched_array is the primary data structure used by the scheduler.
 * We have left it to be modified by you so that you may 
 * implement any type of scheduler that you want.
 */
struct sched_array {
	struct list_head array;
};

/* ---------------- Do NOT Touch -------------- */
/* Sleep Types */
enum sleep_type
{
	SLEEP_NORMAL,
	SLEEP_NONINTERACTIVE,
	SLEEP_INTERACTIVE,
	SLEEP_INTERRUPTED
};

/* task_struct */
struct task_struct
{
	struct thread_info *thread_info;			/* Information about the thread */
	int prio, static_prio, normal_prio;			/* Priority values */	
	unsigned long sleep_avg;					/* The average time the task
												   has been sleeping */
	unsigned long long last_ran;				/* Timestamp for when the
												   task was last run. */
	unsigned long long timestamp;				/* A timestamp value */
	unsigned long long sched_time;				/* The amount of time task
												   was waiting to be scheduled */
	unsigned int time_slice, first_time_slice;	/* Timeslice values */
	struct list_head run_list;					/* Linked list for sched_array */
	struct sched_array *array;					/* The sched_array the task is in */
	enum sleep_type sleep_type;					/* What type of sleep task is in */
	int need_reschedule;						/* Flag, set if task needs to
												   have schedule called */
};

/* runqueue */
struct runqueue {
    unsigned long    nr_running;				/* number of runnable tasks */
    unsigned long    nr_switches;				/* number of contextswitches */
    unsigned long    expired_timestamp;			/* time of last array swap */
	unsigned long long most_recent_timestamp;	/* The last time schedule was run */
    struct task_struct *curr;					/* the currently running task */
    struct sched_array  *active;				/* pointer to the active priority
												   array */
    struct sched_array  *expired;				/* pointer to the expired
											       priority array */
    struct sched_array  arrays[2];				/* the actual priority arrays */
	int best_expired_prio;						/* The highest priority that has
												 * expired thus far */
};

/*----------------------- System Calls ------------------------------*/
/* These calls are provided by the VM for your
 * convenience, and mimic system calls provided
 * normally by Linux
 */
void context_switch(struct task_struct *next); 
unsigned long long sched_clock();

/*------------------YOU MAY EDIT BELOW THIS LINE---------------------*/
/*------------------- User Defined Functions -------------------------*/
/*-------------These functions MUST be defined for the VM-------------*/
void initschedule(struct runqueue *newrq, struct task_struct *seedTask);
void killschedule();
void schedule();
void activate_task(struct task_struct *p);
void deactivate_task(struct task_struct *p);
void __activate_task(struct task_struct *p);
void scheduler_tick(struct task_struct *p);
void sched_fork(struct task_struct *p);
void wake_up_new_task(struct task_struct *p);

/*------------These functions are not necessary, but used----------------*
 *------------by linux normally for scheduling           ----------------*/
void enqueue_task(struct task_struct *p, struct sched_array *array);
void dequeue_task(struct task_struct *p, struct sched_array *array);

#endif
