// In this file, you should modify the main loop inside launchSimulation and
// use the workload structure (either directly or through the getters and
// setters).

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "simulation.h"
#include "process.h"
#include "utils.h"
#include "computer.h"
#include "schedulingLogic.h"

#define MAX_CHAR_PER_LINE 500


/* --------------------------- struct definitions -------------------------- */

/**
 * The ProcessEvent strcut represent processes events as they are in the input
 * file (CPU or IO). They are represented as a linked list where each event
 * points to the next one.
 */
typedef struct ProcessEvent_t ProcessEvent;
/**
 * The ProcessSimulationInfo struct contains all the input file information
 * and the advancement time of a particular process. The Workload struct
 * contains an array of ProcessSimulationInfo.
 */
typedef struct ProcessSimulationInfo_t ProcessSimulationInfo;

typedef enum
{
    CPU_BURST,
    IO_BURST, // For simplicity, we'll consider that IO bursts are blocking (among themselves)
} ProcessEventType;

struct ProcessEvent_t
{
    ProcessEventType type;
    int time; // Time at which the event occurs. /!\ time relative to the process
    ProcessEvent *nextEvent; // Pointer to the next event in the queue
};

struct ProcessSimulationInfo_t
{
    PCB *pcb;
    int startTime;
    int processDuration; // CPU + IO !
    int advancementTime; // CPU + IO !
    ProcessEvent *nextEvent; // Pointer to the next event after the current one
};

struct Workload_t
{
    ProcessSimulationInfo **processesInfo;
    int nbProcesses;
};



/* ---------------------------- static functions --------------------------- */

/**
 * Return the index of the process with the given pid in the array of processes
 * inside the workload.
 *
 * @param workload: the workload
 * @param pid: the pid of the process
 *
 * @return the index of the process in the workload
 */
static int getProcessIndex(Workload *workload, int pid);

/**
 * Set the advancement time of the process with the given pid in the workload.
 *
 * @param workload: the workload
 * @param pid: the pid of the process
 * @param advancementTime: the new advancement time
 */
static void setProcessAdvancementTime(Workload *workload, int pid, int advancementTime);

/*
 * Returns true if all processes in the workload have finished
 * (advancementTime == processDuration).
 *
 * @param workload: the workload
 * @return true if all processes have finished, false otherwise
 */
static bool workloadOver(const Workload *workload);
static bool runningProcess(const Workload *workload);

static void addAllProcessesToStats(AllStats *stats, Workload *workload);

/**
 * Compare function used in qsort before the main simulation loop. If you don't
 * use qsort, you can remove this function.
 *
 * @param a, b: pointers to ProcessSimulationInfo to compare
 *
 * @return < 0 if process a is first, > 0 if b is first, = 0 if they start at
 *         the same time
 */
static int compareProcessStartTime(const void *a, const void *b);

/* -------------------------- getters and setters -------------------------- */

int getProcessCount(const Workload *workload)
{
    return workload->nbProcesses;
}

int getPIDFromWorkload(Workload *workload, int index)
{
    return workload->processesInfo[index]->pcb->pid;
}

int getProcessStartTime(Workload *workload, int pid)
{
    for (int i = 0; i < workload->nbProcesses; i++)
    {
        if (getPIDFromWorkload(workload, i) == pid)
        {
            return workload->processesInfo[i]->startTime;
        }
    }
    return -1;
}

int getProcessDuration(Workload *workload, int pid)
{
    for (int i = 0; i < workload->nbProcesses; i++)
    {
        if (getPIDFromWorkload(workload, i) == pid)
        {
            return workload->processesInfo[i]->processDuration;
        }
    }
    return -1;
}

int getProcessAdvancementTime(Workload *workload, int pid)
{
    for (int i = 0; i < workload->nbProcesses; i++)
    {
        if (getPIDFromWorkload(workload, i) == pid)
        {
            return workload->processesInfo[i]->advancementTime;
        }
    }
    return -1;
}

int getProcessNextEventTime(Workload *workload, int pid)
{
    int processNextEventTime = getProcessDuration(workload, pid); // relative to the process
    for (int i = 0; i < workload->nbProcesses; i++)
    {
        if (getPIDFromWorkload(workload, i) != pid)
        {
            continue;
        }
        if (workload->processesInfo[i]->nextEvent)
        {
            processNextEventTime = workload->processesInfo[i]->nextEvent->time;
        }
        break;
    }
    return processNextEventTime;
}

int getProcessCurEventTimeLeft(Workload *workload, int pid)
{
    return getProcessNextEventTime(workload, pid)
           - getProcessAdvancementTime(workload, pid);
}

static int getProcessIndex(Workload *workload, int pid)
{
    int processIndex = 0;
    for (; processIndex < workload->nbProcesses; processIndex++)
    {
        if (getPIDFromWorkload(workload, processIndex) != pid)
        {
            continue;
        }
        break;
    }

    return processIndex;
}

static void setProcessAdvancementTime(Workload *workload, int pid, int advancementTime)
{
    for (int i = 0; i < workload->nbProcesses; i++)
    {
        if (getPIDFromWorkload(workload, i) == pid)
        {
            workload->processesInfo[i]->advancementTime = advancementTime;
            return;
        }
    }
}


/* -------------------------- init/free functions -------------------------- */

Workload *parseInputFile(const char *fileName)
{
    printVerbose("Parsing input file...\n");
    FILE *file = fopen(fileName, "r");
    if (!file)
    {
        fprintf(stderr, "Error: could not open file %s\n", fileName);
        return NULL;
    }

    Workload *workload = (Workload *) malloc(sizeof(Workload));
    if (!workload)
    {
        fprintf(stderr, "Error: could not allocate memory for workload\n");
        fclose(file);
        return NULL;
    }

    char line[MAX_CHAR_PER_LINE];
    int nbProcesses = 0;
    // 1 line == 1 process
    while (fgets(line, MAX_CHAR_PER_LINE, file))
    {
        if (line[strlen(line) - 1] != '\n')
        {
            fprintf(stderr, "Error: line too long in the input file.\n");
            freeWorkload(workload);
            fclose(file);
            return NULL;
        }
        if (line[0] == '#' || line[0] == '\n')
        {
            continue;
        }
        nbProcesses++;
    }
    
    workload->processesInfo = (ProcessSimulationInfo **) malloc(
            sizeof(ProcessSimulationInfo *) * nbProcesses);
    if (!workload->processesInfo)
    {
        fprintf(stderr, "Error: could not allocate memory for processes info\n");
        freeWorkload(workload);
        fclose(file);
        return NULL;
    }

    workload->nbProcesses = 0;

    rewind(file);
    while (fgets(line, MAX_CHAR_PER_LINE, file)) // Read file line by line
    {
        if (line[0] == '#' || line[0] == '\n')
        {
            continue;
        }

        ProcessSimulationInfo *processInfo = (ProcessSimulationInfo *) malloc(
                sizeof(ProcessSimulationInfo));
        if (!processInfo)
        {
            fprintf(stderr, "Error: could not allocate memory for process info\n");
            freeWorkload(workload);
            fclose(file);
            return NULL;
        }

        processInfo->pcb = (PCB *) malloc(sizeof(PCB));
        if (!processInfo->pcb)
        {
            fprintf(stderr, "Error: could not allocate memory for PCB\n");
            free(processInfo);
            freeWorkload(workload);
            fclose(file);
            return NULL;
        }

        processInfo->pcb->state = READY;

        char *token = strtok(line, ",");
        processInfo->pcb->pid = atoi(token);

        token = strtok(NULL, ",");
        processInfo->startTime = atoi(token);

        token = strtok(NULL, ",");
        processInfo->processDuration = atoi(token);

        token = strtok(NULL, ",");
        processInfo->pcb->priority = atoi(token);

        processInfo->advancementTime = 0;

        token = strtok(NULL, "(");

        ProcessEvent *event = NULL;
        while (strstr(token, ",") || strstr(token, "[")) // Read events
        {
            if (strstr(token, "[")) // first event
            {
                event = (ProcessEvent *) malloc(sizeof(ProcessEvent));
                processInfo->nextEvent = event;
            }
            else
            {
                event->nextEvent = (ProcessEvent *) malloc(sizeof(ProcessEvent));
                event = event->nextEvent;
            }
            if (!event)
            {
                fprintf(stderr, "Error: could not allocate memory for event\n");
                free(processInfo->pcb);
                free(processInfo);
                freeWorkload(workload);
                fclose(file);
                return NULL;
            }

            token = strtok(NULL, ",");
            event->time = atoi(token);

            token = strtok(NULL, ")");

            if (token != NULL)
            {
                if (strstr(token, "CPU"))
                {
                    event->type = CPU_BURST;
                }
                else if (strstr(token, "IO"))
                {
                    event->type = IO_BURST;
                }
                else
                {
                    fprintf(stderr, "Error: Unknown operation type\n");
                }
            }

            event->nextEvent = NULL;
            token = strtok(NULL, "(");
        } // End of events
        workload->processesInfo[workload->nbProcesses] = processInfo;
        workload->nbProcesses++;
    } // End of file

    fclose(file);

    printVerbose("Input file parsed successfully\n");

    return workload;
}

void freeWorkload(Workload *workload)
{
    for (int i = 0; i < workload->nbProcesses; i++)
    {
        ProcessEvent *event = workload->processesInfo[i]->nextEvent;
        while (event)
        {
            ProcessEvent *nextEvent = event->nextEvent;
            free(event);
            event = nextEvent;
        }
        free(workload->processesInfo[i]->pcb);
        free(workload->processesInfo[i]);
    }
    free(workload->processesInfo);
    free(workload);
}


/* ---------------------------- other functions ---------------------------- */

void launchSimulation(Workload *workload, SchedulingAlgorithm **algorithms, int algorithmCount, int cpuCoreCount, ProcessGraph *graph, AllStats *stats)
{
    for (int i = 0; i < getProcessCount(workload); i++)
    {
        addProcessToGraph(graph, getPIDFromWorkload(workload, i));
    }
    setNbProcessesInStats(stats, getProcessCount(workload));

    Scheduler *scheduler = initScheduler(algorithms, algorithmCount, getProcessCount(workload));
    if (!scheduler)
    {
        fprintf(stderr, "Error: could not initialize scheduler\n");
        return;
    }

    CPU *cpu = initCPU(cpuCoreCount);
    if (!cpu)
    {
        fprintf(stderr, "Error: could not initialize CPU\n");
        freeScheduler(scheduler);
        return;
    }

    Disk *disk = initDisk();
    if (!disk)
    {
        fprintf(stderr, "Error: could not initialize disk\n");
        freeCPU(cpu);
        freeScheduler(scheduler);
        return;
    }

    Computer *computer = initComputer(scheduler, cpu, disk);
    if (!computer)
    {
        fprintf(stderr, "Error: could not initialize computer\n");
        freeDisk(disk);
        freeCPU(cpu);
        freeScheduler(scheduler);
        return;
    }

    addAllProcessesToStats(stats, workload);

    // You may want to sort processes by start time to facilitate the
    // simulation. Remove this line and the compareProcessStartTime if you
    // don't want to.
    qsort(workload->processesInfo, workload->nbProcesses, sizeof(ProcessSimulationInfo *), compareProcessStartTime);

    int time = 0;
    int switchinDelay = 1;
    int switchoutDelay = 0;
    int PIDonDisk = 0;
    int interruptHandler = 0;
    int timesliceLeft = algorithms[0]->RRSliceLimit;
    
    while(time<=50){
        /*Handle Events*/
        for (int i = 0; i < workload->nbProcesses; i++){
            /*Check for nextEvent (CPU_BURST OR IO_BURST)*/
            if(workload->processesInfo[i]->nextEvent != NULL){ 
                if(workload->processesInfo[i]->nextEvent->type == CPU_BURST && workload->processesInfo[i]->nextEvent->time == time){
                    if(alreadyReadyQueue(scheduler, workload->processesInfo[i]->pcb) == false){
                        AddReadyQueue(scheduler, workload->processesInfo[i]->pcb);
                        if(disk->isIdle == false) //if process was waiting on disk -> reset disk to idle & add to readyQueue
                            disk->isIdle = true;
                        if(workload->processesInfo[i]->nextEvent->nextEvent != NULL)
                            workload->processesInfo[i]->nextEvent = workload->processesInfo[i]->nextEvent->nextEvent;
                        else
                            workload->processesInfo[i]->nextEvent = NULL;
                    }
                }else if(workload->processesInfo[i]->nextEvent->type == IO_BURST && workload->processesInfo[i]->nextEvent->time == time){
                    IOInterrupt(computer, CoreWithPID(computer, workload->processesInfo[i]->pcb->pid));
                    timesliceLeft = algorithms[0]->RRSliceLimit;
                    if(workload->processesInfo[i]->nextEvent->nextEvent != NULL)
                        workload->processesInfo[i]->nextEvent = workload->processesInfo[i]->nextEvent->nextEvent;
                    else
                        workload->processesInfo[i]->nextEvent = NULL;
                }
            }else{ 
                for(int j=0; j<cpu->coreCount; j++){
                    /*Check that any core is running*/
                    if(cpu->cores[j]->state == NOTIDLE){
                        /*CPU on core advacned enough -> process is terminated*/
                        if(getProcessAdvancementTime(workload, cpu->cores[j]->process->pid) >= getProcessDuration(workload, cpu->cores[j]->process->pid)){    
                            workload->processesInfo[getProcessIndex(workload, cpu->cores[j]->process->pid)]->pcb->state = TERMINATED;
                            cpu->cores[j]->state = IDLE;
                            switchinDelay = 1;
                            switchoutDelay = 0;
                            timesliceLeft = algorithms[0]->RRSliceLimit;
                        }
                        /*CPU on core has spend enough time on core -> switchout (RR slice time)*/
                        if(timesliceLeft == 0 && cpu->cores[j]->process->pid == workload->processesInfo[i]->pcb->pid){
                            if(lastProcess(scheduler) == false){
                                AddReadyQueue(scheduler, workload->processesInfo[i]->pcb);
                                cpu->cores[j]->state = IDLE;
                                switchinDelay = 1;
                                switchoutDelay = 2;
                                timesliceLeft = algorithms[0]->RRSliceLimit;
                            }
                        }
                    }
                }
            }
        }

        /*Assign processes to ressources*/
        switch(algorithms[0]->type){
            case FCFS:
                FCFSff(computer, switchinDelay, switchoutDelay);
                break;
            case PRIORITY:
                PRIORITYff(computer, switchinDelay, switchoutDelay);
                break;
            case SJF:
                SJFff(computer, switchinDelay, switchoutDelay, workload);
                break;
            case RR:
                RRff(computer, switchinDelay, switchoutDelay);
                break;
            default:
                break;
        }

        /*Handle Graph*/
        for(int i=0; i<workload->nbProcesses; i++){
            switch(workload->processesInfo[i]->pcb->state){
                case READY: 
                    addProcessEventToGraph(graph, workload->processesInfo[i]->pcb->pid, time, READY, NO_CORE);
                    if(workload->processesInfo[i]->pcb->pid == PIDonDisk){
                        addDiskEventToGraph(graph, workload->processesInfo[i]->pcb->pid, time, DISK_IDLE);
                        PIDonDisk = 0;
                    }
                    break;
                case RUNNING:
                    addProcessEventToGraph(graph, workload->processesInfo[i]->pcb->pid, time, RUNNING, CoreWithPID(computer, workload->processesInfo[i]->pcb->pid));
                    break;
                case WAITING:
                    addProcessEventToGraph(graph, workload->processesInfo[i]->pcb->pid, time, WAITING, NO_CORE);
                    addDiskEventToGraph(graph, workload->processesInfo[i]->pcb->pid, time, DISK_RUNNING);
                    PIDonDisk = workload->processesInfo[i]->pcb->pid;
                    break;
                case TERMINATED:
                    addProcessEventToGraph(graph, workload->processesInfo[i]->pcb->pid, time, TERMINATED, NO_CORE);
                    break;
            }
        }

        /*Advance time of process in core and of simultion*/
        if(switchinDelay != 0 && switchoutDelay == 0)
            switchinDelay--;
        if(switchoutDelay != 0)
            switchoutDelay--;
        for(int i=0; i<cpu->coreCount; i++){
            if(cpu->cores[i]->state == NOTIDLE){
                workload->processesInfo[getProcessIndex(workload, cpu->cores[i]->process->pid)]->advancementTime++;
            }
        }
        if(disk->isIdle == false){
            workload->processesInfo[getProcessIndex(workload, disk->processIO->pid)]->advancementTime++;
        }
        time++;
        if(algorithms[0]->type == RR && cpu->cores[0]->state == NOTIDLE){
            timesliceLeft--;
        }
    }

    freeComputer(computer);
}


/* ---------------------------- static functions --------------------------- */

static bool runningProcess(const Workload *workload){
    for (int i = 0; i < workload->nbProcesses; i++)
    {
        if (workload->processesInfo[i]->pcb->state == RUNNING)
        {
            return 1;
        }
    }

    return 0;
}

static bool workloadOver(const Workload *workload)
{
    for (int i = 0; i < workload->nbProcesses; i++)
    {
        if (workload->processesInfo[i]->advancementTime < workload->processesInfo[i]->processDuration)
        {
            return 0;
        }
    }

    return 1;
}

static void addAllProcessesToStats(AllStats *stats, Workload *workload)
{
    for (int i = 0; i < workload->nbProcesses; i++)
    {
        ProcessStats *processStats = (ProcessStats *) malloc(sizeof(ProcessStats));
        if (!processStats)
        {
            fprintf(stderr, "Error: could not allocate memory for process stats\n");
            return;
        }
        processStats->processId = getPIDFromWorkload(workload, i);
        processStats->priority = workload->processesInfo[i]->pcb->priority;
        processStats->arrivalTime = 0;
        processStats->finishTime = 0;
        processStats->turnaroundTime = 0;
        processStats->cpuTime = 0;
        processStats->waitingTime = 0;
        processStats->meanResponseTime = 0;
        // You could want to put this field to -1
        processStats->nbContextSwitches = 0;

        addProcessStats(stats, processStats);
    }
}

static int compareProcessStartTime(const void *a, const void *b)
{
    const ProcessSimulationInfo *infoA = *(const ProcessSimulationInfo **)a;
    const ProcessSimulationInfo *infoB = *(const ProcessSimulationInfo **)b;

    if (infoA->startTime < infoB->startTime)
    {
        return -1;
    }
    else if (infoA->startTime > infoB->startTime)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}
