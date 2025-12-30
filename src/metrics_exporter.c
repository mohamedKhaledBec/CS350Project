/*
 * ============================================================================
 * File:        metrics_exporter.c
 * Project:     CS350 Real-Time System Monitor
 * Description: JSON Metrics Exporter for Web UI Integration
 * Author:      CS350 Student Project
 * Date:        2024
 * ============================================================================
 * 
 * Features:
 *   - Real-time metrics from /proc filesystem
 *   - JSON output for web dashboard consumption
 *   - Alert history inclusion
 *   - Scheduler task status export
 * 
 * ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/statvfs.h>
#include <linux/limits.h>

/* ============================================================================
 *                              CONSTANTS
 * ============================================================================ */

#define MAX_LINE_LEN    1024    /* Maximum line buffer size     */
#define MAX_ALERTS      10      /* Maximum alerts in JSON       */
#define NUM_TASKS       4       /* Number of scheduler tasks    */

/* File paths */
#define ALERTS_LOG      "logs/alerts.log"
#define ANALYSIS_LOG    "logs/analysis_report.log"
#define DEFAULT_OUTPUT  "ui/metrics.json"

/* ============================================================================
 *                              ANSI COLORS
 * ============================================================================ */

#define CLR_RESET   "\033[0m"
#define CLR_GREEN   "\033[1;32m"
#define CLR_CYAN    "\033[1;36m"
#define CLR_WHITE   "\033[1;37m"

/* ============================================================================
 *                              DATA TYPES
 * ============================================================================ */

typedef struct {
    char    timestamp[64];      /* Collection timestamp     */
    double  cpu;                /* CPU usage percentage     */
    double  ram;                /* RAM usage percentage     */
    double  disk;               /* Disk usage percentage    */
    int     packets;            /* Network packet count     */
} SystemMetrics;

typedef struct {
    char    timestamp[64];      /* Alert timestamp          */
    char    severity[16];       /* WARNING, CRITICAL, INFO  */
    char    message[256];       /* Alert message text       */
} AlertEntry;

typedef struct {
    char    name[32];           /* Task name                */
    int     priority;           /* Task priority (0-3)      */
    int     interval;           /* Execution interval       */
    char    status[16];         /* ok, warning, starving    */
} TaskInfo;

/* ============================================================================
 *                            GLOBAL VARIABLES
 * ============================================================================ */

static char g_project_root[PATH_MAX] = {0};

/* ============================================================================
 *                          UTILITY FUNCTIONS
 * ============================================================================ */

/*
 * init_paths()
 * Initialize project root path from executable location.
 */
static void init_paths(void) {
    char exe_path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    
    if (len != -1) {
        exe_path[len] = '\0';
        char* dir = dirname(exe_path);
        char* parent = dirname(dir);
        strncpy(g_project_root, parent, PATH_MAX - 1);
    } else {
        if (getcwd(g_project_root, sizeof(g_project_root)) == NULL) {
            strncpy(g_project_root, ".", PATH_MAX - 1);
        }
    }
}

/*
 * escape_json_string()
 * Escapes special characters for JSON string output.
 */
static void escape_json_string(const char* input, char* output, size_t max_len) {
    size_t j = 0;
    for (size_t i = 0; input[i] && j < max_len - 2; i++) {
        switch (input[i]) {
            case '"':  output[j++] = '\\'; output[j++] = '"';  break;
            case '\\': output[j++] = '\\'; output[j++] = '\\'; break;
            case '\n': output[j++] = '\\'; output[j++] = 'n';  break;
            case '\r': output[j++] = '\\'; output[j++] = 'r';  break;
            case '\t': output[j++] = '\\'; output[j++] = 't';  break;
            default:   output[j++] = input[i]; break;
        }
    }
    output[j] = '\0';
}

/* ============================================================================
 *                         METRICS COLLECTION
 * ============================================================================ */

/*
 * get_cpu_usage()
 * Reads CPU usage from /proc/stat.
 */
static double get_cpu_usage(void) {
    static long prev_idle = 0, prev_total = 0;
    
    FILE* fp = fopen("/proc/stat", "r");
    if (!fp) return 0.0;
    
    char buffer[256];
    if (fgets(buffer, sizeof(buffer), fp) == NULL) {
        fclose(fp);
        return 0.0;
    }
    fclose(fp);
    
    long user, nice, system, idle, iowait, irq, softirq;
    sscanf(buffer, "cpu %ld %ld %ld %ld %ld %ld %ld",
           &user, &nice, &system, &idle, &iowait, &irq, &softirq);
    
    long total = user + nice + system + idle + iowait + irq + softirq;
    long total_idle = idle + iowait;
    
    if (prev_total == 0) {
        prev_idle = total_idle;
        prev_total = total;
        usleep(100000);
        return get_cpu_usage();
    }
    
    long diff_total = total - prev_total;
    long diff_idle = total_idle - prev_idle;
    prev_idle = total_idle;
    prev_total = total;
    
    if (diff_total == 0) return 0.0;
    return (100.0 * (diff_total - diff_idle)) / diff_total;
}

/*
 * get_ram_usage()
 * Reads RAM usage from /proc/meminfo.
 */
static double get_ram_usage(void) {
    FILE* fp = fopen("/proc/meminfo", "r");
    if (!fp) return 0.0;
    
    long total = 0, available = 0;
    char line[256];
    
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "MemTotal:", 9) == 0) {
            sscanf(line, "MemTotal: %ld kB", &total);
        } else if (strncmp(line, "MemAvailable:", 13) == 0) {
            sscanf(line, "MemAvailable: %ld kB", &available);
            break;
        }
    }
    fclose(fp);
    
    if (total == 0) return 0.0;
    return ((double)(total - available) / total) * 100.0;
}

/*
 * get_disk_usage()
 * Gets disk usage for root filesystem.
 */
static double get_disk_usage(void) {
    struct statvfs stat;
    if (statvfs("/", &stat) != 0) return 0.0;
    
    unsigned long total = stat.f_blocks * stat.f_frsize;
    unsigned long available = stat.f_bavail * stat.f_frsize;
    
    if (total == 0) return 0.0;
    return ((double)(total - available) / total) * 100.0;
}

/*
 * get_network_packets()
 * Reads network packet counts from /proc/net/dev.
 */
static long g_prev_packet_count = 0;
static time_t g_prev_packet_time = 0;

static int get_network_packets(void) {
    FILE* fp = fopen("/proc/net/dev", "r");
    if (!fp) return 0;
    
    char line[256];
    long total = 0;
    
    /* Skip headers */
    if (fgets(line, sizeof(line), fp) == NULL) { fclose(fp); return 0; }
    if (fgets(line, sizeof(line), fp) == NULL) { fclose(fp); return 0; }
    
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "lo:")) continue;
        
        char iface[32];
        long rx, tx;
        if (sscanf(line, "%[^:]: %*d %ld %*d %*d %*d %*d %*d %*d %*d %ld",
                   iface, &rx, &tx) >= 2) {
            total += (rx + tx);
        }
    }
    fclose(fp);
    
    /* Calculate packets per second */
    time_t now = time(NULL);
    int packets_per_sec = 0;
    
    if (g_prev_packet_time > 0) {
        long time_delta = (long)(now - g_prev_packet_time);
        if (time_delta > 0) {
            long packet_delta = total - g_prev_packet_count;
            packets_per_sec = (int)(packet_delta / time_delta);
        }
    }
    
    g_prev_packet_count = total;
    g_prev_packet_time = now;
    
    return packets_per_sec;
}

/*
 * collect_metrics()
 * Collects current system metrics.
 */
static void collect_metrics(SystemMetrics* metrics) {
    metrics->cpu = get_cpu_usage();
    metrics->ram = get_ram_usage();
    metrics->disk = get_disk_usage();
    metrics->packets = get_network_packets();
    
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    strftime(metrics->timestamp, sizeof(metrics->timestamp),
             "%Y-%m-%d %H:%M:%S", t);
}

/* ============================================================================
 *                           ALERT READING
 * ============================================================================ */

/*
 * read_alerts()
 * Reads recent alerts from log file.
 */
static int read_alerts(AlertEntry alerts[], int max_count) {
    char log_path[PATH_MAX];
    snprintf(log_path, sizeof(log_path), "%s/%s", g_project_root, ALERTS_LOG);
    
    FILE* fp = fopen(log_path, "r");
    if (!fp) return 0;
    
    AlertEntry temp[100];
    int total = 0;
    char line[MAX_LINE_LEN];
    
    while (fgets(line, sizeof(line), fp) && total < 100) {
        /* Parse [timestamp] [SEVERITY] component: message */
        char ts[64] = "", sev[16] = "", msg[256] = "";
        
        if (sscanf(line, "[%63[^]]] [%15[^]]] %255[^\n]", ts, sev, msg) >= 2) {
            strncpy(temp[total].timestamp, ts, sizeof(temp[total].timestamp) - 1);
            strncpy(temp[total].severity, sev, sizeof(temp[total].severity) - 1);
            strncpy(temp[total].message, msg, sizeof(temp[total].message) - 1);
            total++;
        }
    }
    fclose(fp);
    
    /* Return most recent alerts */
    int start = (total > max_count) ? (total - max_count) : 0;
    int count = 0;
    for (int i = start; i < total && count < max_count; i++) {
        alerts[count++] = temp[i];
    }
    
    return count;
}

/* ============================================================================
 *                           TASK INFO
 * ============================================================================ */

/*
 * get_task_info()
 * Returns default task information (would read from scheduler in real impl).
 */
static void get_task_info(TaskInfo tasks[], int count) {
    const char* names[] = {"System Monitor", "Ethernet Fetch", "Analyzer", "Report Generator"};
    const int priorities[] = {0, 1, 2, 3};
    const int intervals[] = {5, 10, 15, 10};
    
    for (int i = 0; i < count && i < NUM_TASKS; i++) {
        strncpy(tasks[i].name, names[i], sizeof(tasks[i].name) - 1);
        tasks[i].priority = priorities[i];
        tasks[i].interval = intervals[i];
        strncpy(tasks[i].status, "ok", sizeof(tasks[i].status) - 1);
    }
}

/* ============================================================================
 *                           JSON EXPORT
 * ============================================================================ */

/*
 * export_to_json()
 * Exports all metrics and data to JSON file.
 */
static void export_to_json(const char* output_file) {
    FILE* fp = fopen(output_file, "w");
    if (!fp) {
        fprintf(stderr, "Error: Cannot create %s\n", output_file);
        return;
    }
    
    /* Collect data */
    SystemMetrics metrics;
    collect_metrics(&metrics);
    
    AlertEntry alerts[MAX_ALERTS];
    int alert_count = read_alerts(alerts, MAX_ALERTS);
    
    TaskInfo tasks[NUM_TASKS];
    get_task_info(tasks, NUM_TASKS);
    
    /* Write JSON */
    fprintf(fp, "{\n");
    fprintf(fp, "  \"timestamp\": \"%s\",\n", metrics.timestamp);
    fprintf(fp, "  \"status\": \"operational\",\n");
    fprintf(fp, "\n");
    
    /* Metrics */
    fprintf(fp, "  \"metrics\": {\n");
    fprintf(fp, "    \"cpu\": %.2f,\n", metrics.cpu);
    fprintf(fp, "    \"ram\": %.2f,\n", metrics.ram);
    fprintf(fp, "    \"disk\": %.2f,\n", metrics.disk);
    fprintf(fp, "    \"network_packets\": %d\n", metrics.packets);
    fprintf(fp, "  },\n");
    fprintf(fp, "\n");
    
    /* Thresholds */
    fprintf(fp, "  \"thresholds\": {\n");
    fprintf(fp, "    \"cpu\": { \"warning\": 70, \"critical\": 90 },\n");
    fprintf(fp, "    \"ram\": { \"warning\": 75, \"critical\": 90 },\n");
    fprintf(fp, "    \"disk\": { \"warning\": 80, \"critical\": 95 }\n");
    fprintf(fp, "  },\n");
    fprintf(fp, "\n");
    
    /* Tasks */
    fprintf(fp, "  \"tasks\": [\n");
    for (int i = 0; i < NUM_TASKS; i++) {
        fprintf(fp, "    {\n");
        fprintf(fp, "      \"id\": %d,\n", i + 1);
        fprintf(fp, "      \"name\": \"%s\",\n", tasks[i].name);
        fprintf(fp, "      \"priority\": %d,\n", tasks[i].priority);
        fprintf(fp, "      \"interval\": %d,\n", tasks[i].interval);
        fprintf(fp, "      \"status\": \"%s\"\n", tasks[i].status);
        fprintf(fp, "    }%s\n", (i < NUM_TASKS - 1) ? "," : "");
    }
    fprintf(fp, "  ],\n");
    fprintf(fp, "\n");
    
    /* Alerts */
    fprintf(fp, "  \"alerts\": [\n");
    for (int i = 0; i < alert_count; i++) {
        char escaped_msg[512];
        escape_json_string(alerts[i].message, escaped_msg, sizeof(escaped_msg));
        
        fprintf(fp, "    {\n");
        fprintf(fp, "      \"timestamp\": \"%s\",\n", alerts[i].timestamp);
        fprintf(fp, "      \"severity\": \"%s\",\n", alerts[i].severity);
        fprintf(fp, "      \"message\": \"%s\"\n", escaped_msg);
        fprintf(fp, "    }%s\n", (i < alert_count - 1) ? "," : "");
    }
    fprintf(fp, "  ]\n");
    
    fprintf(fp, "}\n");
    fclose(fp);
}

/* ============================================================================
 *                                 MAIN
 * ============================================================================ */

int main(int argc, char* argv[]) {
    /* Initialize paths */
    init_paths();
    
    /* Determine output file */
    char output_path[PATH_MAX];
    if (argc > 1) {
        strncpy(output_path, argv[1], PATH_MAX - 1);
    } else {
        snprintf(output_path, sizeof(output_path), "%s/%s", g_project_root, DEFAULT_OUTPUT);
    }
    
    /* Print header */
    printf("\n");
    printf("%s╔═══════════════════════════════════════════════════════════════╗%s\n", CLR_CYAN, CLR_RESET);
    printf("%s║%s          JSON METRICS EXPORTER                               %s║%s\n", CLR_CYAN, CLR_WHITE, CLR_CYAN, CLR_RESET);
    printf("%s╚═══════════════════════════════════════════════════════════════╝%s\n", CLR_CYAN, CLR_RESET);
    printf("\n");
    
    printf("  %s→%s Collecting system metrics...\n", CLR_GREEN, CLR_RESET);
    printf("  %s→%s Reading alert history...\n", CLR_GREEN, CLR_RESET);
    printf("  %s→%s Exporting to JSON...\n", CLR_GREEN, CLR_RESET);
    
    export_to_json(output_path);
    
    printf("\n");
    printf("  %s✓ JSON exported:%s %s%s\n", CLR_GREEN, CLR_WHITE, output_path, CLR_RESET);
    printf("\n");
    
    return 0;
}
