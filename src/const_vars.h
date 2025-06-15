#ifndef CONST_VARS
#define CONST_VARS

#define MSG_QUEUE_KEY 200
#define MSG_TYPE_FINISHED 3 // Process completion
#define MEM_SIZE 1024
#define MAX_PROCESSES 1000

enum SchedulingAlogrithms
{
    HPF,
    SRTN,
    RR
};
enum ProcessStatus
{
    RUNNING,
    WAITING,
};

enum ProcessState
{
    RUN,
    STOP,
    FINISHED
};

enum sendingReceving
{
    SEND = 1,
    RECIEVE
};

#endif