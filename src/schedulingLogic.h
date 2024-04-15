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
int CoreWithPID(Computer *computer, int pid);
bool lastProcess(Scheduler *scheduler);
void AddWaitQueue(Scheduler *scheduler, PCB *process);
bool alreadyReadyQueue(Scheduler *scheduler, PCB* process);
void AddReadyQueue(Scheduler *schedule, PCB* process);
int getWaitQueueCount(void);
void AddFirstReadyQueue(Scheduler *scheduler, PCB* process);

/* -------------------------- init/free functions -------------------------- */

Scheduler *initScheduler(SchedulingAlgorithm **readyQueueAlgorithms, int readyQueueCount, int maxQueue);
void freeScheduler(Scheduler *scheduler);

/* -------------------------- scheduling functions ------------------------- */
void FCFSff(Computer *computer, int switchindelay[], int switchoutdelay[], int InterruptPID, bool InterruptHandlerFinished);
void PRIORITYff(Computer *computer, int switchindelay[], int switchoutdelay[], int InterruptPID, bool InterruptHandlerFinished);
void SJFff(Computer *computer, int switchindelay[], int switchoutdelay[], Workload* workload, int InterruptPID, bool InterruptHandlerFinished);
void RRff(Computer *computer, int switchindelay[], int switchoutdelay[], int InterruptPID, bool InterruptHandlerFinished);

#endif // schedulingLogic_h
