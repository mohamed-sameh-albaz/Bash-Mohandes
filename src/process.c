#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <signal.h>
#include "process.h"
#include "scheduler.h"
#include "clk.h"

// Message structure for process-scheduler communication
typedef struct
{
    long mtype;
    int pid;
} status_msg;

// Global variables
static int msgq_id = -1;
static int remaining_time = 0;
static int process_id = 0;
static int currTime = 0;
static int prevTime = 0;

void cleanup_resources()
{
    destroy_clk(0);
}

void handle_sigterm(int signum)
{
    // Process was terminated
    // Send final status update to scheduler
    if (msgq_id != -1)
    {
        status_msg msg;
        msg.mtype = MSG_TYPE_FINISHED;
        msg.pid = process_id;
        if (msgsnd(msgq_id, &msg, sizeof(msg) - sizeof(long), 0) == -1)
        {
            fprintf(stderr, "Error sending completion message in signal %d ", signum);
        }
    }

    cleanup_resources();
    exit(0);
}

void handle_sigcont(int signum)
{
    currTime = -1;
    prevTime = -1;
    // handle_sigcont(signum);
}
int main(int argc, char *argv[])
{
    // Get process id and remaining time from process generator

    if (argc != 3)
        fprintf(stderr, "error in arguments of process");

    process_id = atoi(argv[1]);
    remaining_time = atoi(argv[2]);

    // Set up signal handlers
    if (signal(SIGTERM, handle_sigterm) == SIG_ERR)
    {
        perror("Error setting up SIGTERM handler");
        exit(1);
    }

    // Initialize clock
    sync_clk();
    if (signal(SIGCONT, handle_sigcont) == SIG_ERR)
    {
        perror("Error setting up SIGCONT handler");
        exit(1);
    }
    currTime = get_clk();
    prevTime = currTime;

    // Connect to message queue
    msgq_id = msgget(MSG_QUEUE_KEY, 0666 | IPC_CREAT);
    if (msgq_id == -1)
    {
        perror("Error accessing message queue");
        exit(1);
    }
    // Main execution loop: decrement remaining_time each clock tick, handle SIGSTOP/SIGCONT

    currTime = get_clk();
    prevTime = currTime;
    while (remaining_time > 0)
    {
        currTime = get_clk();
        if (currTime - prevTime == 1)
        {
            remaining_time--;
        }
        prevTime = currTime;
    }
    // Process completed - send completion message
    struct my_msg_procTermination msg;
    msg.mtype = MSG_TYPE_FINISHED;
    msg.process_id = process_id;
    if (msgsnd(msgq_id, &msg, sizeof(msg) - sizeof(long), !IPC_NOWAIT) == -1)
    {
        perror("Error sending completion message");
        cleanup_resources();
        exit(1);
    }
    cleanup_resources();
    exit(1);
    return 0;
}