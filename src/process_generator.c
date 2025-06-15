#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>    // for fork, execl
#include <sys/types.h> // for pid_t
#include "clk.h"
#include <stdlib.h>
#include <sys/msg.h> // For message queue functions
#include <string.h>
#include <getopt.h> // for terminal parsing using getopt
#include "scheduler.h"
#include <sys/wait.h>
#include "memory_manager.h"
#include "data_structs.h"
#include "const_vars.h"

int arrivalQID = -1;
waitingList *head;
MemoryList memoryList;

MemoryBlock *root = NULL;
FILE *memoryFile;

void clear_resources(int);
int parse_args(int argc, char **argv, char **filename, int *algorithm, int *q);
int read_processes_file(char *filename, struct ProcessInfo **processes, int *cnt);
int fork_Schedular_clk(pid_t *clock_pid, pid_t *scheduler_pid, int *algorithm, int *q, int *processesCnt);
int create_queues();
int send_process(struct ProcessInfo *process, int cntTime);
void send_arrivals(struct ProcessInfo *processes, int processCnt, int *finshed, pid_t *sch_pid);
void printProcessesFileData(struct ProcessInfo **processes, int *cnt);
void initWaitingList();
void addNode(struct ProcessInfo *process);
void deleteNode(struct ProcessInfo *process);
bool checkWaitingQueue(int *cnt, struct ProcessInfo *processes);
void memoryPrint(int processId, char status[]);
void printWaitingList();
MemoryBlock *findMemoryBlock(int pid, struct ProcessInfo **processes, int *processId);

int main(int argc, char *argv[])
{
    signal(SIGINT, clear_resources);
    char *filename;
    int algorithm, q;
    if (parse_args(argc, argv, &filename, &algorithm, &q))
    {
        return 1; // failure while parsing the in cmd
    }
    struct ProcessInfo *processes;
    int cnt; // process counter
    if (read_processes_file(filename, &processes, &cnt))
    {
        return 1; // failure while reading the input file
    }
    if (create_queues())
    {
        free(processes);
        return 1; // failure while creating msg queues
    }
    pid_t clkId, schedularId;
    if (fork_Schedular_clk(&clkId, &schedularId, &algorithm, &q, &cnt))
    {
        free(processes);
        clear_resources(0);
        return 1; // failure while forking the clk or the schedular processes
    }

    sync_clk();
    root = (MemoryBlock *)malloc(sizeof(MemoryBlock));
    if (root == NULL) {
        fprintf(stderr, "Failed to allocate memory for root block\n");
        clear_resources(0);
        return 1;
    }
    initTree();
    memoryList.size = 0;
    memoryList.memoryList = (MemoryBlock **)malloc(cnt * sizeof(MemoryBlock *));
    if (!memoryList.memoryList) {
        fprintf(stderr, "Failed to allocate memory for memory list\n");
        free(root);
        clear_resources(0);
        return 1;
    }
    int cntFinished = 0;
    printProcessesFileData(&processes, &cnt);
    send_arrivals(processes, cnt, &cntFinished, &schedularId);
    while (cntFinished < cnt)
    {
        int status = 0;
        int pid = wait(&status);
        if (pid > 0 && pid != schedularId) // Check if wait returned a valid pid
        {
            if (!(status & 0x00FF))
            {
                printf("\nA child with pid %d terminated with exit code %d\n", pid, status >> 8);
                cntFinished++;
                int processId = 0;
                printf("ssssss\n");
                MemoryBlock *foundBlock = findMemoryBlock(pid, &processes, &processId);
                if (foundBlock != NULL)
                {
                    memoryPrint(processId - 1, "freed");
                    deallocate(foundBlock);
                }
                else
                {
                    printf("Warning: Could not find memory block for pid %d\n", pid);
                }
            }
        }
    }
    free(processes);
    sleep(2);
    clear_resources(0);

    wait(NULL); // Wait for clock
    wait(NULL); // Wait for scheduler

    return 0;
}

void clear_resources(int signum)
{
    (void)signum;
    // TODO Clears all resources in case of interruption
    if (arrivalQID != -1)
        msgctl(arrivalQID, IPC_RMID, NULL);
    destroy_clk(1); // Terminates shared memory and sends SIGINT to process group
    exit(0);
}
int parse_args(int argc, char **argv, char **filename, int *algorithm, int *q)
{
    int opt;
    *filename = NULL;
    *algorithm = -1;
    *q = -1;
    while ((opt = getopt(argc, argv, "s:f:q:")) != -1)
    {
        switch (opt)
        {
        case 's':
            if (strcmp(optarg, "hpf") == 0)
            {
                *algorithm = HPF;
            }
            else if (strcmp(optarg, "srtn") == 0)
            {
                *algorithm = SRTN;
            }
            else if (strcmp(optarg, "rr") == 0)
            {
                *algorithm = RR;
            }
            else
            {
                fprintf(stderr, "Invalid algorithm: %s\n", optarg);
                return -1;
            }
            break;
        case 'f':
            *filename = optarg;
            break;
        case 'q':
            *q = atoi(optarg);
            if (*q <= 0)
            {
                fprintf(stderr, "Invalid quantum: %s\n", optarg);
                return -1;
            }
            break;
        default:
            fprintf(stderr, "Invalid arguments\n");
            return -1;
        }
    }

    if (!*filename || *algorithm == -1 || (*algorithm == RR && *q == -1))
    {
        fprintf(stderr, "Missing required arguments\n");
        return -1;
    }
    return 0;
}

int read_processes_file(char *filename, struct ProcessInfo **processes, int *cnt)
{
    printf("%s", filename);
    FILE *processesFile = fopen(filename, "r");
    if (!processesFile)
    {
        fprintf(stderr, "Can't open the processes file\n");
        return -1;
    }
    *processes = malloc(MAX_PROCESSES * sizeof(struct ProcessInfo));
    if (!*processes)
    {
        fprintf(stderr, "Failed memory allocation\n");
        fclose(processesFile);
        return -1;
    }
    *cnt = 0; // num of processes read from file
    char line[256];
    while (fgets(line, sizeof(line), processesFile))
    {
        if (line[0] == '#' || line[0] == '\n')
        {
            continue;
        }

        int id, arrivalTime, runTime, priority, memsize;
        // sscanf read from a string return num of input values matched successfully
        if (sscanf(line, "%d %d %d %d %d", &id, &arrivalTime, &runTime, &priority, &memsize) != 5)
        {
            fprintf(stderr, "Invalid data\n");
            break;
        }
        if (runTime < 0 || priority < 0 || priority > 10)
        {
            fprintf(stderr, "Invalid data\n");
            continue;
        }
        (*processes)[*cnt].process_id = id;
        (*processes)[*cnt].arrival_time = arrivalTime;
        (*processes)[*cnt].run_time = runTime;
        (*processes)[*cnt].priority = priority;
        (*processes)[*cnt].memsize = memsize;
        (*cnt)++;
    }
    fclose(processesFile);
    return 0;
}

int fork_Schedular_clk(pid_t *clock_pid, pid_t *scheduler_pid, int *algorithm, int *q, int *processesCnt)
{
    *clock_pid = fork();
    if (*clock_pid < 0)
    {
        perror("Fork failed for clock");
        return -1;
    }
    else if (*clock_pid == 0)
    { // start the clk proccess

        init_clk();
        sync_clk();
        sleep(2);
        run_clk();
    }
    else
    {
        *scheduler_pid = fork();
        if (*scheduler_pid < 0)
        {
            perror("Fork failed for scheduler");
            return -1;
        }
        else if (*scheduler_pid == 0) // start the schedular process don't run the binary of schedular use it for the same kernel
        {
            run_scheduler(*algorithm, q, processesCnt, arrivalQID, SEND, RECIEVE);
            exit(1);
        }
    }
    return 0;
}

int create_queues()
{
    arrivalQID = 0;
    if ((arrivalQID = msgget(IPC_PRIVATE, 0666 | IPC_CREAT)) == -1)
    {
        perror("Failed to create arrival queue");
        return -1;
    }
    return 0;
}

int send_process(struct ProcessInfo *process, int cntTime)
{
    struct my_msg_processbuf msg;

    msg.mtype = SEND;

    pid_t pid = fork();
    if (pid < 0)
    {
        perror("Failed to fork process");
        return -1;
    }
    else if (pid == 0)
    {
        char id[20], runningTime[20];
        snprintf(id, sizeof(id), "%d", process->process_id);
        snprintf(runningTime, sizeof(runningTime), "%d", process->run_time);
        execl("./process", "process", id, runningTime, NULL); // tID will be removed
        perror("Failed to execute process");
        exit(1);
    }
    kill(pid, SIGSTOP);

    process->pid = pid;
    msg.p = *process;
    printf("send now process pid : %dprocess_id:%d\n", process->pid, process->process_id);
    if (msgsnd(arrivalQID, &msg, sizeof(msg) - sizeof(long), !IPC_NOWAIT) == -1) // should be received in the schedular
    {
        perror("Failed to send process data");
        return -1;
    }
    return 0;
}
void initMemoryFile()
{
    memoryFile = NULL;
    memoryFile = fopen("memory.log", "w");
    if (!memoryFile)
    {
        perror("Failed to open scheduler.log");
        exit(1);
    }
    fprintf(memoryFile, "#At time x allocated y bytes for process z from i to j \n");
    fflush(memoryFile);
}
void memoryPrint(int processId, char status[])
{
    if (!memoryFile)
        return;

    fprintf(memoryFile, "At time %d %s %d bytes for process %d from %d to %d \n", get_clk(), status, memoryList.memoryList[processId]->ocuupiedmemory, processId + 1, memoryList.memoryList[processId]->address, memoryList.memoryList[processId]->address + memoryList.memoryList[processId]->size - 1);
    fflush(memoryFile);
}
void send_arrivals(struct ProcessInfo *processes, int processCnt, int *cntFinished, pid_t *sch_pid)
{
    initMemoryFile();
    int cnt = 0;
    while (cnt < processCnt)
    {
        int currentTime = get_clk();
        checkWaitingQueue(&cnt, processes);
        for (int i = 0; i < processCnt; i++) // modify make it as a linked list to optimize performance and which is arrived remove it form the list
        {
            if (processes[i].arrival_time <= currentTime)
            {
                // check for waiting queue
                memoryList.memoryList[processes[i].process_id - 1] = allocate(root, processes[i].memsize);
                printf("allocated before\n");
                if (memoryList.memoryList[processes[i].process_id - 1] != NULL)
                {
                    printf("allocated after\n");
                    // call memoryprint
                    memoryList.size++;
                    memoryPrint(processes[i].process_id - 1, "allocated");
                    // printf("send now process  pid and process_id %d :%d\n",processes[i].pid,processes[i].process_id);
                    send_process(&processes[i], currentTime);
                    processes[i].arrival_time = INT_MAX; // marker for sent proccesses
                    cnt++;
                }
                else
                {
                    printf("add node  now process  pid :%d\n", processes[i].pid);
                    addNode(&processes[i]);
                }
            }
        }
        if (*cntFinished < processCnt)
        {
            int status = 0;
            int pid = waitpid(-1, &status, WNOHANG);
            // usleep(10);
            if (pid > 0 && pid != *sch_pid) // Check if wait returned a valid pid
            {
                if (!(status & 0x00FF))
                {
                    printf("\nA child with pid %d terminated with exit code %d\n", pid, status >> 8);
                    (*cntFinished)++;
                    int processId = 0;
                    printf("dddddd\n");
                    MemoryBlock *foundBlock = findMemoryBlock(pid, &processes, &processId);
                    if (foundBlock != NULL)
                    {
                        memoryPrint(processId - 1, "freed");
                        deallocate(foundBlock);
                    }
                    else
                    {
                        printf("Warning: Could not find memory block for pid %d\n", pid);
                    }
                    // MemoryBlock *foundBlock = findMemoryBlock(pid, &processes);
                    // if (foundBlock != NULL)
                    // {
                    //     deallocate(foundBlock);
                    // }
                    // else
                    // {
                    //     printf("Warning: Could not find memory block for pid %d\n", pid);
                    // }
                }
            }
        }
    }
}

// if (!(stat_loc & 0x00FF))
// {
//     printf("\nA child with pid %d terminated with exit code %d\n", pid, stat_loc >> 8);
//     cntFinished++;
// }
void initWaitingList()
{
    head = NULL;
}

void addNode(struct ProcessInfo *process)
{
    if (head == NULL)
    {
        head = (waitingList *)malloc(sizeof(waitingList));
        if (head == NULL)
        {
            perror("Failed to allocate memory for waiting list");
            return;
        }
        head->next = NULL;
        head->prev = NULL;
        head->memory = NULL;
        head->process = process;
        return;
    }

    waitingList *currnode = head;
    while (currnode->next != NULL)
    {
        currnode = currnode->next;
    }

    waitingList *newNode = (waitingList *)malloc(sizeof(waitingList));
    if (newNode == NULL)
    {
        perror("Failed to allocate memory for waiting list node");
        return;
    }
    newNode->process = process;
    newNode->memory = NULL;
    newNode->next = NULL;
    newNode->prev = currnode;
    currnode->next = newNode;
}

void deleteNode(struct ProcessInfo *process)
{
    waitingList *currnode = head;
    while (currnode != NULL)
    {
        if (currnode->process->process_id == process->process_id)
        {
            if (currnode->prev != NULL)
            {
                currnode->prev->next = currnode->next;
            }
            else
            {
                head = currnode->next;
            }

            if (currnode->next != NULL)
            {
                currnode->next->prev = currnode->prev;
            }

            free(currnode);
            return;
        }
        currnode = currnode->next;
    }
}

bool checkWaitingQueue(int *cnt, struct ProcessInfo *processes)
{
    int f = false;
    waitingList *currnode = head;
    while (currnode != NULL)
    {
        // printf("check for process in waitQueue \n");
        if (processes[currnode->process->process_id - 1].arrival_time == INT_MAX)
        {
            deleteNode(currnode->process);
            currnode = head;
          
        }
        else
        {
            memoryList.memoryList[currnode->process->process_id - 1] = allocate(root, currnode->process->memsize);
            if (memoryList.memoryList[currnode->process->process_id - 1] != NULL)
            {
                f = true;
                memoryList.size++;
                memoryPrint(currnode->process->process_id - 1, "allocated");
                printf("send now from the check\n");
                send_process(currnode->process, get_clk());
                // currnode->process->arrival_time = INT_MAX; // marker for sent proccesses
                processes[currnode->process->process_id - 1].arrival_time = INT_MAX;
                (*cnt)++;
                deleteNode(currnode->process);
                currnode = head;
                
            }
            else
            currnode = currnode->next;

        }
    }
    return f;
}

void printWaitingList()
{
    waitingList *currNode = head;
    printf("\n--- Waiting List ---\n");

    if (currNode == NULL)
    {
        printf("No processes waiting for memory allocation.\n");
        return;
    }

    int count = 0;
    while (currNode != NULL)
    {
        printf("Process #%d:\n", currNode->process->process_id);
        printf("  -> Memory Size: %d bytes\n", currNode->process->memsize);
        printf("  -> Arrival Time: %d\n", currNode->process->arrival_time);
        printf("  -> Runtime: %d\n", currNode->process->run_time);
        printf("  -> Priority: %d\n", currNode->process->priority);
        printf("  -> pid: %d\n", currNode->process->pid);
        printf("\n");

        currNode = currNode->next;
        count++;
    }
    printf("Total processes waiting: %d\n", count);
    printf("---------------------\n");
}

MemoryBlock *findMemoryBlock(int pid, struct ProcessInfo **processes, int *processId)
{
    for (int i = 0; i < memoryList.size; i++)
    {
        if (pid == (*processes)[i].pid)
        {
            *processId = (*processes)[i].process_id;
            if (*processId > 0 && *processId <= memoryList.size)
            {
                return memoryList.memoryList[(*processId) - 1];
            }
            return NULL;
        }
    }
    return NULL;
}

void printProcessesFileData(struct ProcessInfo **processes, int *cnt)
{
    for (int i = 0; i < *cnt; ++i)
    {
        printf("Proccess #%d: \n-> runningTime: %d\n-> arrivalTime: %d\n-> priority: %d\n-> pid: %d\n",
               (*processes)[i].process_id, (*processes)[i].run_time, (*processes)[i].arrival_time, (*processes)[i].priority, (*processes)[i].pid);
        printf("\n");
    }
}