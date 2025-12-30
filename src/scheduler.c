/*
 * ============================================================================
 * File:        scheduler.c
 * Project:     CS350 Real-Time System Monitor
 * Description: Preemptive Priority Scheduler with Starvation Prevention
 * Author:      CS350 Student Project
 * Date:        2024
 * ============================================================================
 * 
 * This scheduler implements:
 *   - Preemptive priority scheduling (4 levels, 0 = highest)
 *   - Time quantum-based execution (configurable)
 *   - Thread-safe task management
 *   - Dynamic priority/interval adjustment
 *   - Starvation detection and warnings
 *   - Graceful shutdown handling
 * 
 * ============================================================================
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
#include <float.h>

/* ============================================================================
 *                              CONFIGURATION
 * ============================================================================ */

#define MAX_TASKS           4           /* Total number of tasks                */
#define MAX_PRIORITY        4           /* Priority levels (0-3)                */
#define TIME_QUANTUM        2           /* Time slice in seconds                */
#define STARVATION_THRESHOLD 30         /* Seconds before starvation warning    */
#define VERBOSE_MODE        0           /* 1 = detailed output, 0 = quiet       */

#define PRIORITY_CFG        "config/priority_override.conf"
#define INTERVAL_CFG        "config/interval_override.conf"
#define LOG_FILE            "logs/scheduler.log"

/* ============================================================================
 *                              ANSI COLORS
 * ============================================================================ */

#define CLR_RESET   "\033[0m"
#define CLR_RED     "\033[1;31m"
#define CLR_GREEN   "\033[1;32m"
#define CLR_YELLOW  "\033[1;33m"
#define CLR_BLUE    "\033[1;34m"
#define CLR_CYAN    "\033[1;36m"
#define CLR_WHITE   "\033[1;37m"

/* ============================================================================
 *                              DATA TYPES
 * ============================================================================ */

typedef enum {
    TASK_READY,         /* Ready to execute                     */
    TASK_RUNNING,       /* Currently executing                  */
    TASK_IDLE,          /* Waiting for next interval            */
    TASK_PREEMPTED      /* Interrupted by higher priority       */
} TaskState;

typedef struct Task {
    int             id;                 /* Unique identifier (1-4)          */
    char            name[32];           /* Human-readable name              */
    int             priority;           /* Priority level (0=highest)       */
    TaskState       state;              /* Current state                    */
    pthread_mutex_t lock;               /* Per-task mutex                   */
    pthread_cond_t  cond;               /* Condition variable               */
    void            (*func)(struct Task*);  /* Task function pointer        */
    struct Task*    next;               /* Queue linkage                    */
    
    /* Timing */
    time_t          last_run;           /* Last execution timestamp         */
    int             interval;           /* Execution interval (seconds)     */
    time_t          slice_start;        /* Current slice start time         */
    int             time_used;          /* Time used in current slice       */
    
    /* Statistics */
    int             exec_count;         /* Total executions                 */
    int             preempt_count;      /* Times preempted                  */
    time_t          last_completed;     /* Last successful completion       */
    int             preempt_flag;       /* Preemption signal flag           */
} Task;

/* ============================================================================
 *                            GLOBAL VARIABLES
 * ============================================================================ */

static FILE*            g_log_file = NULL;              /* Log file handle      */
static volatile int     g_running = 1;                  /* Shutdown flag        */
static Task*            g_priority_queues[MAX_PRIORITY];/* Priority queues      */
static Task*            g_tasks[MAX_TASKS];             /* All tasks array      */
static Task*            g_current_task = NULL;          /* Currently running    */
static pthread_mutex_t  g_scheduler_lock;               /* Global lock          */
static int              g_tick_count = 0;               /* Scheduler ticks      */

/* Task names for display */
static const char* TASK_NAMES[] = {
    "System Monitor",
    "Network Fetch",
    "Analyzer",
    "Report Generator"
};

/* ============================================================================
 *                            FUNCTION PROTOTYPES
 * ============================================================================ */

/* Core scheduler functions */
static void     scheduler_init(void);
static void     scheduler_tick(void);
static void     scheduler_cleanup(void);
static void     check_preemption(void);
static void     register_task(Task* task);

/* Task management */
static Task*    create_task(int id, const char* name, int priority, int interval, void (*func)(Task*));
static void     destroy_task(Task* task);
static void*    task_thread(void* arg);

/* Starvation detection */
static void     check_starvation(void);
static int      validate_config_change(int task_id, int new_priority, int new_interval);

/* Configuration */
static void     apply_config_overrides(void);
static void     load_priority_config(void);
static void     load_interval_config(void);

/* Utility functions */
static void     log_msg(const char* format, ...);
static void     print_status(void);
static void     print_banner(void);
static const char* state_to_string(TaskState s);
static void     signal_handler(int sig);

/* Task implementations */
static void     task_system_monitor(Task* t);
static void     task_network_fetch(Task* t);
static void     task_analyzer(Task* t);
static void     task_reporter(Task* t);

/* ============================================================================
 *                              LOGGING
 * ============================================================================ */

static void log_msg(const char* format, ...) {
    va_list args;
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char timestamp[32];
    
    strftime(timestamp, sizeof(timestamp), "%H:%M:%S", tm_info);
    
    /* Write to log file */
    if (g_log_file) {
        fprintf(g_log_file, "[%s] ", timestamp);
        va_start(args, format);
        vfprintf(g_log_file, format, args);
        va_end(args);
        fflush(g_log_file);
    }
    
    /* Console output if verbose */
    if (VERBOSE_MODE) {
        printf("[%s] ", timestamp);
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
    }
}

/* ============================================================================
 *                            STATE CONVERSION
 * ============================================================================ */

static const char* state_to_string(TaskState s) {
    switch (s) {
        case TASK_READY:     return "READY";
        case TASK_RUNNING:   return "RUNNING";
        case TASK_IDLE:      return "IDLE";
        case TASK_PREEMPTED: return "PREEMPTED";
        default:             return "UNKNOWN";
    }
}

/* ============================================================================
 *                           SIGNAL HANDLING
 * ============================================================================ */

static void signal_handler(int sig) {
    (void)sig;
    printf("\n%s[SHUTDOWN]%s Received signal, stopping scheduler...%s\n", 
           CLR_YELLOW, CLR_WHITE, CLR_RESET);
    g_running = 0;
}

/* ============================================================================
 *                          STARVATION DETECTION
 * ============================================================================ */

static void check_starvation(void) {
    time_t now = time(NULL);
    
    for (int i = 0; i < MAX_TASKS; i++) {
        Task* t = g_tasks[i];
        if (!t) continue;
        
        /* Calculate time since last execution */
        int idle_time = (int)difftime(now, t->last_completed);
        
        /* Check for potential starvation */
        if (idle_time > STARVATION_THRESHOLD && t->last_completed > 0) {
            printf("%s[WARNING]%s Task '%s' (P%d) hasn't run for %d seconds - potential starvation!%s\n",
                   CLR_RED, CLR_YELLOW, t->name, t->priority, idle_time, CLR_RESET);
            log_msg("[STARVATION WARNING] Task %d (%s) idle for %d seconds\n", 
                    t->id, t->name, idle_time);
        }
    }
}

/*
 * Validates if a configuration change might cause starvation
 * Returns: 1 if safe, 0 if might cause starvation
 */
static int validate_config_change(int task_id, int new_priority, int new_interval) {
    if (task_id < 1 || task_id > MAX_TASKS) return 0;
    
    Task* t = g_tasks[task_id - 1];
    if (!t) return 0;
    
    int problems = 0;
    
    /* Check if lowering priority of a frequently needed task */
    if (new_priority > t->priority) {
        /* Count how many tasks will have higher priority */
        int higher_count = 0;
        for (int i = 0; i < MAX_TASKS; i++) {
            if (g_tasks[i] && g_tasks[i]->priority < new_priority) {
                higher_count++;
            }
        }
        
        if (higher_count >= 3) {
            printf("%s[CONFIG WARNING]%s Lowering Task %d to P%d: %d tasks will have higher priority%s\n",
                   CLR_YELLOW, CLR_WHITE, task_id, new_priority, higher_count, CLR_RESET);
            problems++;
        }
    }
    
    /* Check if interval is too long for low priority task */
    if (new_priority >= 2 && new_interval > 20) {
        printf("%s[CONFIG WARNING]%s Task %d (P%d) with interval %ds may experience delays%s\n",
               CLR_YELLOW, CLR_WHITE, task_id, new_priority, new_interval, CLR_RESET);
        problems++;
    }
    
    /* Check if too many tasks have same priority */
    int same_priority_count = 0;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (g_tasks[i] && i != (task_id - 1) && g_tasks[i]->priority == new_priority) {
            same_priority_count++;
        }
    }
    
    if (same_priority_count >= 2) {
        printf("%s[CONFIG WARNING]%s %d other tasks share priority P%d - may cause delays%s\n",
               CLR_YELLOW, CLR_WHITE, same_priority_count, new_priority, CLR_RESET);
        problems++;
    }
    
    return (problems == 0);
}

/* ============================================================================
 *                         CONFIGURATION LOADING
 * ============================================================================ */

static void load_priority_config(void) {
    FILE* fp = fopen(PRIORITY_CFG, "r");
    if (!fp) return;
    
    char line[128];
    while (fgets(line, sizeof(line), fp)) {
        /* Skip comments and empty lines */
        if (line[0] == '#' || line[0] == '\n') continue;
        
        int id, new_priority;
        if (sscanf(line, "%d %d", &id, &new_priority) == 2) {
            if (id >= 1 && id <= MAX_TASKS && new_priority >= 0 && new_priority < MAX_PRIORITY) {
                Task* t = g_tasks[id - 1];
                if (t && t->priority != new_priority) {
                    /* Validate change */
                    validate_config_change(id, new_priority, t->interval);
                    
                    printf("%s[CONFIG]%s Task %d priority: %d -> %d%s\n",
                           CLR_CYAN, CLR_WHITE, id, t->priority, new_priority, CLR_RESET);
                    log_msg("[CONFIG] Task %d priority changed: %d -> %d\n", 
                            id, t->priority, new_priority);
                    
                    pthread_mutex_lock(&t->lock);
                    t->priority = new_priority;
                    t->state = TASK_READY;
                    pthread_mutex_unlock(&t->lock);
                }
            }
        }
    }
    fclose(fp);
}

static void load_interval_config(void) {
    FILE* fp = fopen(INTERVAL_CFG, "r");
    if (!fp) return;
    
    char line[128];
    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        
        int id, new_interval;
        if (sscanf(line, "%d %d", &id, &new_interval) == 2) {
            if (id >= 1 && id <= MAX_TASKS && new_interval >= 1 && new_interval <= 300) {
                Task* t = g_tasks[id - 1];
                if (t && t->interval != new_interval) {
                    /* Validate change */
                    validate_config_change(id, t->priority, new_interval);
                    
                    printf("%s[CONFIG]%s Task %d interval: %ds -> %ds%s\n",
                           CLR_CYAN, CLR_WHITE, id, t->interval, new_interval, CLR_RESET);
                    log_msg("[CONFIG] Task %d interval changed: %d -> %d\n", 
                            id, t->interval, new_interval);
                    
                    pthread_mutex_lock(&t->lock);
                    t->interval = new_interval;
                    pthread_mutex_unlock(&t->lock);
                }
            }
        }
    }
    fclose(fp);
}

static void apply_config_overrides(void) {
    load_priority_config();
    load_interval_config();
}

/* ============================================================================
 *                            TASK CREATION
 * ============================================================================ */

static Task* create_task(int id, const char* name, int priority, int interval, void (*func)(Task*)) {
    Task* t = (Task*)calloc(1, sizeof(Task));
    if (!t) {
        perror("Failed to allocate task");
        return NULL;
    }
    
    t->id = id;
    strncpy(t->name, name, sizeof(t->name) - 1);
    t->priority = priority;
    t->interval = interval;
    t->func = func;
    t->state = TASK_READY;
    t->last_run = time(NULL) - interval;  /* Make eligible immediately */
    t->last_completed = 0;
    t->exec_count = 0;
    t->preempt_count = 0;
    t->preempt_flag = 0;
    
    pthread_mutex_init(&t->lock, NULL);
    pthread_cond_init(&t->cond, NULL);
    
    return t;
}

static void destroy_task(Task* task) {
    if (!task) return;
    pthread_mutex_destroy(&task->lock);
    pthread_cond_destroy(&task->cond);
    free(task);
}

/* ============================================================================
 *                              TASK QUEUE
 * ============================================================================ */

static void clear_queues(void) {
    for (int i = 0; i < MAX_PRIORITY; i++) {
        g_priority_queues[i] = NULL;
    }
}

static void register_task(Task* task) {
    int p = task->priority;
    task->next = g_priority_queues[p];
    g_priority_queues[p] = task;
}

static void rebuild_queues(void) {
    clear_queues();
    for (int i = 0; i < MAX_TASKS; i++) {
        if (g_tasks[i]) {
            register_task(g_tasks[i]);
        }
    }
}

/* ============================================================================
 *                            PREEMPTION CHECK
 * ============================================================================ */

static void check_preemption(void) {
    if (!g_current_task) return;
    
    /* Look for higher priority ready tasks */
    for (int p = 0; p < g_current_task->priority; p++) {
        Task* t = g_priority_queues[p];
        while (t) {
            if (t->state == TASK_READY) {
                printf("%s[PREEMPT]%s %s (P%d) preempts %s (P%d)%s\n",
                       CLR_RED, CLR_WHITE,
                       t->name, t->priority,
                       g_current_task->name, g_current_task->priority,
                       CLR_RESET);
                
                log_msg("[PREEMPT] Task %d preempts Task %d\n", t->id, g_current_task->id);
                
                pthread_mutex_lock(&g_current_task->lock);
                g_current_task->preempt_flag = 1;
                g_current_task->state = TASK_PREEMPTED;
                g_current_task->preempt_count++;
                pthread_cond_signal(&g_current_task->cond);
                pthread_mutex_unlock(&g_current_task->lock);
                return;
            }
            t = t->next;
        }
    }
}

/* ============================================================================
 *                           SCHEDULER TICK
 * ============================================================================ */

static void scheduler_tick(void) {
    pthread_mutex_lock(&g_scheduler_lock);
    
    check_preemption();
    
    /* Find highest priority READY task */
    for (int p = 0; p < MAX_PRIORITY; p++) {
        Task* t = g_priority_queues[p];
        while (t) {
            if (t->state == TASK_READY) {
                t->state = TASK_RUNNING;
                t->slice_start = time(NULL);
                t->time_used = 0;
                t->preempt_flag = 0;
                g_current_task = t;
                
                pthread_mutex_lock(&t->lock);
                pthread_cond_signal(&t->cond);
                pthread_mutex_unlock(&t->lock);
                
                log_msg("[DISPATCH] Task %d (%s) gets CPU\n", t->id, t->name);
                pthread_mutex_unlock(&g_scheduler_lock);
                return;
            }
            t = t->next;
        }
    }
    
    pthread_mutex_unlock(&g_scheduler_lock);
}

/* ============================================================================
 *                            TASK THREAD
 * ============================================================================ */

static void* task_thread(void* arg) {
    Task* t = (Task*)arg;
    
    while (g_running) {
        pthread_mutex_lock(&t->lock);
        
        /* Wait until scheduled */
        while (t->state != TASK_RUNNING && t->state != TASK_PREEMPTED && g_running) {
            pthread_cond_wait(&t->cond, &t->lock);
        }
        
        if (!g_running) {
            pthread_mutex_unlock(&t->lock);
            break;
        }
        
        /* Handle preemption */
        if (t->state == TASK_PREEMPTED) {
            t->state = TASK_READY;
            pthread_mutex_unlock(&t->lock);
            continue;
        }
        
        time_t start = time(NULL);
        pthread_mutex_unlock(&t->lock);
        
        /* Execute task function */
        t->func(t);
        
        pthread_mutex_lock(&t->lock);
        
        if (t->preempt_flag) {
            t->state = TASK_READY;
            t->preempt_flag = 0;
        } else {
            t->last_run = start;
            t->last_completed = time(NULL);
            t->exec_count++;
            t->time_used = (int)(time(NULL) - t->slice_start);
            t->state = TASK_IDLE;
            
            pthread_mutex_lock(&g_scheduler_lock);
            if (g_current_task == t) {
                g_current_task = NULL;
            }
            pthread_mutex_unlock(&g_scheduler_lock);
        }
        
        pthread_mutex_unlock(&t->lock);
    }
    
    return NULL;
}

/* ============================================================================
 *                          TASK IMPLEMENTATIONS
 * ============================================================================ */

static void task_system_monitor(Task* t) {
    (void)t;
    log_msg("[TASK] System Monitor running\n");
    system("bash scripts/monitor_scheduler.sh > /dev/null 2>&1");
}

static void task_network_fetch(Task* t) {
    (void)t;
    log_msg("[TASK] Network Fetch running\n");
    system("bash scripts/ethernet_fetch.sh > /dev/null 2>&1");
}

static void task_analyzer(Task* t) {
    (void)t;
    log_msg("[TASK] Analyzer running\n");
    system("./build/analyze 50.0 60.0 75.0 logs/netdump.log > /dev/null 2>&1");
}

static void task_reporter(Task* t) {
    (void)t;
    log_msg("[TASK] Report Generator running\n");
    system("./build/report_generator logs/system_report.html > /dev/null 2>&1");
}

/* ============================================================================
 *                            STATUS DISPLAY
 * ============================================================================ */

static void print_banner(void) {
    printf("\n");
    printf("%s", CLR_CYAN);
    printf("  ╔═══════════════════════════════════════════════════════════════╗\n");
    printf("  ║     %sCS350 Real-Time System Monitor%s - Scheduler v1.0%s         ║\n", 
           CLR_WHITE, CLR_CYAN, CLR_CYAN);
    printf("  ╚═══════════════════════════════════════════════════════════════╝\n");
    printf("%s\n", CLR_RESET);
}

static void print_status(void) {
    time_t now = time(NULL);
    
    printf("\n%s┌─────────────────────────────────────────────────────────────────┐%s\n", CLR_BLUE, CLR_RESET);
    printf("%s│%s  TICK %-4d │ Tasks: %d │ Time Quantum: %ds                     %s│%s\n",
           CLR_BLUE, CLR_WHITE, g_tick_count, MAX_TASKS, TIME_QUANTUM, CLR_BLUE, CLR_RESET);
    printf("%s├─────────────────────────────────────────────────────────────────┤%s\n", CLR_BLUE, CLR_RESET);
    printf("%s│%s  ID   Name                  Pri   State      Runs   Last Run   %s│%s\n", 
           CLR_BLUE, CLR_CYAN, CLR_BLUE, CLR_RESET);
    printf("%s├─────────────────────────────────────────────────────────────────┤%s\n", CLR_BLUE, CLR_RESET);
    
    for (int i = 0; i < MAX_TASKS; i++) {
        Task* t = g_tasks[i];
        if (!t) continue;
        
        pthread_mutex_lock(&t->lock);
        
        int since_run = (t->last_completed > 0) ? (int)difftime(now, t->last_completed) : -1;
        const char* state_color;
        
        switch (t->state) {
            case TASK_RUNNING:   state_color = CLR_GREEN; break;
            case TASK_READY:     state_color = CLR_YELLOW; break;
            case TASK_PREEMPTED: state_color = CLR_RED; break;
            default:             state_color = CLR_WHITE; break;
        }
        
        char last_run_str[16];
        if (since_run >= 0) {
            snprintf(last_run_str, sizeof(last_run_str), "%ds ago", since_run);
        } else {
            snprintf(last_run_str, sizeof(last_run_str), "Never");
        }
        
        printf("%s│%s  %-4d %-20s  P%-2d   %s%-10s%s %-6d %-10s %s│%s\n",
               CLR_BLUE, CLR_WHITE,
               t->id,
               t->name,
               t->priority,
               state_color,
               state_to_string(t->state),
               CLR_WHITE,
               t->exec_count,
               last_run_str,
               CLR_BLUE, CLR_RESET);
        
        pthread_mutex_unlock(&t->lock);
    }
    
    printf("%s└─────────────────────────────────────────────────────────────────┘%s\n", CLR_BLUE, CLR_RESET);
}

/* ============================================================================
 *                              INITIALIZATION
 * ============================================================================ */

static void scheduler_init(void) {
    print_banner();
    
    printf("%s[INIT]%s Setting up scheduler...%s\n", CLR_GREEN, CLR_WHITE, CLR_RESET);
    
    /* Setup signal handlers */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* Create directories */
    system("mkdir -p logs config data 2>/dev/null");
    
    /* Make scripts executable */
    system("chmod +x scripts/*.sh 2>/dev/null");
    
    /* Open log file */
    g_log_file = fopen(LOG_FILE, "w");
    if (g_log_file) {
        fprintf(g_log_file, "=== CS350 Scheduler Log ===\n");
        fprintf(g_log_file, "Started: %s\n\n", __DATE__ " " __TIME__);
    }
    
    /* Initialize scheduler lock */
    pthread_mutex_init(&g_scheduler_lock, NULL);
    
    /* Clear queues */
    clear_queues();
    for (int i = 0; i < MAX_TASKS; i++) {
        g_tasks[i] = NULL;
    }
    
    printf("%s[INIT]%s Creating tasks...%s\n", CLR_GREEN, CLR_WHITE, CLR_RESET);
    
    /* Create tasks with default configuration */
    g_tasks[0] = create_task(1, TASK_NAMES[0], 0, 5,  task_system_monitor);
    g_tasks[1] = create_task(2, TASK_NAMES[1], 1, 10, task_network_fetch);
    g_tasks[2] = create_task(3, TASK_NAMES[2], 2, 15, task_analyzer);
    g_tasks[3] = create_task(4, TASK_NAMES[3], 3, 10, task_reporter);
    
    /* Register tasks in priority queues */
    for (int i = 0; i < MAX_TASKS; i++) {
        if (g_tasks[i]) {
            register_task(g_tasks[i]);
            printf("  %s+%s Task %d: %-20s [P%d, %ds interval]%s\n",
                   CLR_GREEN, CLR_WHITE,
                   g_tasks[i]->id, g_tasks[i]->name,
                   g_tasks[i]->priority, g_tasks[i]->interval,
                   CLR_RESET);
        }
    }
    
    printf("%s[INIT]%s Scheduler ready.%s\n\n", CLR_GREEN, CLR_WHITE, CLR_RESET);
}

static void scheduler_cleanup(void) {
    printf("\n%s[CLEANUP]%s Shutting down...%s\n", CLR_YELLOW, CLR_WHITE, CLR_RESET);
    
    for (int i = 0; i < MAX_TASKS; i++) {
        if (g_tasks[i]) {
            destroy_task(g_tasks[i]);
            g_tasks[i] = NULL;
        }
    }
    
    pthread_mutex_destroy(&g_scheduler_lock);
    
    if (g_log_file) {
        fprintf(g_log_file, "\n=== Scheduler Stopped ===\n");
        fprintf(g_log_file, "Total ticks: %d\n", g_tick_count);
        fclose(g_log_file);
        g_log_file = NULL;
    }
    
    printf("%s[CLEANUP]%s Shutdown complete. Total ticks: %d%s\n", 
           CLR_GREEN, CLR_WHITE, g_tick_count, CLR_RESET);
}

/* ============================================================================
 *                                 MAIN
 * ============================================================================ */

int main(void) {
    pthread_t threads[MAX_TASKS];
    
    scheduler_init();
    
    /* Start task threads */
    for (int i = 0; i < MAX_TASKS; i++) {
        if (g_tasks[i]) {
            pthread_create(&threads[i], NULL, task_thread, g_tasks[i]);
        }
    }
    
    printf("%s[RUN]%s Scheduler running. Press Ctrl+C to stop.%s\n", 
           CLR_GREEN, CLR_WHITE, CLR_RESET);
    
    /* Main scheduler loop */
    while (g_running) {
        g_tick_count++;
        log_msg("\n=== TICK %d ===\n", g_tick_count);
        
        /* Print status every 5 ticks */
        if (g_tick_count % 5 == 1) {
            print_status();
        }
        
        /* Apply configuration changes */
        apply_config_overrides();
        
        /* Rebuild queues if priorities changed */
        rebuild_queues();
        
        /* Check for starvation */
        if (g_tick_count % 10 == 0) {
            check_starvation();
        }
        
        /* Check task readiness */
        time_t now = time(NULL);
        for (int i = 0; i < MAX_TASKS; i++) {
            Task* t = g_tasks[i];
            if (!t) continue;
            
            if (t->state == TASK_IDLE || t->state == TASK_PREEMPTED) {
                if (difftime(now, t->last_run) >= t->interval) {
                    pthread_mutex_lock(&t->lock);
                    t->state = TASK_READY;
                    pthread_mutex_unlock(&t->lock);
                    log_msg("[READY] Task %d ready (interval elapsed)\n", t->id);
                }
            }
        }
        
        /* Dispatch next task */
        scheduler_tick();
        
        sleep(TIME_QUANTUM);
    }
    
    /* Cleanup */
    for (int i = 0; i < MAX_TASKS; i++) {
        if (g_tasks[i]) {
            pthread_mutex_lock(&g_tasks[i]->lock);
            pthread_cond_signal(&g_tasks[i]->cond);
            pthread_mutex_unlock(&g_tasks[i]->lock);
            pthread_cancel(threads[i]);
            pthread_join(threads[i], NULL);
        }
    }
    
    scheduler_cleanup();
    return 0;
}

