#ifndef SCHEDULER_H
#define SCHEDULER_H
#include "clk.h"
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h> 
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <limits.h>
#include <errno.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <math.h>
#include "const_vars.h"
#include "data_structs.h"
void schedule(struct Processes *list, int *processesCnt, int messageQID, long arrival_mtype, long termination_mtype, enum SchedulingAlogrithms algorithm, int quantum);
void add_process(struct Processes *list, struct ProcessInfo p_info);
void deleteProcess(struct PCB *delProc, struct Processes *list, int finish_time);
void outputFile(struct PCB *process, int current_time, char status[]);
void initOutputFile(int processesCnt);
void closeOutputFile();
void prefOutPut(int *processessCnt);
void updatePrefStats(struct PCB *process);
void run_scheduler(enum SchedulingAlogrithms algorithm, int *q, int *processesCnt, int messageQID, long arrival_mtype, long termination_mtype);
struct PCB *initPCB(struct ProcessInfo p_info);
struct PCB *dequeue(Queue *queue);
void enqueue(Queue *queue, struct PCB *process);
int isEmpty(Queue *queue);
extern FILE *outFile;
extern int *taArr;
extern double *wtaArr;
extern int *watingTime;
extern double totalCPU;

Queue *createQueue(int capacity);

#endif