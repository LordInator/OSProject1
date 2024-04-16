// This is where you should implement most of your code.
// You will have to add function declarations in the header file as well to be
// able to call them from simulation.c.

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

#include "process.h"
#include "computer.h"
#include "schedulingLogic.h"
#include "utils.h"
#include "schedulingAlgorithms.h"
#include "simulation.h"

#define NB_WAIT_QUEUES 1

/* --------------------------- struct definitions -------------------------- */

struct Scheduler_t
{
    // This is not the ready queues, but the ready queue algorithms
    SchedulingAlgorithm **readyQueueAlgorithms;
    int readyQueueCount;
    PCB **readyQueue;
    PCB **waitQueue;
    int maxQueue;
    int IndexReady;
    int IndexWaiting;
};

/* ---------------------------- static functions --------------------------- */

static int compareProcessPriority(const void *a, const void *b)
{
    const PCB *infoA = *(const PCB **)a;
    const PCB *infoB = *(const PCB **)b;

    if (infoA->priority < infoB->priority)
    {
        return -1;
    }
    else if (infoA->priority > infoB->priority)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

/* -------------------------- getters and setters -------------------------- */

int CoreWithPID(Computer *computer, int pid){
    for(int i=0; i<computer->cpu->coreCount; i++){
        if(computer->cpu->cores[i]->process->pid == pid)
            return i;
    }
    return 0;
}


bool lastProcess(Scheduler *scheduler){
    if(scheduler->IndexReady <= 0){
        return true;
    }
    return false;
}

void AddWaitQueue(Scheduler *scheduler, PCB *process){
    process->state = WAITING;
    scheduler->waitQueue[scheduler->IndexWaiting] = process;
    scheduler->IndexWaiting++;
}

bool alreadyReadyQueue(Scheduler *scheduler, PCB* process){
    for(int i = 0; i < scheduler->IndexReady; i++){
        if(scheduler->readyQueue[i]->pid == process->pid){
            return true;
        }
    }
    return false;
}

void AddFirstReadyQueue(Scheduler *scheduler, PCB* process){
    process->state = READY;
    for(int i = scheduler->IndexReady; i > 0; i--){
        scheduler->readyQueue[i] = scheduler->readyQueue[i-1];
    }
    scheduler->readyQueue[0] = process;
    scheduler->IndexReady++;
}

void AddReadyQueue(Scheduler *schedule, PCB* process){
    process->state = READY;
    schedule->readyQueue[schedule->IndexReady] = process;
    schedule->IndexReady++;
}

void reduceReadyQueue(Scheduler *scheduler){
    for(int i=0; (i+1)<scheduler->IndexReady; i++){
        scheduler->readyQueue[i] = scheduler->readyQueue[i+1];
    }
    scheduler->IndexReady--;
}

void reduceReadyQueueSJF(Scheduler *scheduler, int index){
    for(int i=index; (i+1)<scheduler->IndexReady; i++){
        scheduler->readyQueue[i] = scheduler->readyQueue[i+1];
    }
    scheduler->IndexReady--;
}

int getIndexReady(Scheduler *scheduler){
    return scheduler->IndexReady;
}

int getWaitQueueCount(void)
{
    return NB_WAIT_QUEUES;
}

/* -------------------------- init/free functions -------------------------- */

Scheduler *initScheduler(SchedulingAlgorithm **readyQueueAlgorithms, int readyQueueCount, int maxQueue)
{
    Scheduler *scheduler = malloc(sizeof(Scheduler));
    if (!scheduler)
    {
        return NULL;
    }

    scheduler->readyQueueAlgorithms = readyQueueAlgorithms;
    scheduler->readyQueueCount = readyQueueCount;
    scheduler->readyQueue = (PCB**)malloc(sizeof(PCB*)*maxQueue);
    scheduler->waitQueue = (PCB**)malloc(sizeof(PCB*)*maxQueue);
    scheduler->maxQueue = maxQueue;
    scheduler->IndexReady = 0;
    scheduler->IndexWaiting = 0;
    return scheduler;
}

void freeScheduler(Scheduler *scheduler)
{
    for (int i = 0; i < scheduler->readyQueueCount; i++)
    {
        free(scheduler->readyQueueAlgorithms[i]);
    }
    free(scheduler->readyQueue);
    free(scheduler->waitQueue);
    free(scheduler->readyQueueAlgorithms);
    free(scheduler);
}

/* -------------------------- scheduling functions ------------------------- */

void FCFSff(Computer *computer, int switchindelay[], int switchoutdelay[], int InterruptPID, bool InterruptHandlerFinished){
    /*for(int i = 0; i < computer->scheduler->IndexReady; i++){
        printf("ReadyQueue : %d \n", computer->scheduler->readyQueue[i]->pid);
    }*/

    /*InterruptHandler finished => Scheduler put process that was on disk back in readyQueue*/
    if(InterruptHandlerFinished == true){
        AddReadyQueue(computer->scheduler, computer->disk->processIO);
    }

    /*IO event // Scheduler put a process on disk if idle*/
    if(InterruptPID != 0){
        if(computer->disk->isIdle == true){
            int indexCore = CoreWithPID(computer, InterruptPID);
            computer->disk->processIO = computer->cpu->cores[indexCore]->process;
            computer->disk->isIdle = false;
            computer->cpu->cores[indexCore]->state = IDLE;
            AddWaitQueue(computer->scheduler, computer->disk->processIO); //also modify process state
        }
    }

    //printf("count %d \n", computer->scheduler->IndexReady);
    for(int i=0; i<computer->cpu->coreCount; i++){
    /*Core 0 idle && atleast 1 process in readyQueue*/
        if(computer->cpu->cores[i]->state == IDLE && computer->scheduler->IndexReady > 0){
            //printf("delays : %d & %d  && index %d \n", switchindelay[i], switchoutdelay[i], i);
            /*Skip for switch-in delay*/
            if(switchindelay[i] == 0 && switchoutdelay[i] == 0){
                //printf("comp : %d \n", computer->scheduler->readyQueue[0]->pid);
                computer->cpu->cores[i]->process = computer->scheduler->readyQueue[0];
                computer->cpu->cores[i]->process->state = RUNNING;
                computer->cpu->cores[i]->state = NOTIDLE;
                reduceReadyQueue(computer->scheduler);
            }
        }
    }
}

void PRIORITYff(Computer *computer, int switchindelay[], int switchoutdelay[], int InterruptPID, bool InterruptHandlerFinished){

    /*InterruptHandler finished => Scheduler put process that was on disk back in readyQueue*/
    if(InterruptHandlerFinished == true){
        AddReadyQueue(computer->scheduler, computer->disk->processIO);
    }

    qsort(computer->scheduler->readyQueue, computer->scheduler->IndexReady, sizeof(PCB *), compareProcessPriority);

    /*IO event => Scheduler put a process on disk if idle*/
    if(InterruptPID != 0){
        if(computer->disk->isIdle == true){
            int indexCore = CoreWithPID(computer, InterruptPID);
            computer->disk->processIO = computer->cpu->cores[indexCore]->process;
            computer->disk->isIdle = false;
            computer->cpu->cores[indexCore]->state = IDLE;
            AddWaitQueue(computer->scheduler, computer->disk->processIO); //also modify process state
        }
    }

    for(int i=0; i<computer->cpu->coreCount; i++){
    /*Core 0 idle && atleast 1 process in readyQueue*/
        if(computer->cpu->cores[i]->state == IDLE && computer->scheduler->IndexReady > 0){
            /*Skip for switch-in delay*/
            if(switchindelay[i] == 0 && switchoutdelay[i] == 0){
                computer->cpu->cores[i]->process = computer->scheduler->readyQueue[0];
                computer->cpu->cores[i]->process->state = RUNNING;
                computer->cpu->cores[i]->state = NOTIDLE;
                reduceReadyQueue(computer->scheduler);
            }
        }
    }
}

void SJFff(Computer *computer, int switchindelay[], int switchoutdelay[], Workload* workload, int InterruptPID, bool InterruptHandlerFinished){

    /*InterruptHandler finished => Scheduler put process that was on disk back in readyQueue*/
    if(InterruptHandlerFinished == true){
        AddReadyQueue(computer->scheduler, computer->disk->processIO);
    }

    /*IO event // Scheduler put a process on disk if idle*/
    if(InterruptPID != 0){
        if(computer->disk->isIdle == true){
            int indexCore = CoreWithPID(computer, InterruptPID);
            computer->disk->processIO = computer->cpu->cores[indexCore]->process;
            computer->disk->isIdle = false;
            computer->cpu->cores[indexCore]->state = IDLE;
            AddWaitQueue(computer->scheduler, computer->disk->processIO); //also modify process state
        }
    }

    for(int i=0; i<computer->cpu->coreCount; i++){
        int indexLowest = 0;
        int LowestScore = 999;
        for(int j=0; j<computer->scheduler->IndexReady; j++){
            if(LowestScore > getProcessCurEventTimeLeft(workload, computer->scheduler->readyQueue[j]->pid) && getProcessCurEventTimeLeft(workload, computer->scheduler->readyQueue[j]->pid) > 0){
                LowestScore = getProcessCurEventTimeLeft(workload, computer->scheduler->readyQueue[j]->pid);
                indexLowest = j;
            }
        }

        /*Core 0 idle && atleast 1 process in readyQueue*/
        if(computer->cpu->cores[i]->state == IDLE && computer->scheduler->IndexReady > 0){
            /*Skip for switch-in delay*/
            if(switchindelay[i] == 0 && switchoutdelay[i] == 0){
                computer->cpu->cores[i]->process = computer->scheduler->readyQueue[indexLowest];
                computer->cpu->cores[i]->process->state = RUNNING;
                computer->cpu->cores[i]->state = NOTIDLE;
                reduceReadyQueueSJF(computer->scheduler, indexLowest);
            }
        }
    }
}

void RRff(Computer *computer, int switchindelay[], int switchoutdelay[], int InterruptPID, bool InterruptHandlerFinished){

    /*InterruptHandler finished => Scheduler put process that was on disk back in readyQueue*/
    if(InterruptHandlerFinished == true){
        AddReadyQueue(computer->scheduler, computer->disk->processIO);
    }

    /*IO event // Scheduler put a process on disk if idle*/
    if(InterruptPID != 0){
        if(computer->disk->isIdle == true){
            int indexCore = CoreWithPID(computer, InterruptPID);
            computer->disk->processIO = computer->cpu->cores[indexCore]->process;
            computer->disk->isIdle = false;
            computer->cpu->cores[indexCore]->state = IDLE;
            AddWaitQueue(computer->scheduler, computer->disk->processIO); //also modify process state
        }
    }

    for(int i=0; i<computer->cpu->coreCount; i++){
        if(computer->cpu->cores[i]->state == IDLE && computer->scheduler->IndexReady > 0){
            /*Skip for switch-in delay*/
            if(switchindelay[i] == 0 && switchoutdelay[i] == 0){
                //printf("comp : %d \n", computer->scheduler->readyQueue[0]->pid);
                computer->cpu->cores[i]->process = computer->scheduler->readyQueue[0];
                computer->cpu->cores[i]->process->state = RUNNING;
                computer->cpu->cores[i]->state = NOTIDLE;
                reduceReadyQueue(computer->scheduler);
            }
        }
    }
}