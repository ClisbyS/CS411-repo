/* cpuinit.c
 * Contains a few *large* helper functions
 * for initializing the virtual machine.
 * The main guts here is the parser,
 * which is fairly large and parses
 * the CPU profiles.
 */
 
#include <stdio.h>
#include <stdlib.h>
#include "schedule.h"
#include "privatestructs.h"
#include <string.h>

/* Parse Data */
#define CSIZE 12
#define TSIZE 2

/* The various parse options */
char *coptions[CSIZE] = 	{
"CYCLE_TIME",
"NEWPROCESS",
"ENDPROCESS",
"SPAWNTIME", 
"NAME",
"TYPE",
"SEED", 
"ENDTIME",
"KILLTIME",
"NICE",
"SPAWN",
"ENDSPAWN"
};

/* Type Options */
char *ttype[TSIZE] = {
"INTERACTIVE",
"NONINTERACTIVE"
};

int tint[TSIZE] = {
INTERACTIVE,
NONINTERACTIVE
};

/* Static prototypes */
static int readint(FILE *fp);
static void readstring(FILE *fp, char **str);

/* External References */
extern struct task_struct *idle;
extern struct task_struct *init;
extern int processID;
extern int cycletime;
extern int ranSeed;
extern long endtime;

/* createTask 
 * Helper method that creates and zeros
 * a task_struct.
 */
struct task_struct *createTask()
{
	struct task_struct *task;
	
	task = (struct task_struct*)malloc(sizeof(struct task_struct));
	task->thread_info = (struct thread_info*)malloc(sizeof(struct thread_info));
	task->static_prio = NICE_TO_PRIO(0);
	task->prio = 0;
	task->sleep_avg = 0;
	task->time_slice = 0;
	task->first_time_slice = 0;
	task->need_reschedule = 0;
	task->last_ran = 0;
	task->timestamp = 0;
	
	return(task);
}

/* createInfo
 * Helper method that creates and zeros
 * a thread_info struct.
 */
struct thread_info *createInfo(const char *name)
{
	struct thread_info *thread_info;
	char pname[1024];
	
	thread_info = (struct thread_info*)malloc(sizeof(struct thread_info));
	thread_info->id = processID++;
	thread_info->parent = NULL;
	thread_info->spawns = 0;
	thread_info->niceValue = 0;
	thread_info->kill = 0;
	thread_info->thread_type = -1;
	thread_info->children = 0;
	thread_info->type_struct = NULL;
	sprintf(pname, "%d", thread_info->id);
	thread_info->processName = (char *)malloc(strlen(name) + strlen(pname) + 4);
	sprintf(thread_info->processName, "(%s:%s)", name, pname);
	
	return(thread_info);
}

/* readProfile
 * Main body of parser.
 */
int readProfile(char *filename)
{
	int i, j, offset;
	char copt[50];
	char *temp;
	FILE *fp;
	int children = 0;
	struct thread_info *newtask = NULL;
	struct thread_info *old;
	struct thread_info *top = init->thread_info;
	top->spawns = 1;
	
	if((fp = fopen(filename, "r")) == NULL)
	{
		printf("ERROR: profile %s not found\n", filename);
		return(0);
	}

	while(!feof(fp))
	{
		/* Eat Whitespace */
		WHITESPACE:
			if(!fread(copt, 1, 1, fp))
				continue;
			if(isspace(*copt))
				goto WHITESPACE;

		/* Ignore whitespace */
		if(*copt == ';')
		{
			while(*copt != '\n')
				if(!fread(copt, 1, 1, fp))
					continue;
			if(*copt != '#')
				continue;
		}

		if(*copt != '#')
		{
			printf("Missing '#' at beginning of command\n");
			return(0);
		}

		/* Read option */
		i = 0;
		while(!feof(fp))
		{
			if(i >= 48)
				return(0);

			if(!fread((copt + i), 1, 1, fp))
				goto WHITESPACE;
			if(isspace(*(copt + i)))
				break;
			i++;
		};

		*(copt + i) = '\0';

		/* Parse Option */
		offset = -1;
		for(i = 0; i < CSIZE; i++)
		{
			/*printf("Comparing %s to %s\n", copt, coptions[i]);*/

			if(strcmp(copt, coptions[i]) == 0)
			{
				offset = i;
				break;
			}
			
		}

		if(offset == -1)
		{
			printf("Command %s is unknown\n", copt);
			return(0);
		}

		/* Eat Whitespace */
		WHITESPACE_VAL:
			if(!fread(copt, 1, 1, fp))
				continue;
			if(isspace(*copt))
				goto WHITESPACE_VAL;

		fseek(fp, -1, SEEK_CUR);

		/* Parse Value
		 * Switched off of parse array
		 */
		switch(i)
		{
			/* Read Int */
			case 0: /* Cycle Delay */
				cycletime = readint(fp);
			break;
			
			case 1: /* New Process */
				newtask = (struct thread_info*)malloc(sizeof(struct thread_info));
				newtask->kill_time = -1;
				newtask->niceValue = 0;
				newtask->parent = top;
				INIT_LIST_HEAD(&newtask->list);
				list_add_tail(&newtask->clist, &top->list);
			break;
			
			case 2: /* End Process */
				newtask = NULL;
			break;
			
			case 3: /* Spawn Time */
				newtask->spawn_time = readint(fp);
			break;
				
			case 4: /* Name */
				readstring(fp, &newtask->processName);
			break;
			
			case 5: /* Type */
				readstring(fp, &temp);
				offset = 0;
				for(j = 0; j < TSIZE; j++)
				{
					if(strcmp(ttype[j], temp) == 0)
					{
						newtask->thread_type = tint[j];
						offset = 1;
					}
				}
				
				if(!offset)
					printf("Bad type!\n");
			break;
			
			case 6: /* randomd seed */
				ranSeed = readint(fp);
			break;
			
			case 7: /* endtime */
				endtime = readint(fp);
			break;
			
			case 8: /* Kill time */
				newtask->kill_time = readint(fp);
			break;
			
			case 9: /* Nice Value */
				newtask->niceValue = readint(fp);
				if(newtask->niceValue < -19 || newtask->niceValue > 20)
					newtask->niceValue = 0;
			break;
			
			case 10: /* SPAWN */
				children++;
				top = newtask;
				top->spawns = 1;
			break;
			
			case 11: /* ENDSPAWN */
				children--;
				old = top;
				top = top->parent;
				newtask = old;
			break;
			
			default:
				return(0);
			break;
		}
	}
	

	fclose(fp);

	if(children)
		return(0);
		
	return(1);
}

/* readint
 * Helper function. Reads an integer from
 * the file.
 */
static int readint(FILE *fp)
{
	int val;

	fscanf(fp, "%d", &val);
	
	return(val);
}

/* readstring
 * Helper function. Reads a 
 * string from the file.
 */
static void readstring(FILE *fp, char **str)
{
	int i = 0, j = 0;
	char c;	

	*str = NULL;

	while(!feof(fp))
	{
		if(!fread(&c, 1, 1, fp))
			continue;
		if(isspace(c))
			break;

		i++;
	}
	
	if(i == 0)
		return;

	*str = (char*)malloc(sizeof(char) * (i + 1));

	fseek(fp, -i - 1, SEEK_CUR);
	
	for(j = 0; j < i; j++)
		fread((*str + j), 1, 1, fp);
	
	*(*str + i) = '\0';

	return;
}
