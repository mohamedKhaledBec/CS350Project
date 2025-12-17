// scheduler.c
// Sample code for 3 activities: system fetch, ethernet fetch, logging
// Each activity is a task with its own priority

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <string.h>
#include <time.h>

#define MAX_PRIORITY 4
#define TIME_QUANTUM 2  // Each task gets 2 seconds per tick

typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_IDLE
} task_state_t;

typedef struct Task {
    int id;
    int priority;
    task_state_t state;
    pthread_mutex_t lock;
    void (*task_func)(struct Task*);
    struct Task* next;
    time_t last_run;
    int interval; // in seconds
    int cycle_count;
    time_t slice_start;  // When current slice started
    int time_used;       // Time used in current slice
} Task;

Task* priority_queues[MAX_PRIORITY];

void register_task(Task* task) {
    int p = task->priority;
    task->next = priority_queues[p];
    priority_queues[p] = task;
}

void scheduler_tick() {
    for (int p = 0; p < MAX_PRIORITY; ++p) {
        Task* t = priority_queues[p];
        while (t) {
            if (t->state == TASK_READY) {
                // Mark task as RUNNING (don't hold the lock here)
                t->state = TASK_RUNNING;
                t->slice_start = time(NULL);
                t->time_used = 0;
                
                // Print when task starts its time slice
                printf("[SCHEDULER] Task %d (Priority %d) gets CPU - Time Quantum: %d sec\n", 
                       t->id, t->priority, TIME_QUANTUM);
                return;
            }
            t = t->next;
        }
    }
}

void* task_thread(void* arg) {
    Task* t = (Task*)arg;
    
    while (1) {
        // Check if I'm scheduled to run
        if (t->state == TASK_RUNNING) {
            // Try to acquire lock for execution
            pthread_mutex_lock(&t->lock);
            
            time_t now = time(NULL);
            
            // I have the lock and running - execute my work
            printf("[TASK %d] Starting execution\n", t->id);
            
            t->task_func(t);
            t->last_run = now;
            
            t->time_used = (int)(time(NULL) - t->slice_start);
            
            printf("[TASK %d] Completed (Used: %d sec)\n", t->id, t->time_used);
            
            // Release the lock and yield
            pthread_mutex_unlock(&t->lock);
            t->state = TASK_IDLE;
            printf("[TASK %d] Yielding CPU\n\n", t->id);
        }
        
        sleep(1); // Check every second
    }
    return NULL;
}

// Initialize system
void init_system() {
    printf("Initializing system...\n");
    
    // Print current working directory
    char cwd[512];
    getcwd(cwd, sizeof(cwd));
    printf("Current working directory: %s\n", cwd);
    
    // Make scripts executable from parent directory
    system("chmod +x ../scripts/monitor_scheduler.sh 2>/dev/null");
    system("chmod +x ../scripts/ethernet_fetch.sh 2>/dev/null");
    
    // Create necessary directories
    system("mkdir -p ../logs 2>/dev/null");
    system("mkdir -p ../data 2>/dev/null");
    
    printf("System initialized.\n\n");
}

// Sample activities
void system_fetch(Task* t) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "bash '%s/scripts/monitor_scheduler.sh'", getenv("PWD"));
    printf("[System Monitor] Running\n");
    system(cmd);
}

void ethernet_fetch(Task* t) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "bash '%s/scripts/ethernet_fetch.sh'", getenv("PWD"));
    printf("[Ethernet Fetch] Running\n");
    system(cmd);
}

void analyze_data(Task* t) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "'%s/build/analyze' 50.0 60.0 75.0 '%s/logs/netdump.log'", 
             getenv("PWD"), getenv("PWD"));
    printf("[Analyzer] Running\n");
    system(cmd);
}

void logging(Task* t) {
    printf("System monitoring and analysis complete.\n");
}

void init_scheduler() {
    for (int i = 0; i < MAX_PRIORITY; ++i) {
        priority_queues[i] = NULL;
    }
}

int main() {
    init_system();
    init_scheduler();
    Task* tasks[MAX_PRIORITY];
    pthread_t threads[MAX_PRIORITY];

    printf("\n=== SCHEDULER STARTING ===\n");
    printf("Task 1 (System Monitor): every 5 seconds\n");
    printf("Task 2 (Ethernet Fetch): every 10 seconds\n");
    printf("Task 3 (Analyzer): every 15 seconds\n");
    printf("Task 4 (Logging): every 10 seconds\n\n");

    // Create tasks
    tasks[0] = malloc(sizeof(Task));
    tasks[0]->id = 1;
    tasks[0]->priority = 0; // Highest
    tasks[0]->state = TASK_READY;
    tasks[0]->interval = 5; // 5 seconds
    tasks[0]->last_run = time(NULL) - 5;
    tasks[0]->cycle_count = 0;
    pthread_mutex_init(&tasks[0]->lock, NULL);
    tasks[0]->task_func = system_fetch;
    register_task(tasks[0]);

    tasks[1] = malloc(sizeof(Task));
    tasks[1]->id = 2;
    tasks[1]->priority = 1;
    tasks[1]->state = TASK_READY;
    tasks[1]->interval = 10; // 10 seconds
    tasks[1]->last_run = time(NULL) - 10;
    tasks[1]->cycle_count = 0;
    pthread_mutex_init(&tasks[1]->lock, NULL);
    tasks[1]->task_func = ethernet_fetch;
    register_task(tasks[1]);

    tasks[2] = malloc(sizeof(Task));
    tasks[2]->id = 3;
    tasks[2]->priority = 2;
    tasks[2]->state = TASK_READY;
    tasks[2]->interval = 15; // 15 seconds
    tasks[2]->last_run = time(NULL) - 15;
    tasks[2]->cycle_count = 0;
    pthread_mutex_init(&tasks[2]->lock, NULL);
    tasks[2]->task_func = analyze_data;
    register_task(tasks[2]);

    tasks[3] = malloc(sizeof(Task));
    tasks[3]->id = 4;
    tasks[3]->priority = 3; // Lowest
    tasks[3]->state = TASK_READY;
    tasks[3]->interval = 10;
    tasks[3]->last_run = time(NULL) - 10;
    tasks[3]->cycle_count = 0;
    pthread_mutex_init(&tasks[3]->lock, NULL);
    tasks[3]->task_func = logging;
    register_task(tasks[3]);

    // Start threads
    for (int i = 0; i < MAX_PRIORITY; ++i) {
        pthread_create(&threads[i], NULL, task_thread, tasks[i]);
    }

    // Scheduler main loop - dispatcher runs continuously
    int tick_count = 0;
    while (1) {
        tick_count++;
        printf("\n=== SCHEDULER TICK %d ===\n", tick_count);
        
        // Check each task's readiness based on interval
        for (int i = 0; i < MAX_PRIORITY; ++i) {
            Task* t = tasks[i];
            time_t now = time(NULL);
            
            // If task completed its quantum or waiting for interval, make it READY
            if (t->state == TASK_IDLE) {
                if ((now - t->last_run) >= t->interval) {
                    t->state = TASK_READY;
                    printf("[DISPATCHER] Task %d is now READY (interval due)\n", t->id);
                }
            }
        }
        
        // Find highest priority READY task and give it CPU
        scheduler_tick();
        
        sleep(2); // Main tick every 2 seconds
    }

    // Cleanup (not reached)
    for (int i = 0; i < MAX_PRIORITY; ++i) {
        pthread_cancel(threads[i]);
        pthread_join(threads[i], NULL);
        pthread_mutex_destroy(&tasks[i]->lock);
        free(tasks[i]);
    }
    return 0;
}

