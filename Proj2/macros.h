#ifndef MACROS_H
#define MACROS_H

#define HZ 100

#define MAX_PRIO 1
#define NICE_TO_PRIO(p) (0)

#define JIFFIES_TO_NS(TIME) ((TIME) * (1000000000 / HZ))
#define NS_TO_JIFFIES(TIME) ((TIME) / (1000000000 / HZ))

#endif