/*
 * ============================================================================
 * File:        gtk_gui.c (Main GUI + Scheduler)
 * Project:     CS350 Real-Time System Monitor
 * Description: GTK3 GUI with integrated preemptive priority scheduler
 * Author:      CS350 Student Project
 * Date:        2024
 * ============================================================================
 * 
 * This is the MAIN application that runs both the GUI and scheduler.
 * Uses the same scheduling logic as scheduler.c:
 *   - Preemptive priority scheduling (lower number = higher priority)
 *   - Tasks run when their interval elapses
 *   - Highest priority ready task runs first
 *   - Starvation detection with suggested fixes
 * 
 * Features:
 *   - Real-time system metrics (CPU, RAM, Disk, Network)
 *   - Integrated task scheduler (executes same scripts as scheduler.c)
 *   - Starvation detection with suggested priority/interval values
 *   - Modern dark theme with color-coded visual feedback
 *   - Live task execution tracking
 * 
 * Build:
 *   gcc -o gtk_gui gtk_gui.c $(pkg-config --cflags --libs gtk+-3.0)
 * 
 * ============================================================================
 */

#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdarg.h>

/* ============================================================================
 *                              CONSTANTS
 * ============================================================================ */

#define REFRESH_INTERVAL_MS     1000    /* GUI refresh interval (milliseconds)  */
#define STARVATION_THRESHOLD    30      /* Seconds before starvation warning    */
#define NUM_TASKS               6       /* Number of scheduler tasks            */

/* Configuration file paths */
#define PRIORITY_CONFIG_FILE    "config/priority_override.conf"
#define INTERVAL_CONFIG_FILE    "config/interval_override.conf"

/* Log file paths */
#define LOG_DIR "logs"
#define SCHEDULER_LOG_FILE      "logs/scheduler.log"
#define GUI_LOG_FILE            "logs/gui.log"

/* Alert thresholds */
#define CPU_WARNING_THRESHOLD   70.0
#define CPU_CRITICAL_THRESHOLD  90.0
#define RAM_WARNING_THRESHOLD   75.0
#define RAM_CRITICAL_THRESHOLD  90.0
#define DISK_WARNING_THRESHOLD  80.0
#define DISK_CRITICAL_THRESHOLD 95.0

#define MAX_HISTORY 60

/* ============================================================================
 *                              DATA TYPES
 * ============================================================================ */

typedef struct {
    int         id;                     /* Task ID (1-4)                    */
    char        name[32];               /* Task display name                */
    int         priority;               /* Current priority (0-3)           */
    int         interval;               /* Execution interval (seconds)     */
    int         last_run;               /* Seconds since last execution     */
    GtkWidget*  spin_priority;          /* Priority spin button widget      */
    GtkWidget*  spin_interval;          /* Interval spin button widget      */
    GtkWidget*  status_label;           /* Starvation status indicator      */
} TaskRow;

typedef struct {
    /* Window and main containers */
    GtkWidget*      window;
    
    /* Metric display widgets */
    GtkWidget *cpu_label;
    GtkWidget *ram_label;
    GtkWidget *disk_label;
    GtkWidget *disk_status;
    GtkWidget *network_label;
    GtkWidget *cpu_drawing_area;
    GtkWidget *ram_drawing_area;
    GtkWidget *disk_drawing_area;
    
    /* Status and info widgets */
    GtkWidget*      status_label;
    GtkWidget*      alert_label;
    GtkWidget*      tasks_textview;
    GtkTextBuffer*  tasks_buffer;
    
    /* Task control */
    TaskRow         tasks[NUM_TASKS];
    
    /* Timer and counters */
    guint           timer_id;
    int             update_count;
    int             alert_count;
} AppWidgets;
typedef struct {
    float values[MAX_HISTORY];
    int count;
} HistoryData;
/* ============================================================================
 *                           TASK DEFINITIONS
 * ============================================================================ */

static const char* TASK_NAMES[NUM_TASKS] = {
    "Collect CPU",
    "Collect RAM",
    "Collect DISK",
    "Collect Network",
    "Analyzer",
    "Report Generator"
};

/* P3 = highest priority, P0 = lowest priority */
static const int DEFAULT_INTERVALS[NUM_TASKS] = {5, 10, 15, 25};
static const int DEFAULT_PRIORITIES[NUM_TASKS] = {3, 2, 1, 2};

/* ============================================================================
 *                         SYSTEM METRICS FUNCTIONS
 * ============================================================================ */

/* Cached metrics from log files */
float cpu_usage;
float ram_usage;
float disk_usage;
char ram_details[256];
char disk_details[256];
char network_details[512];

//total system specs
char total_ram[32];
char total_disk[32];
int cpu_cores;

// History Data
HistoryData cpu_history;
HistoryData ram_history;
HistoryData disk_history;

void detect_system_info() {
    FILE *fp;
    char buffer[256];

    // Detect total RAM
    fp = popen("free -h | grep Mem | awk '{print $2}'", "r");
    if (fp) {
        if (fgets(buffer, sizeof(buffer), fp)) {
            buffer[strcspn(buffer, "\n")] = 0;
            strncpy(total_ram, buffer, sizeof(total_ram) - 1);
        }
        pclose(fp);
    }

    // Detect total disk
    fp = popen("df -h / | tail -1 | awk '{print $2}'", "r");
    if (fp) {
        if (fgets(buffer, sizeof(buffer), fp)) {
            buffer[strcspn(buffer, "\n")] = 0;
            strncpy(total_disk, buffer, sizeof(total_disk) - 1);
        }
        pclose(fp);
    }

    // Detect CPU cores
    fp = popen("nproc", "r");
    if (fp) {
        if (fgets(buffer, sizeof(buffer), fp)) {
            cpu_cores = atoi(buffer);
        }
        pclose(fp);
    }
}

void add_to_history(HistoryData *history, float value) {
    if (history->count < MAX_HISTORY) {
        history->values[history->count++] = value;
    } else {
        // Shift array left
        for (int i = 0; i < MAX_HISTORY - 1; i++) {
            history->values[i] = history->values[i + 1];
        }
        history->values[MAX_HISTORY - 1] = value;
    }
}

gboolean draw_graph(GtkWidget *widget, cairo_t *cr, gpointer data) {
    HistoryData *history = (HistoryData *)data;

    int width = gtk_widget_get_allocated_width(widget);
    int height = gtk_widget_get_allocated_height(widget);

    // Background
    cairo_set_source_rgb(cr, 0.1, 0.1, 0.15);
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_fill(cr);

    // Grid lines
    cairo_set_source_rgba(cr, 0.3, 0.3, 0.35, 0.5);
    cairo_set_line_width(cr, 1.0);

    // Horizontal grid lines
    for (int i = 0; i <= 4; i++) {
        double y = (height / 4.0) * i;
        cairo_move_to(cr, 0, y);
        cairo_line_to(cr, width, y);
    }
    cairo_stroke(cr);

    // Draw graph line
    if (history->count > 1) {
        cairo_set_source_rgb(cr, 0.2, 0.8, 0.2);
        cairo_set_line_width(cr, 2.0);

        double x_step = (double)width / (MAX_HISTORY - 1);

        cairo_move_to(cr, 0, height - (history->values[0] / 100.0) * height);

        for (int i = 1; i < history->count; i++) {
            double x = i * x_step;
            double y = height - (history->values[i] / 100.0) * height;
            cairo_line_to(cr, x, y);
        }

        cairo_stroke_preserve(cr);

        // Fill under the line
        cairo_line_to(cr, (history->count - 1) * x_step, height);
        cairo_line_to(cr, 0, height);
        cairo_close_path(cr);
        cairo_set_source_rgba(cr, 0.2, 0.8, 0.2, 0.2);
        cairo_fill(cr);
    }

    return FALSE;
}

/* Scheduler log file handle */
static FILE* g_scheduler_log = NULL;
static int g_scheduler_tick_count = 0;

/*
 * scheduler_log()
 * Writes timestamped messages to the scheduler log file.
 */
static void scheduler_log(const char* format, ...) {
    if (!g_scheduler_log) {
        g_mkdir_with_parents("logs", 0755);
        g_scheduler_log = fopen(SCHEDULER_LOG_FILE, "a");
        if (g_scheduler_log) {
            time_t now = time(NULL);
            fprintf(g_scheduler_log, "\n=== CS350 GUI Scheduler Session Started ===\n");
            fprintf(g_scheduler_log, "Started: %s\n", ctime(&now));
        }
    }
    
    if (g_scheduler_log) {
        time_t now = time(NULL);
        struct tm* t = localtime(&now);
        char ts[32];
        strftime(ts, sizeof(ts), "%H:%M:%S", t);
        
        fprintf(g_scheduler_log, "[%s] ", ts);
        
        va_list args;
        va_start(args, format);
        vfprintf(g_scheduler_log, format, args);
        va_end(args);
        
        fflush(g_scheduler_log);
    }
}

void read_last_line(const char *filename, char *buffer, int bufsize) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", LOG_DIR, filename);

    FILE *fp = fopen(path, "r");
    if (!fp) {
        buffer[0] = '\0';
        return;
    }

    char line[512];
    char last_line[512] = "";

    while (fgets(line, sizeof(line), fp)) {
        if (strlen(line) > 10) {
            strncpy(last_line, line, sizeof(last_line) - 1);
            last_line[sizeof(last_line) - 1] = '\0';
        }
    }

    fclose(fp);
    strncpy(buffer, last_line, bufsize - 1);
    buffer[bufsize - 1] = '\0';
}
char* extract_value(const char *line, const char *prefix, const char *suffix) {
    static char result[256];
    const char *start = strstr(line, prefix);
    if (!start) {
        strcpy(result, "N/A");
        return result;
    }

    start += strlen(prefix);
    const char *end = strstr(start, suffix);
    if (!end) end = start + strlen(start);

    int len = end - start;
    if (len > 255) len = 255;
    strncpy(result, start, len);
    result[len] = '\0';

    // Trim whitespace
    while (len > 0 && (result[len-1] == ' ' || result[len-1] == '\n')) {
        result[--len] = '\0';
    }

    return result;
}

char line[512];
char temp1[256], temp2[256], temp3[256], temp4[256], temp5[256];

/*
 * Reads system metrics from logs/directory (written by monitor_scheduler.sh)
 */
static void read_cpu_from_log(void) {
    read_last_line("cpu_log.txt", line, sizeof(line));
    if (line[0] == '\0') return;

    cpu_usage = atof(extract_value(line, "CPU Usage: ", "%"));
    add_to_history(&cpu_history, cpu_usage);
}

static void read_ram_from_log(void){
    read_last_line("ram_log.txt", line, sizeof(line));
    ram_usage = atof(extract_value(line, "RAM Usage: ", "%"));
    strncpy(temp1, extract_value(line, "Used: ", "i /"), sizeof(temp1) - 1);
    strncpy(temp2, extract_value(line, "Total: ", "i)"), sizeof(temp2) - 1);
    snprintf(ram_details, sizeof(ram_details), "Used: %sB / %sB", temp1, temp2);
    add_to_history(&ram_history, ram_usage);
}
static void read_disk_from_log(void){
    read_last_line("disk_log.txt", line, sizeof(line));
    disk_usage = atof(extract_value(line, "Disk Usage: ", "%"));
    strncpy(temp1, extract_value(line, "Used: ", " /"), sizeof(temp1) - 1);
    strncpy(temp2, extract_value(line, "Total: ", " /"), sizeof(temp2) - 1);
    strncpy(temp3, extract_value(line, "Available: ", ")"), sizeof(temp3) - 1);
    snprintf(disk_details, sizeof(disk_details),
             "Used: %sB / %sB | Available: %sB", temp1, temp2, temp3);
    add_to_history(&disk_history, disk_usage);
}

/*
 * read_network_from_log()
 * Reads packet count from logs/network_metrics.log (written by collect_network.sh)
 */
static float g_prev_total_MB_transmitted = 0;
static float g_prev_total_MB_received = 0;
static time_t g_prev_network_time = 0;

static void read_network_from_log(void) {
    float upstream = 0.0f;
    float downstream = 0.0f;
    read_last_line("net_log.txt", line, sizeof(line));
    strncpy(temp1, extract_value(line, "Interface: ", " |"), sizeof(temp1) - 1);
    strncpy(temp2, extract_value(line, "RX Packets: ", " |"), sizeof(temp2) - 1);
    strncpy(temp3, extract_value(line, "TX Packets: ", " |"), sizeof(temp3) - 1);
    strncpy(temp4, extract_value(line, "RX: ", "MB"), sizeof(temp4) - 1);
    strncpy(temp5, extract_value(line, "TX: ", "MB"), sizeof(temp5) - 1);
    time_t now = time(NULL);
    if (g_prev_network_time > 0){
        if (g_prev_total_MB_transmitted > 0){
            float time_delta = now - g_prev_network_time;
            if (time_delta > 0) {
                float packet_delta = atof(temp5) - g_prev_total_MB_transmitted;
                if (packet_delta >= 0) {
                    upstream = packet_delta / time_delta;
                }
            }
        }
        if (g_prev_total_MB_received > 0){
            float time_delta = now - g_prev_network_time;
            if (time_delta > 0) {
                float packet_delta = atof(temp4) - g_prev_total_MB_received;
                if (packet_delta >= 0) {
                    downstream = packet_delta / time_delta;
                }
            }
        }
    }
    snprintf(network_details, sizeof(network_details),
         "Interface: <b>%s</b>\n"
         "Received: <b>%s MB</b> (Packets: <b>%s</b>)\n"
         "Transmitted: <b>%s MB</b> (Packets: <b>%s</b>)\n"
         "UPstream: <b>%.2f MB/s</b>\nDOWNstream: <b>%.2f MB/s</b>",
         temp1, temp4, temp2, temp5, temp3, upstream, downstream);

    /* Calculate packets per second since last reading */

    g_prev_total_MB_transmitted = atof(temp5);
    g_prev_total_MB_received = atof(temp4);
    g_prev_network_time = now;
}

/* ============================================================================
 *                           TASK EXECUTION
 * ============================================================================ */

/* Task tracking - initialized to -1 to indicate "never run" */
static time_t g_task_last_run[NUM_TASKS] = {-1, -1, -1, -1};
static time_t g_task_last_completed[NUM_TASKS] = {-1, -1, -1, -1};
static int g_task_exec_count[NUM_TASKS] = {0};
static int g_scheduler_initialized = 0;

/*
 * init_scheduler()
 * Initialize task execution times to current time
 */
static void init_scheduler(void) {
    if (g_scheduler_initialized) return;

    time_t now = time(NULL);
    for (int i = 0; i < NUM_TASKS; i++) {
        /* Stagger initial ready times so tasks don't all fire at once */
        g_task_last_run[i] = now;
        g_task_last_completed[i] = now;
    }
    g_scheduler_initialized = 1;
}

/*
 * execute_task_collect_cpu()
 * Collects CPU usage metrics
 * Script writes to: logs/cpu_metrics.log
 */
static void execute_task_collect_cpu(void) {
    g_task_last_run[0] = time(NULL);
    scheduler_log("[TASK] CPU Monitor starting\n");

    int ret = system("bash scripts/collect_cpu.sh > /dev/null 2>&1");

    g_task_last_completed[0] = time(NULL);
    g_task_exec_count[0]++;

    if (ret == 0) {
        scheduler_log("[TASK] CPU Monitor completed (exec #%d)\n",
                      g_task_exec_count[0]);
    } else {
        scheduler_log("[ERROR] CPU Monitor failed with code %d\n", ret);
    }
}


/*
 * execute_task_collect_ram()
 * Collects RAM usage metrics
 * Script writes to: logs/ram_metrics.log
 */
static void execute_task_collect_ram(void) {
    g_task_last_run[1] = time(NULL);
    scheduler_log("[TASK] RAM Monitor starting\n");

    int ret = system("bash scripts/collect_ram.sh > /dev/null 2>&1");

    g_task_last_completed[1] = time(NULL);
    g_task_exec_count[1]++;

    if (ret == 0) {
        scheduler_log("[TASK] RAM Monitor completed (exec #%d)\n",
                      g_task_exec_count[1]);
    } else {
        scheduler_log("[ERROR] RAM Monitor failed with code %d\n", ret);
    }
}


/*
 * execute_task_collect_disk()
 * Collects Disk usage metrics
 * Script writes to: logs/disk_metrics.log
 */
static void execute_task_collect_disk(void) {
    g_task_last_run[2] = time(NULL);
    scheduler_log("[TASK] Disk Monitor starting\n");

    int ret = system("bash scripts/collect_disk.sh > /dev/null 2>&1");

    g_task_last_completed[2] = time(NULL);
    g_task_exec_count[2]++;

    if (ret == 0) {
        scheduler_log("[TASK] Disk Monitor completed (exec #%d)\n",
                      g_task_exec_count[2]);
    } else {
        scheduler_log("[ERROR] Disk Monitor failed with code %d\n", ret);
    }
}


/*
 * execute_task_collect_network()
 * Collects network metrics
 * Script writes to: logs/network_metrics.log, logs/netdump.log
 */
static void execute_task_collect_network(void) {
    g_task_last_run[3] = time(NULL);
    scheduler_log("[TASK] Network Fetch starting\n");

    int ret = system("bash scripts/collect_network.sh > /dev/null 2>&1");

    g_task_last_completed[3] = time(NULL);
    g_task_exec_count[3]++;

    if (ret == 0) {
        scheduler_log("[TASK] Network Fetch completed (exec #%d)\n",
                      g_task_exec_count[3]);
    } else {
        scheduler_log("[ERROR] Network Fetch failed with code %d\n", ret);
    }
}


/*
 * execute_task_analyzer()
 * Runs analysis on collected metrics
 * Program writes to: logs/analysis_report.log, logs/alerts.log
 */
static void execute_task_analyzer(void) {
    g_task_last_run[4] = time(NULL);
    scheduler_log("[TASK] Analyzer starting\n");
    char cmd[256];
    snprintf(cmd, sizeof(cmd),
             "./build/analyze %.1f %.1f %.1f > /dev/null 2>&1",
             cpu_usage, ram_usage, disk_usage);

    int ret = system(cmd);


    g_task_last_completed[4] = time(NULL);
    g_task_exec_count[4]++;

    if (ret == 0) {
        scheduler_log("[TASK] Analyzer completed (exec #%d)\n",
                      g_task_exec_count[4]);
    } else {
        scheduler_log(
            "[ERROR] Analyzer failed with code %d (may need 'make all' first)\n",
            ret
        );
    }
}


/*
 * execute_task_reporter()
 * Generates HTML system report
 * Program writes to: logs/system_report.html
 */
static void execute_task_reporter(void) {
    g_task_last_run[5] = time(NULL);
    scheduler_log("[TASK] Report Generator starting\n");

    if (access("./build/report_generator", F_OK) == 0) {
        int ret = system("./build/report_generator > /dev/null 2>&1");

        g_task_last_completed[5] = time(NULL);
        g_task_exec_count[5]++;

        if (ret == 0) {
            scheduler_log("[TASK] Report Generator completed (exec #%d)\n",
                          g_task_exec_count[5]);
        } else {
            scheduler_log("[ERROR] Report Generator failed with code %d\n", ret);
        }
    } else {
        scheduler_log("[ERROR] Report Generator not found - run 'make all' first\n");
    }
}

/* Function pointers array (same pattern as scheduler) */
typedef void (*TaskFunc)(void);
static TaskFunc g_task_functions[NUM_TASKS] = {
    execute_task_collect_cpu,
    execute_task_collect_ram,
    execute_task_collect_disk,
    execute_task_collect_network,
    execute_task_analyzer,
    execute_task_reporter
};

/*
 * scheduler_tick()
 * Executes ALL ready tasks in priority order (preemptive priority scheduling)
 * All ready tasks execute each tick, highest priority first
 */
static void scheduler_tick(TaskRow tasks[], int count) {
    /* Initialize scheduler on first call */
    init_scheduler();
    
    time_t now = time(NULL);
    g_scheduler_tick_count++;
    
    /* Log tick */
    scheduler_log("\n=== TICK %d ===\n", g_scheduler_tick_count);
    
    /* Build list of ready tasks sorted by priority */
    int ready_tasks[NUM_TASKS];
    int ready_count = 0;
    
    for (int i = 0; i < count; i++) {
        /* Check if task interval has elapsed */
        double elapsed = difftime(now, g_task_last_run[i]);
        if (elapsed >= tasks[i].interval) {
            ready_tasks[ready_count++] = i;
            scheduler_log("[READY] Task %d (%s) ready - interval %ds elapsed\n", 
                         i + 1, tasks[i].name, tasks[i].interval);
        }
    }
    
    if (ready_count == 0) {
        return;  /* No tasks ready */
    }
    
    /* Sort ready tasks by priority (bubble sort - small array) */
    /* Higher priority NUMBER = higher priority (P3 > P2 > P1 > P0) */
    for (int i = 0; i < ready_count - 1; i++) {
        for (int j = 0; j < ready_count - i - 1; j++) {
            if (tasks[ready_tasks[j]].priority < tasks[ready_tasks[j+1]].priority) {
                int tmp = ready_tasks[j];
                ready_tasks[j] = ready_tasks[j+1];
                ready_tasks[j+1] = tmp;
            }
        }
    }
    
    scheduler_log("[DISPATCH] Executing %d ready task(s) in priority order\n", ready_count);
    
    /* Execute ALL ready tasks in priority order */
    for (int i = 0; i < ready_count; i++) {
        int task_idx = ready_tasks[i];
        if (task_idx >= 0 && task_idx < NUM_TASKS) {
            scheduler_log("[DISPATCH] Task %d (%s) P%d gets CPU\n", 
                         task_idx + 1, tasks[task_idx].name, tasks[task_idx].priority);
            g_task_functions[task_idx]();
        }
    }
}

/* ============================================================================
 *                       CONFIGURATION MANAGEMENT
 * ============================================================================ */

/*
 * load_task_config()
 * Loads task priorities and intervals from config files.
 */
static void load_task_config(TaskRow tasks[], int count) {
    /* Initialize defaults - MUST match scheduler.c defaults */
    for (int i = 0; i < count; i++) {
        tasks[i].id = i + 1;
        strncpy(tasks[i].name, TASK_NAMES[i], sizeof(tasks[i].name) - 1);
        tasks[i].name[sizeof(tasks[i].name) - 1] = '\0';
        tasks[i].priority = DEFAULT_PRIORITIES[i];  /* Use same defaults as scheduler */
        tasks[i].interval = DEFAULT_INTERVALS[i];
        tasks[i].last_run = 0;
        tasks[i].spin_priority = NULL;
        tasks[i].spin_interval = NULL;
        tasks[i].status_label = NULL;
    }
    
    /* Load priority overrides */
    FILE* fp = fopen(PRIORITY_CONFIG_FILE, "r");
    if (fp) {
        char line[128];
        while (fgets(line, sizeof(line), fp)) {
            int id, priority;
            if (sscanf(line, "%d %d", &id, &priority) == 2) {
                if (id >= 1 && id <= count && priority >= 0 && priority < count) {
                    tasks[id - 1].priority = priority;
                }
            }
        }
        fclose(fp);
    }
    
    /* Load interval overrides */
    fp = fopen(INTERVAL_CONFIG_FILE, "r");
    if (fp) {
        char line[128];
        while (fgets(line, sizeof(line), fp)) {
            int id, interval;
            if (sscanf(line, "%d %d", &id, &interval) == 2) {
                if (id >= 1 && id <= count && interval >= 1 && interval <= 300) {
                    tasks[id - 1].interval = interval;
                }
            }
        }
        fclose(fp);
    }
}

/*
 * save_priorities()
 * Saves current task priorities to config file.
 */
static gboolean save_priorities(AppWidgets* widgets) {
    g_mkdir_with_parents("config", 0755);
    
    FILE* fp = fopen(PRIORITY_CONFIG_FILE, "w");
    if (!fp) return FALSE;
    
    for (int i = 0; i < NUM_TASKS; i++) {
        int priority = gtk_spin_button_get_value_as_int(
            GTK_SPIN_BUTTON(widgets->tasks[i].spin_priority));
        widgets->tasks[i].priority = priority;
        fprintf(fp, "%d %d\n", widgets->tasks[i].id, priority);
    }
    fclose(fp);
    
    return TRUE;
}

/*
 * save_intervals()
 * Saves current task intervals to config file.
 */
static gboolean save_intervals(AppWidgets* widgets) {
    g_mkdir_with_parents("config", 0755);
    
    FILE* fp = fopen(INTERVAL_CONFIG_FILE, "w");
    if (!fp) return FALSE;
    
    for (int i = 0; i < NUM_TASKS; i++) {
        int interval = gtk_spin_button_get_value_as_int(
            GTK_SPIN_BUTTON(widgets->tasks[i].spin_interval));
        widgets->tasks[i].interval = interval;
        fprintf(fp, "%d %d\n", widgets->tasks[i].id, interval);
    }
    fclose(fp);
    
    return TRUE;
}

/* ============================================================================
 *                        STARVATION DETECTION
 * ============================================================================ */

/*
 * check_starvation_risk()
 * Analyzes if a configuration change may cause task starvation.
 * Returns a warning message or NULL if safe.
 */
static const char* check_starvation_risk(AppWidgets* widgets, int changed_task, int new_priority) {
    static char warning[256];
    int priority_counts[NUM_TASKS] = {0};
    
    /* Count tasks at each priority level */
    for (int i = 0; i < NUM_TASKS; i++) {
        int priority;
        if (i == changed_task) {
            priority = new_priority;
        } else {
            priority = gtk_spin_button_get_value_as_int(
                GTK_SPIN_BUTTON(widgets->tasks[i].spin_priority));
        }
        priority_counts[priority]++;
    }
    
    /* Check for potential starvation */
    int high_priority_count = priority_counts[0] + priority_counts[1];
    if (high_priority_count >= 3 && new_priority >= 2) {
        snprintf(warning, sizeof(warning),
                 "⚠ Warning: Task '%s' may starve with priority %d!",
                 widgets->tasks[changed_task].name, new_priority);
        return warning;
    }
    
    return NULL;
}

/*
 * update_starvation_indicators()
 * Updates visual indicators for task starvation status based on actual execution.
 * Suggests recommended priority/interval when starving.
 * Note: Higher priority NUMBER = HIGHER priority (P3 is highest, P0 is lowest)
 */
static void update_starvation_indicators(AppWidgets* widgets) {
    time_t now = time(NULL);
    static int starvation_logged[NUM_TASKS] = {0};  /* Track if we've logged starvation */
    
    for (int i = 0; i < NUM_TASKS; i++) {
        if (widgets->tasks[i].status_label == NULL) continue;
        
        /* Calculate seconds since last completion */
        int since_last_run;
        if (g_task_last_completed[i] <= 0 || g_task_exec_count[i] == 0) {
            /* Never run yet - check how long since scheduler started */
            since_last_run = g_scheduler_initialized ? 
                (int)difftime(now, g_task_last_run[i]) : 0;
        } else {
            since_last_run = (int)difftime(now, g_task_last_completed[i]);
        }
        
        widgets->tasks[i].last_run = since_last_run;
        
        /* Update status label based on actual last_run time with suggestions */
        if (widgets->tasks[i].last_run >= STARVATION_THRESHOLD) {
            /* Log starvation warning (only once per starvation event) */
            if (!starvation_logged[i]) {
                scheduler_log("[STARVATION WARNING] Task %d (%s) idle for %d seconds!\n",
                             i + 1, widgets->tasks[i].name, since_last_run);
                starvation_logged[i] = 1;
            }
            
            /* Task is STARVING - suggest HIGHER priority (higher number) */
            int suggested_priority = (widgets->tasks[i].priority < 3) ? widgets->tasks[i].priority + 1 : 3;
            int suggested_interval = widgets->tasks[i].interval / 2;
            if (suggested_interval < 5) suggested_interval = 5;
            
            gchar* starving_msg = g_strdup_printf(
                "<span foreground='#ef4444'>⚠ STARVING</span>\n"
                "<span foreground='#94a3b8' size='small'>↑ Priority to P%d or interval %ds</span>",
                suggested_priority, suggested_interval);
            gtk_label_set_markup(GTK_LABEL(widgets->tasks[i].status_label), starving_msg);
            g_free(starving_msg);
        } else if (widgets->tasks[i].last_run >= STARVATION_THRESHOLD / 2) {
            /* Task is AT RISK - suggest higher priority */
            int suggested_priority = (widgets->tasks[i].priority < 3) ? widgets->tasks[i].priority + 1 : 3;
            gchar* risk_msg = g_strdup_printf(
                "<span foreground='#f59e0b'>○ At Risk</span>\n"
                "<span foreground='#94a3b8' size='small'>↑ Priority to P%d</span>",
                suggested_priority);
            gtk_label_set_markup(GTK_LABEL(widgets->tasks[i].status_label), risk_msg);
            g_free(risk_msg);
            starvation_logged[i] = 0;  /* Reset so we log if it becomes starving */
        } else {
            gtk_label_set_markup(GTK_LABEL(widgets->tasks[i].status_label),
                "<span foreground='#22c55e'>● OK</span>");
            starvation_logged[i] = 0;  /* Reset starvation log flag */
        }
    }
}

/* ============================================================================
 *                           SIGNAL HANDLERS
 * ============================================================================ */

/*
 * on_priority_changed()
 * Handler for priority spin button value changes.
 */
static void on_priority_changed(GtkSpinButton* spin, gpointer user_data) {
    AppWidgets* widgets = (AppWidgets*)user_data;
    
    /* Find which task was changed */
    int changed_task = -1;
    for (int i = 0; i < NUM_TASKS; i++) {
        if (GTK_SPIN_BUTTON(widgets->tasks[i].spin_priority) == spin) {
            changed_task = i;
            break;
        }
    }
    
    if (changed_task < 0) return;
    
    int new_priority = gtk_spin_button_get_value_as_int(spin);
    
    /* Check for starvation risk */
    const char* warning = check_starvation_risk(widgets, changed_task, new_priority);
    if (warning) {
        gtk_label_set_markup(GTK_LABEL(widgets->alert_label),
            g_markup_printf_escaped("<span foreground='#f59e0b'>%s</span>", warning));
        widgets->alert_count++;
    } else {
        gtk_label_set_text(GTK_LABEL(widgets->alert_label), "");
    }
    
    /* Save configuration */
    if (save_priorities(widgets)) {
        gchar* msg = g_strdup_printf("Priority updated: %s → P%d",
                                     widgets->tasks[changed_task].name, new_priority);
        gtk_label_set_text(GTK_LABEL(widgets->status_label), msg);
        g_free(msg);
    }
}

/*
 * on_interval_changed()
 * Handler for interval spin button value changes.
 */
static void on_interval_changed(GtkSpinButton* spin, gpointer user_data) {
    AppWidgets* widgets = (AppWidgets*)user_data;
    
    /* Find which task was changed */
    int changed_task = -1;
    for (int i = 0; i < NUM_TASKS; i++) {
        if (GTK_SPIN_BUTTON(widgets->tasks[i].spin_interval) == spin) {
            changed_task = i;
            break;
        }
    }
    
    if (changed_task < 0) return;
    
    int new_interval = gtk_spin_button_get_value_as_int(spin);
    
    /* Save configuration */
    if (save_intervals(widgets)) {
        gchar* msg = g_strdup_printf("Interval updated: %s → %ds",
                                     widgets->tasks[changed_task].name, new_interval);
        gtk_label_set_text(GTK_LABEL(widgets->status_label), msg);
        g_free(msg);
    }
}

/* ============================================================================
 *                           DISPLAY UPDATE
 * ============================================================================ */

/*
 * get_status_color()
 * Returns CSS color string based on value and thresholds.
 */
static int get_status(double value, double warn, double crit) {
    if (value >= crit) return 0;  /* Red */
    if (value >= warn) return 1;  /* Yellow */
    return 2;                      /* Green */
}
static const char* get_status_color(int status) {
    if (status == 0) return "#ef4444";  /* Red */
    if (status == 1) return "#f59e0b";  /* Yellow */
    return "#22c55e";                      /* Green */
}
static const char* get_status_text(int status) {
    if (status == 0) return "⚠ Critical";
    if (status == 1) return "⚠ Warning";
    return "✓ Normal";
}

/*
 * update_display()
 * Timer callback to refresh all GUI elements with current system metrics.
 * Also runs scheduler tick to execute tasks.
 */
static gboolean update_display(gpointer data) {
    char label_text[512];
    AppWidgets* widgets = (AppWidgets*)data;
    widgets->update_count++;
    
    /* Run scheduler tick - executes tasks based on priority */
    scheduler_tick(widgets->tasks, NUM_TASKS);
    
    /* Read metrics from log files (populated by bash scripts) */
    read_cpu_from_log();
    read_ram_from_log();
    read_disk_from_log();
    read_network_from_log();
    
    /* Update CPU */
    int cpu_status = get_status(cpu_usage, CPU_WARNING_THRESHOLD, CPU_CRITICAL_THRESHOLD);
    snprintf(label_text, sizeof(label_text),
         "<span font='14' weight='bold'>CPU Usage (%d cores)</span>\n"
         "<span font='32' weight='bold' foreground='%s'>%.1f%%</span>\n"
         "<span font='10'>Status: %s</span>",
         cpu_cores,
         get_status_color(cpu_status),
         cpu_usage,
         get_status_text(cpu_status));
    gtk_label_set_markup(GTK_LABEL(widgets->cpu_label), label_text);
    
    /* Update RAM */
    int ram_status = get_status(ram_usage, RAM_WARNING_THRESHOLD, RAM_CRITICAL_THRESHOLD);
    snprintf(label_text, sizeof(label_text),
             "<span font='14' weight='bold'>RAM Usage (Total: %s)</span>\n"
             "<span font='32' weight='bold' foreground='%s'>%.1f%%</span>\n"
             "<span font='10'>%s</span>\n"
             "<span font='10'>Status: %s</span>",
             total_ram,
             get_status_color(ram_status),
             ram_usage,
             ram_details,
             get_status_text(ram_status));
    gtk_label_set_markup(GTK_LABEL(widgets->ram_label), label_text);

    // Redraw graphs
    gtk_widget_queue_draw(widgets->cpu_drawing_area);
    gtk_widget_queue_draw(widgets->ram_drawing_area);
    
    /* Update Disk */
    int disk_status = get_status(disk_usage,
                                 DISK_WARNING_THRESHOLD,
                                 DISK_CRITICAL_THRESHOLD);

    /* Update progress bar value */
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(widgets->disk_label),
                                  disk_usage / 100.0);

    /* Show used / total + percentage on the bar */
    gchar *disk_text = g_strdup_printf("%s\n\n                              (%.1f%%)", disk_details, disk_usage);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(widgets->disk_label), disk_text);
    g_free(disk_text);

    /* Update disk status LABEL (not the progress bar) */
    snprintf(label_text, sizeof(label_text),"<span font='10'>Status: %s</span>",get_status_text(disk_status));
    gtk_label_set_markup(GTK_LABEL(widgets->disk_status),label_text);

    
    /* Update Network */
    snprintf(label_text, sizeof(label_text),
             "<span font='14' weight='bold'>Network Statistics</span>\n"
             "<span font='10'>%s</span>",
             network_details);
    gtk_label_set_markup(GTK_LABEL(widgets->network_label), label_text);
    
    /* Update starvation indicators */
    update_starvation_indicators(widgets);
    
    /* Update task info display with actual execution counts */
    GString *task_info = g_string_new("");

    g_string_append(task_info,
        "╔════════════════════════════════════════════════════════════════════╗\n"
        "║  SCHEDULER TASK STATUS (Live Execution)                            ║\n"
        "╠════════════════════════════════════════════════════════════════════╣\n"
        "║  ID │ Task Name          │ Pri │ Int  │ Runs │ Last Run │ Status   ║\n"
        "╠════════════════════════════════════════════════════════════════════╣\n"
    );

    for (int i = 0; i < NUM_TASKS; i++) {
        g_string_append_printf(task_info,
            "║  %d  │ %-18s │ P%d  │ %3ds │ %4d │  %3ds    │   %s     ║\n",
            i + 1,
            widgets->tasks[i].name,
            widgets->tasks[i].priority,
            widgets->tasks[i].interval,
            g_task_exec_count[i],
            widgets->tasks[i].last_run,
            widgets->tasks[i].last_run < STARVATION_THRESHOLD / 2 ? "OK" : "⚠"
        );
    }

    g_string_append(task_info,
        "╚════════════════════════════════════════════════════════════════════╝"
    );

    gtk_text_buffer_set_text(widgets->tasks_buffer, task_info->str, -1);
    g_string_free(task_info, TRUE);

    
    /* Update status bar */
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    char time_str[32];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", t);
    
    gchar* status = g_strdup_printf(
        "● Ready │ Updates: %d │ Alerts: %d │ %s",
        widgets->update_count, widgets->alert_count, time_str);
    gtk_label_set_text(GTK_LABEL(widgets->status_label), status);
    g_free(status);
    
    return TRUE;  /* Continue timer */
}

/* ============================================================================
 *                           WINDOW CLEANUP
 * ============================================================================ */

/*
 * on_window_destroy()
 * Cleanup handler when main window is closed.
 */
static void on_window_destroy(GtkWidget* widget, gpointer data) {
    (void)widget;
    AppWidgets* widgets = (AppWidgets*)data;
    
    if (widgets->timer_id > 0) {
        g_source_remove(widgets->timer_id);
        widgets->timer_id = 0;
    }
    
    /* Close scheduler log */
    if (g_scheduler_log) {
        scheduler_log("\n=== Scheduler Session Ended ===\n");
        scheduler_log("Total ticks: %d\n", g_scheduler_tick_count);
        scheduler_log("Task execution counts: T1=%d, T2=%d, T3=%d, T4=%d\n",
                     g_task_exec_count[0], g_task_exec_count[1],
                     g_task_exec_count[2], g_task_exec_count[3]);
        fclose(g_scheduler_log);
        g_scheduler_log = NULL;
    }
    
    g_free(widgets);
    gtk_main_quit();
}

/* ============================================================================
 *                           APPLICATION SETUP
 * ============================================================================ */

/*
 * apply_dark_theme()
 * Applies custom CSS styling for dark theme.
 */
static void apply_dark_theme(void) {
    const gchar* css =
        "/* Main background */\n"
        "#main-container { background: #0f172a; }\n"
        "window { background: #0f172a; }\n"
        "\n"
        "/* Labels and text */\n"
        "label { color: #e2e8f0; }\n"
        "label.title { font-size: 18px; font-weight: bold; color: #f8fafc; }\n"
        "label.subtitle { font-size: 11px; color: #22c55e; }\n"
        "label.alert { color: #f59e0b; }\n"
        "\n"
        "/* Cards and frames */\n"
        "frame { border-radius: 8px; border: 1px solid #334155; background: #1e293b; }\n"
        "frame > label { color: #94a3b8; font-weight: bold; }\n"
        "\n"
        "/* Progress bars */\n"
        "progressbar { min-height: 20px; }\n"
        "progressbar trough { min-height: 20px; border-radius: 10px; background: #334155; }\n"
        "progressbar progress { border-radius: 10px; }\n"
        "progressbar text { color: #f8fafc; font-weight: bold; }\n"
        "#cpu-bar progress { background: linear-gradient(90deg, #3b82f6, #60a5fa); }\n"
        "#ram-bar progress { background: linear-gradient(90deg, #8b5cf6, #a78bfa); }\n"
        "#disk-bar progress { background: linear-gradient(90deg, #ec4899, #f472b6); }\n"
        "\n"
        "/* Spin buttons */\n"
        "spinbutton { background: #1e293b; color: #e2e8f0; border: 1px solid #475569; border-radius: 4px; }\n"
        "spinbutton entry { background: #1e293b; color: #e2e8f0; }\n"
        "\n"
        "/* Text view */\n"
        "textview { background: #0f172a; color: #e2e8f0; font-family: monospace; }\n"
        "textview text { background: #0f172a; color: #e2e8f0; }\n"
        "\n"
        "/* Status bar */\n"
        "#status-bar { background: #1e293b; padding: 8px; border-radius: 6px; }\n"
        "#status-bar label { color: #22c55e; font-weight: bold; }\n";
    
    GtkCssProvider* provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider, css, -1, NULL);
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_USER);
    g_object_unref(provider);
}

/*
 * create_metrics_section()
 * Creates the system metrics display section.
 */
static GtkWidget* create_metrics_section(AppWidgets* widgets) {
    GtkWidget* frame = gtk_frame_new("System Metrics");
    
    GtkWidget* grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 12);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 12);
    gtk_container_add(GTK_CONTAINER(frame), grid);
    
    /* CPU Row */
    GtkWidget *cpu_frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(cpu_frame), GTK_SHADOW_ETCHED_IN);
    GtkWidget *cpu_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(cpu_frame), cpu_box);

    widgets->cpu_label = gtk_label_new("");
    gtk_label_set_justify(GTK_LABEL(widgets->cpu_label), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start(GTK_BOX(cpu_box), widgets->cpu_label, FALSE, FALSE, 5);

    widgets->cpu_drawing_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(widgets->cpu_drawing_area, 400, 200);
    g_signal_connect(widgets->cpu_drawing_area, "draw", G_CALLBACK(draw_graph), &cpu_history);
    gtk_box_pack_start(GTK_BOX(cpu_box), widgets->cpu_drawing_area, TRUE, TRUE, 5);

    gtk_grid_attach(GTK_GRID(grid), cpu_frame, 0, 0, 1, 1);

    /* RAM Row */
    GtkWidget *ram_frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(ram_frame), GTK_SHADOW_ETCHED_IN);
    GtkWidget *ram_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(ram_frame), ram_box);

    widgets->ram_label = gtk_label_new("");
    gtk_label_set_justify(GTK_LABEL(widgets->ram_label), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start(GTK_BOX(ram_box), widgets->ram_label, FALSE, FALSE, 5);

    widgets->ram_drawing_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(widgets->ram_drawing_area, 400, 200);
    g_signal_connect(widgets->ram_drawing_area, "draw", G_CALLBACK(draw_graph), &ram_history);
    gtk_box_pack_start(GTK_BOX(ram_box), widgets->ram_drawing_area, TRUE, TRUE, 5);

    gtk_grid_attach(GTK_GRID(grid), ram_frame, 1, 0, 1, 1);
    
    /* Disk Row (Card like CPU/RAM) */
    GtkWidget *disk_frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(disk_frame), GTK_SHADOW_ETCHED_IN);

    GtkWidget *disk_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(disk_box), 8);
    gtk_container_add(GTK_CONTAINER(disk_frame), disk_box);

    /* Disk title */
    GtkWidget *disk_title = gtk_label_new("Disk Usage");
    gtk_widget_set_halign(disk_title, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(disk_box), disk_title, FALSE, FALSE, 0);

    /* Disk progress bar */
    widgets->disk_label = gtk_progress_bar_new();
    gtk_widget_set_name(widgets->disk_label, "disk-bar");
    gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(widgets->disk_label), TRUE);
    gtk_box_pack_start(GTK_BOX(disk_box), widgets->disk_label, FALSE, FALSE, 0);

    /* Disk status indicator */
    widgets->disk_status = gtk_label_new("");
    gtk_widget_set_halign(widgets->disk_status, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(disk_box), widgets->disk_status, FALSE, FALSE, 0);

    /* Attach disk card to grid */
    gtk_grid_attach(GTK_GRID(grid), disk_frame, 0, 1, 1, 1);

    
    /* Network Card */
    GtkWidget *net_frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(net_frame), GTK_SHADOW_ETCHED_IN);

    GtkWidget *net_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(net_box), 8);
    gtk_container_add(GTK_CONTAINER(net_frame), net_box);

    /* Network title */
    GtkWidget *net_title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(net_title),
        "<span weight='bold'>Network Statistics</span>");
    gtk_widget_set_halign(net_title, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(net_box), net_title, FALSE, FALSE, 0);

    /* Network content */
    widgets->network_label = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(widgets->network_label), 0.0); // left aligned text
    gtk_box_pack_start(GTK_BOX(net_box), widgets->network_label, FALSE, FALSE, 0);

    /* Attach to grid (below Disk card) */
    gtk_grid_attach(GTK_GRID(grid), net_frame, 1, 1, 1, 1);

    
    return frame;
}

/*
 * create_scheduler_section()
 * Creates the scheduler control section with task priority/interval controls.
 */
static GtkWidget* create_scheduler_section(AppWidgets* widgets) {
    GtkWidget* frame = gtk_frame_new("Scheduler Control");
    
    GtkWidget* grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 16);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 12);
    gtk_container_add(GTK_CONTAINER(frame), grid);
    
    /* Headers */
    GtkWidget* hdr_task = gtk_label_new("Task");
    gtk_widget_set_halign(hdr_task, GTK_ALIGN_START);
    GtkWidget* hdr_prio = gtk_label_new("Priority");
    GtkWidget* hdr_intv = gtk_label_new("Interval");
    GtkWidget* hdr_stat = gtk_label_new("Status");
    
    gtk_grid_attach(GTK_GRID(grid), hdr_task, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), hdr_prio, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), hdr_intv, 2, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), hdr_stat, 3, 0, 1, 1);
    
    /* Task rows */
    for (int i = 0; i < NUM_TASKS; i++) {
        GtkWidget* name = gtk_label_new(widgets->tasks[i].name);
        gtk_widget_set_halign(name, GTK_ALIGN_START);
        gtk_grid_attach(GTK_GRID(grid), name, 0, i + 1, 1, 1);
        
        /* Priority spinner (0-3) */
        GtkWidget* spin_prio = gtk_spin_button_new_with_range(0, 3, 1);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_prio), widgets->tasks[i].priority);
        g_signal_connect(spin_prio, "value-changed", G_CALLBACK(on_priority_changed), widgets);
        widgets->tasks[i].spin_priority = spin_prio;
        gtk_grid_attach(GTK_GRID(grid), spin_prio, 1, i + 1, 1, 1);
        
        /* Interval spinner (1-300 seconds) */
        GtkWidget* spin_intv = gtk_spin_button_new_with_range(1, 300, 1);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_intv), widgets->tasks[i].interval);
        g_signal_connect(spin_intv, "value-changed", G_CALLBACK(on_interval_changed), widgets);
        widgets->tasks[i].spin_interval = spin_intv;
        gtk_grid_attach(GTK_GRID(grid), spin_intv, 2, i + 1, 1, 1);
        
        /* Status indicator */
        GtkWidget* status = gtk_label_new("● OK");
        widgets->tasks[i].status_label = status;
        gtk_grid_attach(GTK_GRID(grid), status, 3, i + 1, 1, 1);
    }
    
    /* Alert label */
    widgets->alert_label = gtk_label_new("");
    gtk_widget_set_name(widgets->alert_label, "alert");
    gtk_grid_attach(GTK_GRID(grid), widgets->alert_label, 0, NUM_TASKS + 1, 4, 1);
    
    return frame;
}

/*
 * create_task_info_section()
 * Creates the task information display section.
 */
static GtkWidget* create_task_info_section(AppWidgets* widgets) {
    GtkWidget* frame = gtk_frame_new("Task Details");

    widgets->tasks_textview = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(widgets->tasks_textview), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(widgets->tasks_textview), TRUE);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(widgets->tasks_textview), 8);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(widgets->tasks_textview), 8);

    widgets->tasks_buffer =
        gtk_text_view_get_buffer(GTK_TEXT_VIEW(widgets->tasks_textview));

    /* IMPORTANT: let it expand fully */
    gtk_widget_set_vexpand(widgets->tasks_textview, TRUE);
    gtk_widget_set_hexpand(widgets->tasks_textview, TRUE);

    gtk_container_add(GTK_CONTAINER(frame), widgets->tasks_textview);

    return frame;
}

/*
 * activate()
 * Application activation callback - creates main window and widgets.
 */
static void activate(GtkApplication* app, gpointer user_data) {
    (void)user_data;
    
    /* Allocate and initialize app state */
    AppWidgets* widgets = g_malloc0(sizeof(AppWidgets));
    widgets->update_count = 0;
    widgets->alert_count = 0;
    
    /* Load task configuration */
    load_task_config(widgets->tasks, NUM_TASKS);

    /* detect system info ONCE */
    detect_system_info();

    /* Clear alerts at GUI startup (new monitoring session) */
    FILE *fp = fopen("logs/alerts.log", "w");
    if (fp) {
        fclose(fp);
    }

    /* Apply dark theme */
    apply_dark_theme();
    
    /* Create main window */
    widgets->window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(widgets->window), "CS350 Real-Time System Monitor");
    gtk_window_set_default_size(GTK_WINDOW(widgets->window), 750, 600);
    gtk_container_set_border_width(GTK_CONTAINER(widgets->window), 12);
    
    /* Main container */
    GtkWidget* main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_name(main_box, "main-container");
    gtk_container_add(GTK_CONTAINER(widgets->window), main_box);
    
    /* Header */
    GtkWidget* header_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_halign(header_box, GTK_ALIGN_CENTER);
    
    GtkWidget* title = gtk_label_new("Real-Time System Monitor");
    gtk_style_context_add_class(gtk_widget_get_style_context(title), "title");
    
    GtkWidget* subtitle = gtk_label_new("● System Monitor");
    gtk_style_context_add_class(gtk_widget_get_style_context(subtitle), "subtitle");
    
    gtk_box_pack_start(GTK_BOX(header_box), title, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(header_box), subtitle, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(main_box), header_box, FALSE, FALSE, 0);
    
    /* =======================
    * Landscape content area
    * ======================= */
    GtkWidget* content_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_box_pack_start(GTK_BOX(main_box), content_box, TRUE, TRUE, 0);

    /* LEFT: Metrics */
    GtkWidget* metrics = create_metrics_section(widgets);
    gtk_widget_set_hexpand(metrics, TRUE);
    gtk_box_pack_start(GTK_BOX(content_box), metrics, TRUE, TRUE, 0);

    /* RIGHT: Scheduler + Task info */
    GtkWidget* right_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_hexpand(right_box, TRUE);
    gtk_box_pack_start(GTK_BOX(content_box), right_box, TRUE, TRUE, 0);

    /* Scheduler */
    GtkWidget* scheduler = create_scheduler_section(widgets);
    gtk_box_pack_start(GTK_BOX(right_box), scheduler, FALSE, FALSE, 0);

    /* Task info */
    GtkWidget* task_info = create_task_info_section(widgets);
    gtk_box_pack_start(GTK_BOX(right_box), task_info, TRUE, TRUE, 0);

    
    /* Status bar */
    GtkWidget* status_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_name(status_bar, "status-bar");
    widgets->status_label = gtk_label_new("Initializing...");
    gtk_box_pack_start(GTK_BOX(status_bar), widgets->status_label, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(main_box), status_bar, FALSE, FALSE, 0);
    
    /* Connect destroy signal */
    g_signal_connect(widgets->window, "destroy", G_CALLBACK(on_window_destroy), widgets);
    
    /* Show all widgets */
    gtk_widget_show_all(widgets->window);
    
    /* Initial update and start timer */
    update_display(widgets);
    widgets->timer_id = g_timeout_add(REFRESH_INTERVAL_MS, update_display, widgets);
}

/* ============================================================================
 *                                 MAIN
 * ============================================================================ */

int main(int argc, char** argv) {
    GtkApplication* app;
    int status;
    
    app = gtk_application_new("com.cs350.sysmonitor", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    
    return status;
}
