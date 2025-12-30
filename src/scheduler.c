/**
 * @file scheduler.c
 * @brief Preemptive Priority Scheduler for Real-Time Linux System Monitor
 * 
 * Implements a real-time task scheduler with the following features:
 *   - Preemptive priority scheduling (4 priority levels, 0 = highest)
 *   - Time quantum-based execution (2 seconds per slice)
 *   - Thread-safe task management with mutexes and condition variables
 *   - Dynamic priority/interval adjustment via config files
 *   - Comprehensive logging system
 * 
 * Task Architecture:
 *   Task 1 (P0): System metrics collection (CPU, RAM, Disk)
 *   Task 2 (P1): Network/Ethernet monitoring
 *   Task 3 (P2): Data analysis and alert generation
 *   Task 4 (P3): Logging and report generation
 * 
 * @author CS350 Student Project
 * @date 2024
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <signal.h>

/*============================================================================
 *                              CONSTANTS
 *===========================================================================*/
#define MAX_PRIORITY    4       /* Number of priority levels (0-3) */
#define TIME_QUANTUM    2       /* Time slice per task in seconds */
#define VERBOSE_MODE    0       /* 1 = detailed output, 0 = quiet */
#define PRIORITY_CFG    "config/priority_override.conf"
#define INTERVAL_CFG    "config/interval_override.conf"

/*============================================================================
 *                           GLOBAL VARIABLES
 *===========================================================================*/
FILE* log_file = NULL;              /* Log file handle */
static volatile int g_running = 1;  /* Control flag for graceful shutdown */

/*============================================================================
 *                              LOGGING
 *===========================================================================*/
/**
 * @brief Thread-safe logging function with optional console output
 * @param format Printf-style format string
 * @param ... Variable arguments
 */
void log_message(const char* format, ...) {
    va_list args;
    
    // Always write to log file
    if (log_file) {
        va_start(args, format);
        vfprintf(log_file, format, args);
        fflush(log_file);
        va_end(args);
    }
    
    // Only print to terminal if verbose mode is on
    if (VERBOSE_MODE) {
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
    }
}

/*============================================================================
 *                            DATA TYPES
 *===========================================================================*/
/**
 * @brief Task execution states
 */
typedef enum {
    TASK_READY,     /* Task is ready to execute */
    TASK_RUNNING,   /* Task is currently executing */
    TASK_IDLE,      /* Task completed its work, waiting for next interval */
    TASK_PREEMPTED  /* Task was interrupted by higher priority task */
} task_state_t;

/**
 * @brief Task Control Block (TCB)
 * 
 * Contains all information needed to manage a schedulable task.
 */
typedef struct Task {
    int id;                     /* Unique task identifier (1-4) */
    int priority;               /* Priority level (0=highest, 3=lowest) */
    task_state_t state;         /* Current execution state */
    pthread_mutex_t lock;       /* Mutex for thread-safe access */
    pthread_cond_t cond;        /* Condition variable for synchronization */
    void (*task_func)(struct Task*);  /* Task function pointer */
    struct Task* next;          /* Next task in priority queue */
    time_t last_run;            /* Timestamp of last execution */
    int interval;               /* Execution interval in seconds */
    int cycle_count;            /* Number of scheduling cycles */
    time_t slice_start;         /* Start time of current time slice */
    int time_used;              /* Time consumed in current slice */
    int preempt_flag;           /* Flag indicating preemption request */
    int execution_count;        /* Total number of executions */
} Task;

/*============================================================================
 *                         SCHEDULER DATA STRUCTURES
 *===========================================================================*/
Task* priority_queues[MAX_PRIORITY];    /* Priority-based ready queues */
Task* tasks_list[MAX_PRIORITY];         /* Master list of all tasks */
Task* current_running_task = NULL;      /* Currently executing task */
pthread_mutex_t scheduler_lock;         /* Global scheduler lock */

/* Forward declarations */
void register_task(Task* task);

/*============================================================================
 *                          UTILITY FUNCTIONS
 *===========================================================================*/
/**
 * @brief Signal handler for graceful shutdown
 * @param sig Signal number
 */
static void signal_handler(int sig) {
    (void)sig;
    printf("\nReceived shutdown signal, cleaning up...\n");
    g_running = 0;
}

/**
 * @brief Convert task state enum to string
 * @param s Task state
 * @return String representation of state
 */
static const char* state_name(task_state_t s) {
    switch (s) {
        case TASK_READY: return "READY";
        case TASK_RUNNING: return "RUNNING";
        case TASK_IDLE: return "IDLE";
        case TASK_PREEMPTED: return "PREEMPTED";
        default: return "UNKNOWN";
    }
}

static void print_tick_status(int tick) {
    printf("\n========== TICK %d ==========%s\n", tick, VERBOSE_MODE ? " (verbose off)" : "");
    printf("ID  PRI   STATE       EXEC  LAST_RUN_DIFF(s)\n");
    printf("-------------------------------------------\n");

    pthread_mutex_lock(&scheduler_lock);
    for (int i = 0; i < MAX_PRIORITY; ++i) {
        Task* t = tasks_list[i];
        if (!t) { continue; }
        pthread_mutex_lock(&t->lock);
        time_t now = time(NULL);
        int since = (int)difftime(now, t->last_run);
        printf("%-2d  %-3d  %-10s  %-4d  %4d\n",
               t->id,
               t->priority,
               state_name(t->state),
               t->execution_count,
               since);
        pthread_mutex_unlock(&t->lock);
    }
    pthread_mutex_unlock(&scheduler_lock);
    printf("-------------------------------------------\n");
}

static void clear_priority_queues(void) {
    for (int i = 0; i < MAX_PRIORITY; ++i) {
        priority_queues[i] = NULL;
    }
}

static void rebuild_priority_queues(void) {
    clear_priority_queues();
    for (int i = 0; i < MAX_PRIORITY; ++i) {
        if (tasks_list[i]) {
            register_task(tasks_list[i]);
        }
    }
    current_running_task = NULL;
}

static void apply_priority_overrides(void) {
    FILE* fp = fopen(PRIORITY_CFG, "r");
    int changed = 0;
    
    if (fp) {
        char line[128];
        while (fgets(line, sizeof(line), fp)) {
            int id = -1;
            int new_p = -1;
            if (sscanf(line, "%d %d", &id, &new_p) == 2) {
                if (id >= 1 && id <= MAX_PRIORITY && new_p >= 0 && new_p < MAX_PRIORITY) {
                    Task* t = tasks_list[id - 1];
                    if (t && t->priority != new_p) {
                        printf("↺ Updating Task %d priority: %d -> %d\n", id, t->priority, new_p);
                        log_message("[PRIORITY] Task %d priority changed from %d to %d\n", id, t->priority, new_p);
                        t->priority = new_p;
                        t->state = TASK_READY;
                        t->preempt_flag = 0;
                        changed = 1;
                    }
                }
            }
        }
        fclose(fp);
    }

    fp = fopen(INTERVAL_CFG, "r");
    if (fp) {
        char line[128];
        while (fgets(line, sizeof(line), fp)) {
            int id = -1;
            int new_intv = -1;
            if (sscanf(line, "%d %d", &id, &new_intv) == 2) {
                if (id >= 1 && id <= MAX_PRIORITY && new_intv >= 1 && new_intv <= 300) {
                    Task* t = tasks_list[id - 1];
                    if (t && t->interval != new_intv) {
                        printf("↻ Updating Task %d interval: %d -> %d seconds\n", id, t->interval, new_intv);
                        log_message("[INTERVAL] Task %d interval changed from %d to %d seconds\n", id, t->interval, new_intv);
                        t->interval = new_intv;
                        changed = 1;
                    }
                }
            }
        }
        fclose(fp);
    }

    if (changed) {
        pthread_mutex_lock(&scheduler_lock);

        // Preempt any currently running task so it can be rescheduled
        if (current_running_task) {
            pthread_mutex_lock(&current_running_task->lock);
            current_running_task->preempt_flag = 1;
            current_running_task->state = TASK_PREEMPTED;
            pthread_cond_signal(&current_running_task->cond);
            pthread_mutex_unlock(&current_running_task->lock);
            current_running_task = NULL;
        }

        // Reset all tasks to READY and clear preempt flags
        for (int i = 0; i < MAX_PRIORITY; ++i) {
            if (tasks_list[i]) {
                pthread_mutex_lock(&tasks_list[i]->lock);
                tasks_list[i]->state = TASK_READY;
                tasks_list[i]->preempt_flag = 0;
                pthread_mutex_unlock(&tasks_list[i]->lock);
            }
        }

        rebuild_priority_queues();
        pthread_mutex_unlock(&scheduler_lock);
    }
}

void register_task(Task* task) {
    int p = task->priority;
    task->next = priority_queues[p];
    priority_queues[p] = task;
}

// Check if a higher priority task needs to preempt current
void check_preemption() {
    if (!current_running_task) return;
    
    // Check if any higher priority task is READY
    for (int p = 0; p < current_running_task->priority; p++) {
        Task* t = priority_queues[p];
        while (t) {
            if (t->state == TASK_READY) {
                // Found higher priority task - preempt current
                log_message("[PREEMPTION] Task %d (Priority %d) preempting Task %d (Priority %d)\n",
                       t->id, t->priority, current_running_task->id, current_running_task->priority);
                printf("⚡ PREEMPTION: Task %d → Task %d\n", 
                       current_running_task->id, t->id);
                
                pthread_mutex_lock(&current_running_task->lock);
                current_running_task->preempt_flag = 1;
                current_running_task->state = TASK_PREEMPTED;
                pthread_cond_signal(&current_running_task->cond);
                pthread_mutex_unlock(&current_running_task->lock);
                return;
            }
            t = t->next;
        }
    }
}

void scheduler_tick() {
    pthread_mutex_lock(&scheduler_lock);
    
    // First check if we need to preempt current task
    check_preemption();
    
    // Find highest priority READY task
    for (int p = 0; p < MAX_PRIORITY; ++p) {
        Task* t = priority_queues[p];
        while (t) {
            if (t->state == TASK_READY) {
                // Mark task as RUNNING
                t->state = TASK_RUNNING;
                t->slice_start = time(NULL);
                t->time_used = 0;
                t->preempt_flag = 0;
                current_running_task = t;
                
                // Signal the task thread to wake up and run
                pthread_mutex_lock(&t->lock);
                pthread_cond_signal(&t->cond);
                pthread_mutex_unlock(&t->lock);
                
                log_message("[SCHEDULER] Task %d (Priority %d) gets CPU - Time Quantum: %d sec\n", 
                       t->id, t->priority, TIME_QUANTUM);
                if (!VERBOSE_MODE) {
                    printf("→ Task %d running (P%d)\n", t->id, t->priority);
                }
                pthread_mutex_unlock(&scheduler_lock);
                return;
            }
            t = t->next;
        }
    }
    pthread_mutex_unlock(&scheduler_lock);
}

void* task_thread(void* arg) {
    Task* t = (Task*)arg;
    
    while (1) {
        pthread_mutex_lock(&t->lock);
        
        // Wait until scheduled to run
        while (t->state != TASK_RUNNING && t->state != TASK_PREEMPTED) {
            pthread_cond_wait(&t->cond, &t->lock);
        }
        
        // Handle preempted state - return to READY
        if (t->state == TASK_PREEMPTED) {
            t->state = TASK_READY;
            log_message("[TASK %d] Was preempted, returning to READY state\n", t->id);
            pthread_mutex_unlock(&t->lock);
            continue;
        }
        
        time_t now = time(NULL);
        log_message("[TASK %d] Starting execution\n", t->id);
        
        pthread_mutex_unlock(&t->lock);
        
        // Execute work - check preempt_flag periodically
        t->task_func(t);
        
        pthread_mutex_lock(&t->lock);
        
        // Check if we were preempted during execution
        if (t->preempt_flag) {
            log_message("[TASK %d] Execution interrupted by preemption\n", t->id);
            t->state = TASK_READY;  // Will be rescheduled
            t->preempt_flag = 0;
        } else {
            t->last_run = now;
            t->execution_count++;
            t->time_used = (int)(time(NULL) - t->slice_start);
            log_message("[TASK %d] Completed (Used: %d sec, Total executions: %d)\n", 
                   t->id, t->time_used, t->execution_count);
            
            pthread_mutex_lock(&scheduler_lock);
            if (current_running_task == t) {
                current_running_task = NULL;
            }
            pthread_mutex_unlock(&scheduler_lock);
            
            t->state = TASK_IDLE;
            log_message("[TASK %d] Yielding CPU\n\n", t->id);
        }
        
        pthread_mutex_unlock(&t->lock);
    }
    return NULL;
}

/*============================================================================
 *                        SYSTEM INITIALIZATION
 *===========================================================================*/
/**
 * @brief Initialize system resources and directories
 * 
 * Creates necessary directories, opens log file, and prepares
 * shell scripts for execution.
 */
void init_system() {
    printf("Initializing Real-Time System Monitor...\n");
    
    /* Setup signal handlers for graceful shutdown */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* Open log file */
    log_file = fopen("logs/scheduler.log", "w");
    if (log_file) {
        fprintf(log_file, "=== SCHEDULER LOG START ===\n");
        fprintf(log_file, "Timestamp: %s\n", __DATE__ " " __TIME__);
        fprintf(log_file, "Verbose Mode: %s\n\n", VERBOSE_MODE ? "ON" : "OFF");
    }
    
    // Print current working directory
    char cwd[512];
    getcwd(cwd, sizeof(cwd));
    printf("Current directory: %s\n", cwd);
    
    // Make scripts executable
    system("chmod +x scripts/monitor_scheduler.sh 2>/dev/null");
    system("chmod +x scripts/ethernet_fetch.sh 2>/dev/null");
    
    // Create necessary directories
    system("mkdir -p logs 2>/dev/null");
    system("mkdir -p data 2>/dev/null");
    system("mkdir -p config 2>/dev/null");
    
    printf("System initialized.\n");
    printf("Verbose mode: %s (change VERBOSE_MODE in source to toggle)\n", 
           VERBOSE_MODE ? "ON" : "OFF");
    printf("Scheduler log: logs/scheduler.log\n\n");
}

/*============================================================================
 *                            TASK FUNCTIONS
 *===========================================================================*/
/**
 * @brief Task 1: System metrics collection (CPU, RAM, Disk)
 * @param t Pointer to the executing task's TCB
 */
void system_fetch(Task* t) {
    (void)t; /* Unused - task info available if needed */
    log_message("[System Monitor] Running\n");
    system("bash scripts/monitor_scheduler.sh");
}

/**
 * @brief Task 2: Network/Ethernet monitoring
 * @param t Pointer to the executing task's TCB
 */
void ethernet_fetch(Task* t) {
    (void)t;
    log_message("[Ethernet Fetch] Running\n");
    system("bash scripts/ethernet_fetch.sh");
}

/**
 * @brief Task 3: Data analysis and alert generation
 * @param t Pointer to the executing task's TCB
 */
void analyze_data(Task* t) {
    (void)t;
    log_message("[Analyzer] Running\n");
    system("./build/analyze 50.0 60.0 75.0 logs/netdump.log");
}

/**
 * @brief Task 4: Logging and status reporting
 * @param t Pointer to the executing task's TCB
 */
void logging(Task* t) {
    (void)t;
    log_message("[Logger] System monitoring cycle complete.\n");
    printf("✓ Monitoring cycle complete\n");
}

/**
 * @brief Initialize scheduler data structures
 * 
 * Clears priority queues, task lists, and initializes the global scheduler lock.
 */
void init_scheduler() {
    clear_priority_queues();
    for (int i = 0; i < MAX_PRIORITY; ++i) {
        tasks_list[i] = NULL;
    }
    current_running_task = NULL;
    pthread_mutex_init(&scheduler_lock, NULL);
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
    tasks[0]->preempt_flag = 0;
    tasks[0]->execution_count = 0;
    pthread_mutex_init(&tasks[0]->lock, NULL);
    pthread_cond_init(&tasks[0]->cond, NULL);
    tasks[0]->task_func = system_fetch;
    tasks_list[0] = tasks[0];
    register_task(tasks[0]);

    tasks[1] = malloc(sizeof(Task));
    tasks[1]->id = 2;
    tasks[1]->priority = 1;
    tasks[1]->state = TASK_READY;
    tasks[1]->interval = 10; // 10 seconds
    tasks[1]->last_run = time(NULL) - 10;
    tasks[1]->cycle_count = 0;
    tasks[1]->preempt_flag = 0;
    tasks[1]->execution_count = 0;
    pthread_mutex_init(&tasks[1]->lock, NULL);
    pthread_cond_init(&tasks[1]->cond, NULL);
    tasks[1]->task_func = ethernet_fetch;
    tasks_list[1] = tasks[1];
    register_task(tasks[1]);

    tasks[2] = malloc(sizeof(Task));
    tasks[2]->id = 3;
    tasks[2]->priority = 2;
    tasks[2]->state = TASK_READY;
    tasks[2]->interval = 15; // 15 seconds
    tasks[2]->last_run = time(NULL) - 15;
    tasks[2]->cycle_count = 0;
    tasks[2]->preempt_flag = 0;
    tasks[2]->execution_count = 0;
    pthread_mutex_init(&tasks[2]->lock, NULL);
    pthread_cond_init(&tasks[2]->cond, NULL);
    tasks[2]->task_func = analyze_data;
    tasks_list[2] = tasks[2];
    register_task(tasks[2]);

    tasks[3] = malloc(sizeof(Task));
    tasks[3]->id = 4;
    tasks[3]->priority = 3; // Lowest
    tasks[3]->state = TASK_READY;
    tasks[3]->interval = 10;
    tasks[3]->last_run = time(NULL) - 10;
    tasks[3]->cycle_count = 0;
    tasks[3]->preempt_flag = 0;
    tasks[3]->execution_count = 0;
    pthread_mutex_init(&tasks[3]->lock, NULL);
    pthread_cond_init(&tasks[3]->cond, NULL);
    tasks[3]->task_func = logging;
    tasks_list[3] = tasks[3];
    register_task(tasks[3]);

    // Start threads
    for (int i = 0; i < MAX_PRIORITY; ++i) {
        pthread_create(&threads[i], NULL, task_thread, tasks[i]);
    }

    // Scheduler main loop - dispatcher runs continuously
    int tick_count = 0;
    while (1) {
        tick_count++;
        log_message("\n=== SCHEDULER TICK %d ===\n", tick_count);
        if (!VERBOSE_MODE && tick_count % 5 == 1) {
            printf("\n[Tick %d] Scheduler running...\n", tick_count);
        }

        // Print a clear snapshot each tick
        print_tick_status(tick_count);

        // Apply any priority overrides from UI
        apply_priority_overrides();
        
        // Check each task's readiness based on interval
        for (int i = 0; i < MAX_PRIORITY; ++i) {
            Task* t = tasks_list[i];
            if (!t) { continue; }
            time_t now = time(NULL);
            
            // If task completed its quantum or waiting for interval, make it READY
            if (t->state == TASK_IDLE || t->state == TASK_PREEMPTED) {
                if ((now - t->last_run) >= t->interval) {
                    pthread_mutex_lock(&t->lock);
                    t->state = TASK_READY;
                    pthread_mutex_unlock(&t->lock);
                    log_message("[DISPATCHER] Task %d is now READY (interval due)\n", t->id);
                }
            }
        }
        
        // Find highest priority READY task and give it CPU
        scheduler_tick();
        
        sleep(2); // Main tick every 2 seconds
    }

    // Cleanup (not reached)
    if (log_file) {
        fprintf(log_file, "\n=== SCHEDULER LOG END ===\n");
        fclose(log_file);
    }
    
    for (int i = 0; i < MAX_PRIORITY; ++i) {
        pthread_cancel(threads[i]);
        pthread_join(threads[i], NULL);
        pthread_mutex_destroy(&tasks[i]->lock);
        free(tasks[i]);
    }
    return 0;
}

