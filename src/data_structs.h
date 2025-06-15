#ifndef DATA_STRUCTS_h
#define DATA_STRUCTS_h
#include "const_vars.h"
#include <unistd.h>

struct PCB
{
    int process_id;
    enum ProcessStatus status;
    int execution_time;
    int remaining_time;
    int waiting_time;
    int priority;
    int arrival_time;
    int finish_time;
    int stop_time; //  make it array and index id process id
    pid_t pid;
    int memsize; // occupied
    int startingAddress;
};

struct Processes
{
    int processes_size;
    int total_process;
    struct PCB **processes_list;
};

struct ProcessInfo
{
    int process_id;
    int arrival_time;
    int run_time;
    int priority;
    int memsize;
    pid_t pid;
};

struct my_msg_algorithmbuf
{
    long mtype;
    enum SchedulingAlogrithms algorithm;
}; // saif

struct my_msg_processbuf
{
    long mtype;
    struct ProcessInfo p;
};
struct my_msg_procTermination
{
    long mtype;
    int process_id;
};

struct my_msg_runProcess
{
    long mtype;
    enum ProcessState state;
};

typedef struct
{
    struct PCB **processes;
    int front, rear, size;
    int capacity;
} Queue;

typedef enum
{
    FREE,
    RESERVED,
    SPLIT
} BlockStatus;

typedef struct MemoryBlock
{
    int size;
    int address; // starting address
    int ocuupiedmemory;
    BlockStatus status;
    struct MemoryBlock *parent;
    struct MemoryBlock *left;
    struct MemoryBlock *right;
} MemoryBlock;

typedef struct
{
    long mtype;                 // Message type (must be > 0)
    struct ProcessInfo process; // Message process data
} msgbuf;

typedef struct waitingList
{
    MemoryBlock *memory; // when deallocate
    struct ProcessInfo *process;
    struct waitingList *next;
    struct waitingList *prev;
} waitingList;

typedef struct
{
    MemoryBlock **memoryList;
    int size;
} MemoryList;



#endif