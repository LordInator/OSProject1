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

/* -------------------------- getters and setters -------------------------- */

bool alreadyReadyQueue(Scheduler *scheduler, PCB* process){
    for(int i = 0; i < scheduler->IndexReady; i++){
        if(scheduler->readyQueue[i]->pid == process->pid){
            return true;
        }
    }
    return false;
}

void AddReadyQueue(Scheduler *schedule, PCB* process){
    schedule->readyQueue[schedule->IndexReady] = process;
    schedule->IndexReady++;
}

void reduceReadyQueue(Scheduler *scheduler){
    for(int i=0; i<scheduler->IndexReady; i++){
        scheduler->readyQueue[i] = scheduler->readyQueue[i+1];
    }
    scheduler->IndexReady--;
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

void FCFSff(Computer *computer, int time, ProcessGraph *graph, AllStats *stats){
    //char *strings[] = {"READY", "RUNNING", "WAITING", "TERMINATED"};
    if(computer->cpu->cores[0]->state == IDLE && computer->scheduler->IndexReady > 0){
        computer->cpu->cores[0]->process = computer->scheduler->readyQueue[0];
        computer->cpu->cores[0]->process->state = RUNNING;
        computer->cpu->cores[0]->state = NOTIDLE;
        printf("%d \n", computer->scheduler->IndexReady);
        reduceReadyQueue(computer->scheduler);
        printf("%d \n", computer->scheduler->IndexReady);

        addProcessEventToGraph(graph, computer->cpu->cores[0]->process->pid, time, RUNNING, 0);
        for(int i=0; i<computer->scheduler->IndexReady; i++){
            addProcessEventToGraph(graph, computer->scheduler->readyQueue[i]->pid, time, READY, NO_CORE);
        }
    }
}