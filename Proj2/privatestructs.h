/* privatestruct
 * This file contains data structures that are 
 * exclusively for the VM, and should not be
 * accessible to the scheduler.
 */

#ifndef PRIVATESTRUCTS_H
#define PRIVATESTRUCTS_H

#include "list.h"

// Task Types
#define INIT				0
#define INTERACTIVE			1
#define NONINTERACTIVE		2

struct thread_info
{
	int id;
	int spawn_time;
	int kill_time;
	int niceValue;
	int spawns;
	int children;
	int kill;
	int thread_type;
	void *type_struct;
	char *processName;
	struct thread_info *parent;
	struct list_head list;
	struct list_head clist;
};

#endif