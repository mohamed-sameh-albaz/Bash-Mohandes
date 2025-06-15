#include "scheduler.h"

FILE *outFile = NULL;
int *taArr = NULL;
double *wtaArr = NULL;
int *watingTime = NULL;
double totalCPU = 0.0;

void run_scheduler(enum SchedulingAlogrithms algorithm, int *q, int *processesCnt, int messageQID, long arrival_mtype, long termination_mtype)
{
    sync_clk();

    struct Processes list = {.processes_size = 0, .total_process = 0};
    list.processes_list = (struct PCB **)malloc((*processesCnt) * sizeof(struct PCB *));
    if (!list.processes_list)
    {
        printf("Memory allocation failed\n");
        return;
    }
    initOutputFile(*processesCnt);

    schedule(&list, processesCnt, messageQID, arrival_mtype, termination_mtype, algorithm, *q);

    free(list.processes_list);
    closeOutputFile();
    prefOutPut(processesCnt);
    closeOutputFile();

    destroy_clk(0);
}

void schedule(struct Processes *list, int *processesCnt, int messageQID, long arrival_mtype, long termination_mtype, enum SchedulingAlogrithms algorithm, int q)
{
    Queue *queue = NULL;
    // Create queue for RR
    if (algorithm == RR)
    {
        queue = createQueue(*processesCnt);
    }

    int msgq_id = messageQID;
    if (msgq_id == -1)
    {
        printf("error in creating message buffer for receive the running process from process generator ");
        exit(-1);
    }

    struct my_msg_processbuf process_msg;
    key_t proc_key_id = MSG_QUEUE_KEY;
    int proc_msgq_id = msgget(proc_key_id, 0666 | IPC_CREAT);
    if (proc_msgq_id == -1)
    {
        printf("error in creating message buffer for communicating with running process");
        exit(-1);
    }

    struct my_msg_procTermination run_proc_msg;
    struct PCB *running_proc = NULL;
    bool termination = false;
    bool running = false;
    int start_time = 0;
    int count = 0;

    while (1)
    {
        int current_time = get_clk();

        // Handle new process arrivals
        while (msgrcv(msgq_id, &process_msg, sizeof(process_msg) - sizeof(long), arrival_mtype, IPC_NOWAIT) != -1)
        {
            if (algorithm != RR)
            {
                add_process(list, process_msg.p);
            }
            else
            {
                struct PCB *p = initPCB(process_msg.p);
                enqueue(queue, p);
            }
        }

        // Check for process termination
        if (running_proc)
        {
            int rec_run_proc = msgrcv(proc_msgq_id, &run_proc_msg, sizeof(run_proc_msg) - sizeof(long), MSG_TYPE_FINISHED, IPC_NOWAIT);
            if (rec_run_proc != -1 && run_proc_msg.process_id == running_proc->process_id)
            {
                termination = true;
            }
        }

        // Handle process termination
        if (termination)
        {
            termination = false;
            running = false;
            count++;

            // Update final times
            running_proc->finish_time = current_time;
            running_proc->remaining_time = 0;
            running_proc->execution_time += (current_time - start_time);

            if (algorithm != RR)
            {
                deleteProcess(running_proc, list, current_time);
            }
            else
            {
                outputFile(running_proc, current_time, "finished");
                updatePrefStats(running_proc);
                totalCPU += running_proc->execution_time;
            }

            struct my_msg_procTermination term_proc_msg;
            term_proc_msg.mtype = termination_mtype;
            term_proc_msg.process_id = running_proc->process_id;
            msgsnd(msgq_id, &term_proc_msg, sizeof(term_proc_msg) - sizeof(long), !IPC_NOWAIT);

            if (algorithm == RR)
            {
                free(running_proc);
            }

            running_proc = NULL;
        }

        struct PCB *next_proc = NULL;

        if (algorithm == RR)
        {
            // handle RR preemption
            if (running && (current_time - start_time) >= q)
            {
                running_proc->status = WAITING;
                running_proc->remaining_time = running_proc->remaining_time - (current_time - start_time);
                running_proc->execution_time = running_proc->execution_time + (current_time - start_time);
                running_proc->stop_time = current_time;
                kill(running_proc->pid, SIGSTOP);
                outputFile(running_proc, current_time, "stopped");
                enqueue(queue, running_proc);
                running = false;
            }

            // handle RR scheduling
            if (!running && !isEmpty(queue))
            {
                next_proc = dequeue(queue);
            }
        }
        else if (algorithm == SRTN)
        {
            // Find process with shortest remaining time
            int shortest_time = INT_MAX;
            for (int i = 0; i < list->processes_size; i++)
            {
                if (list->processes_list[i]->status == WAITING &&
                    list->processes_list[i]->remaining_time < shortest_time)
                {
                    shortest_time = list->processes_list[i]->remaining_time;
                    next_proc = list->processes_list[i]; // set next process to run
                }
            }

            // handle SRTN preemption
            if (running && next_proc)
            {
                // Update times before preemption
                running_proc->execution_time += (current_time - start_time);
                running_proc->remaining_time -= (current_time - start_time);

                // make sure the next process has a shorter remaining time
                if (next_proc->remaining_time < running_proc->remaining_time)
                {
                    running_proc->status = WAITING;
                    running_proc->stop_time = current_time;
                    kill(running_proc->pid, SIGSTOP);
                    outputFile(running_proc, current_time, "stopped");
                    running = false;
                }
                else
                {
                    // backtrack
                    running_proc->remaining_time += (current_time - start_time);
                    running_proc->execution_time -= (current_time - start_time);
                }
            }
        }
        else{
            //handle HPF scheduling
            int highest_priority = INT_MAX;
            for (int i = 0; i < list->processes_size; i++) {
                if (list->processes_list[i]->status == WAITING &&
                    list->processes_list[i]->priority < highest_priority) {
                    highest_priority = list->processes_list[i]->priority;
                                next_proc = list->processes_list[i];
                            }
                        }
                    }

        // Start (or resume) a process if none is running
        if (!running && next_proc)
        {
            running_proc = next_proc;
            running_proc->status = RUNNING;
            kill(running_proc->pid, SIGCONT);

            if (running_proc->execution_time == 0)
            {
                running_proc->waiting_time = current_time - running_proc->arrival_time;
                outputFile(running_proc, current_time, "started");
            }
            else
            {
                running_proc->waiting_time += (current_time - running_proc->stop_time);
                outputFile(running_proc, current_time, "resumed");
            }
            running = true;
            start_time = current_time;
        }

        usleep(50000); // 50 ms delay

        if (count == *processesCnt)
        { // all processes have finished
            if (algorithm == RR)
            {
                free(queue->processes);
                free(queue);
            }
            break;
        }
    }
    msgctl(proc_msgq_id, IPC_RMID, (struct msqid_ds *)0); // remove message queue
}

void initOutputFile(int processesCnt)
{
    outFile = NULL;
    outFile = fopen("scheduler.log", "w");
    if (!outFile)
    {
        perror("Failed to open scheduler.log");
        exit(1);
    }
    fprintf(outFile, "#At time x process y state arr w total z remain y wait k\n");
    fflush(outFile);
    taArr = malloc(processesCnt * sizeof(int));
    wtaArr = malloc(processesCnt * sizeof(int));
    watingTime = malloc(processesCnt * sizeof(int));
}

void closeOutputFile()
{
    if (outFile)
    {
        fclose(outFile);
        outFile = NULL;
    }
}

void outputFile(struct PCB *process, int current_time, char status[])
{
    if (!outFile)
        return;

    if (strcmp(status, "finished") == 0)
    {
        int ta = process->finish_time - process->arrival_time;
        fprintf(outFile, "At time %d process %d finished arr %d total %d remain %d wait %d TA %d WTA %.2f\n",
                current_time, process->process_id, process->arrival_time,
                process->execution_time + process->remaining_time, process->remaining_time, process->waiting_time,
                ta, (float)ta / (process->execution_time ? process->execution_time : 1));
    }
    else if (strcmp(status, "stopped") == 0)
    {
        fprintf(outFile, "At time %d process %d stopped arr %d total %d remain %d wait %d\n",
                current_time, process->process_id, process->arrival_time,
                process->execution_time + process->remaining_time, process->remaining_time, process->waiting_time);
    }
    else if (strcmp(status, "started") == 0)
    {
        fprintf(outFile, "At time %d process %d started arr %d total %d remain %d wait %d\n",
                current_time, process->process_id, process->arrival_time,
                process->execution_time + process->remaining_time, process->remaining_time, process->waiting_time);
    }
    else if (strcmp(status, "resumed") == 0)
    {
        fprintf(outFile, "At time %d process %d resumed arr %d total %d remain %d wait %d\n",
                current_time, process->process_id, process->arrival_time,
                process->execution_time + process->remaining_time, process->remaining_time, process->waiting_time);
    }
    fflush(outFile);
}

void updatePrefStats(struct PCB *process)
{
    wtaArr[process->process_id - 1] = (process->finish_time - process->arrival_time) / (process->execution_time ? process->execution_time : 1);
    taArr[process->process_id - 1] = (process->finish_time - process->arrival_time);
    watingTime[process->process_id - 1] = (process->waiting_time);
}

void prefOutPut(int *processessCnt)
{
    if (*processessCnt)
    {
        double avgWaitingTime = 0;
        double avgWTA = 0;
        double stdWTA = 0;

        for (int i = 0; i < *processessCnt; ++i)
        {
            avgWaitingTime += watingTime[i];
            avgWTA += wtaArr[i];
        }
        avgWaitingTime = avgWaitingTime / *processessCnt;
        avgWTA = avgWTA / *processessCnt;
        for (int i = 0; i < *processessCnt; ++i)
        {
            stdWTA += powf(wtaArr[i] - avgWTA, 2);
        }
        stdWTA = stdWTA / *processessCnt;
        stdWTA = sqrt(stdWTA);
        totalCPU = totalCPU / get_clk();
        outFile = NULL;
        outFile = fopen("scheduler.perf", "w");
        if (!outFile)
        {
            perror("Failed to open scheduler.perf");
            exit(1);
        }
        fprintf(outFile, "CPU utilization = %.2f%c\nAvg WTA = %.2f\nAvg Waiting = %.2f\nStd WTA = %.2f\n", totalCPU * 100, '%', avgWTA, avgWaitingTime, stdWTA);
        fflush(outFile);
    }
}

void deleteProcess(struct PCB *delProc, struct Processes *list, int finish_time)
{

    for (int i = 0; i < list->processes_size; i++)
    {
        if (list->processes_list[i]->process_id == delProc->process_id)
        {
            delProc->finish_time = finish_time;
            outputFile(delProc, finish_time, "finished");
            updatePrefStats(delProc);
            totalCPU += delProc->execution_time;
            list->processes_list[i] = list->processes_list[list->processes_size - 1];
            free(delProc);
            list->processes_list[list->processes_size - 1] = NULL;
            break;
        }
    }
    list->processes_size--;
    list->total_process++;
}

void add_process(struct Processes *list, struct ProcessInfo p_info)
{

    if (!list)
        return;

    struct PCB *p = (struct PCB *)malloc(sizeof(struct PCB));
    if (!p)
    {
        printf("Memory allocation failed\n");
        return;
    }
    p->execution_time = 0;
    p->waiting_time = get_clk() - p_info.arrival_time;
    p->status = WAITING;
    p->process_id = p_info.process_id;
    p->priority = p_info.priority;
    p->remaining_time = p_info.run_time;
    p->arrival_time = p_info.arrival_time;
    p->pid = p_info.pid;

    list->processes_list[list->processes_size++] = p;
}

Queue *createQueue(int capacity)
{
    Queue *queue = (Queue *)malloc(sizeof(Queue));
    queue->capacity = capacity;
    queue->front = 0;
    queue->rear = -1;
    queue->size = 0;
    queue->processes = (struct PCB **)malloc(queue->capacity * sizeof(struct PCB *));
    return queue;
}

int isFull(Queue *queue)
{
    return (queue->size == queue->capacity);
}
int isEmpty(Queue *queue)
{
    return (queue->size == 0);
}

void enqueue(Queue *queue, struct PCB *process)
{
    if (isFull(queue))
        return;
    process->status = WAITING;
    queue->rear = (queue->rear + 1) % queue->capacity;
    queue->processes[queue->rear] = process;
    queue->size++;
}

struct PCB *dequeue(Queue *queue)
{
    if (isEmpty(queue))
    {
        struct PCB *empty_process = {0};
        return empty_process;
    }
    queue->processes[queue->front]->status = RUNNING;
    struct PCB *process = queue->processes[queue->front];
    queue->front = (queue->front + 1) % queue->capacity;
    queue->size--;

    return process;
}
struct PCB *front(Queue *queue)
{
    if (isEmpty(queue))
    {
        struct PCB *empty_process = {0};
        return empty_process;
    }
    return queue->processes[queue->front];
}

struct PCB *initPCB(struct ProcessInfo p_info)
{
    struct PCB *p = (struct PCB *)malloc(sizeof(struct PCB));
    p->execution_time = 0;
    p->waiting_time = get_clk() - p_info.arrival_time;
    p->status = WAITING;
    p->process_id = p_info.process_id;
    p->priority = p_info.priority;
    p->remaining_time = p_info.run_time;
    p->arrival_time = p_info.arrival_time;
    p->pid = p_info.pid;
    p->stop_time = 0;
    p->finish_time = 0;
    return p;
}