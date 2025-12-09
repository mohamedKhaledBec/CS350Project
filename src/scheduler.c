// scheduler.c
// Sample code for 3 activities: system fetch, ethernet fetch, logging
// Each activity is a task with its own priority

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define MAX_PRIORITY 3

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
                pthread_mutex_unlock(&t->lock);
                t->state = TASK_RUNNING;
                return;
            }
            t = t->next;
        }
    }
}

void* task_thread(void* arg) {
    Task* t = (Task*)arg;
    while (1) {
        pthread_mutex_lock(&t->lock);
        t->task_func(t);
        t->state = TASK_IDLE;
        sleep(1); // Simulate waiting for next tick
        t->state = TASK_READY;
    }
    return NULL;
}

// Sample activities
void system_fetch(Task* t) {
    printf("[System Fetch] Task %d running.\n", t->id);
    // Simulate system monitoring
}

void ethernet_fetch(Task* t) {
    printf("[Ethernet Fetch] Task %d running.\n", t->id);
    // Simulate ethernet data fetching
}

void logging(Task* t) {
    printf("[Logging] Task %d running.\n", t->id);
    // Simulate logging activity
}

void init_scheduler() {
    for (int i = 0; i < MAX_PRIORITY; ++i) {
        priority_queues[i] = NULL;
    }
}

int main() {
    init_scheduler();
    Task* tasks[MAX_PRIORITY];
    pthread_t threads[MAX_PRIORITY];

    // Create tasks
    tasks[0] = malloc(sizeof(Task));
    tasks[0]->id = 1;
    tasks[0]->priority = 0; // Highest
    tasks[0]->state = TASK_READY;
    pthread_mutex_init(&tasks[0]->lock, NULL);
    pthread_mutex_lock(&tasks[0]->lock);
    tasks[0]->task_func = system_fetch;
    register_task(tasks[0]);

    tasks[1] = malloc(sizeof(Task));
    tasks[1]->id = 2;
    tasks[1]->priority = 1;
    tasks[1]->state = TASK_READY;
    pthread_mutex_init(&tasks[1]->lock, NULL);
    pthread_mutex_lock(&tasks[1]->lock);
    tasks[1]->task_func = ethernet_fetch;
    register_task(tasks[1]);

    tasks[2] = malloc(sizeof(Task));
    tasks[2]->id = 3;
    tasks[2]->priority = 2; // Lowest
    tasks[2]->state = TASK_READY;
    pthread_mutex_init(&tasks[2]->lock, NULL);
    pthread_mutex_lock(&tasks[2]->lock);
    tasks[2]->task_func = logging;
    register_task(tasks[2]);

    // Start threads
    for (int i = 0; i < MAX_PRIORITY; ++i) {
        pthread_create(&threads[i], NULL, task_thread, tasks[i]);
    }

    // Scheduler loop
    while (1) {
        scheduler_tick();
        sleep(1); // Tick rate
    }

    // Cleanup (not reached in this sample)
    for (int i = 0; i < MAX_PRIORITY; ++i) {
        pthread_join(threads[i], NULL);
        pthread_mutex_destroy(&tasks[i]->lock);
        free(tasks[i]);
    }
    return 0;
}

