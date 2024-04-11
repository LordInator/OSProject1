#ifndef schedulingLogic_h
#define schedulingLogic_h

#include <stdbool.h>

#include "graph.h"
#include "stats.h"
#include "simulation.h"

// Cannot include computer.h because of circular dependency
// -> forward declaration of Computer
typedef struct Computer_t Computer;


/* ---------------------------- Scheduler struct ---------------------------- */

typedef struct Scheduler_t Scheduler;

/* -------------------------- getters and setters -------------------------- */

void AddWaitQueue(Scheduler *scheduler, PCB *process);
bool alreadyReadyQueue(Scheduler *scheduler, PCB* process);
void AddReadyQueue(Scheduler *schedule, PCB* process);
int getWaitQueueCount(void);

/* -------------------------- init/free functions -------------------------- */

Scheduler *initScheduler(SchedulingAlgorithm **readyQueueAlgorithms, int readyQueueCount, int maxQueue);
void freeScheduler(Scheduler *scheduler);


/* -------------------------- scheduling functions ------------------------- */
void FCFSff(Computer *computer, int switchindelay, int switchoutdelay);

#endif // schedulingLogic_h
