/*
 * ============================================================================
 * File:        analyze.c
 * Project:     CS350 Real-Time System Monitor
 * Description: System Metrics Analyzer with Threshold-Based Alerting
 * Author:      CS350 Student Project
 * Date:        2024
 * ============================================================================
 * 
 * This analyzer:
 *   - Processes CPU, RAM, Disk, and Network metrics
 *   - Generates alerts when thresholds are exceeded
 *   - Creates detailed analysis reports
 *   - Supports configurable warning/critical levels
 * 
 * ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <libgen.h>
#include <linux/limits.h>

/* ============================================================================
 *                              CONFIGURATION
 * ============================================================================ */

/* Warning thresholds (percentage) */
#define CPU_WARN_THRESHOLD      70
#define RAM_WARN_THRESHOLD      75
#define DISK_WARN_THRESHOLD     80
#define NET_WARN_THRESHOLD      5

/* Critical thresholds (percentage) */
#define CPU_CRIT_THRESHOLD      90
#define RAM_CRIT_THRESHOLD      90
#define DISK_CRIT_THRESHOLD     95
#define NET_CRIT_THRESHOLD      10

/* File paths */
#define ALERTS_LOG              "logs/alerts.log"
#define ANALYSIS_LOG            "logs/analysis_report.log"

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
    LEVEL_OK,
    LEVEL_WARNING,
    LEVEL_CRITICAL
} AlertLevel;

typedef struct {
    double      cpu;            /* CPU usage percentage         */
    double      ram;            /* RAM usage percentage         */
    double      disk;           /* Disk usage percentage        */
    int         packets;        /* Network packet count         */
    char        timestamp[64];  /* Collection timestamp         */
} SystemMetrics;

typedef struct {
    int         total;          /* Total alerts generated       */
    int         warnings;       /* Warning level alerts         */
    int         criticals;      /* Critical level alerts        */
} AlertStats;

/* ============================================================================
 *                            GLOBAL VARIABLES
 * ============================================================================ */

static char         g_project_root[PATH_MAX] = {0};
static AlertStats   g_stats = {0, 0, 0};

/* ============================================================================
 *                            FUNCTION PROTOTYPES
 * ============================================================================ */

static void         init_paths(void);
static AlertLevel   check_threshold(double value, double warn, double crit);
static const char*  level_to_string(AlertLevel level);
static const char*  level_to_color(AlertLevel level);
static void         log_alert(const char* component, AlertLevel level, const char* message);
static int          count_packets(const char* filepath);
static void         analyze_metrics(SystemMetrics* metrics);
static void         print_analysis_header(void);
static void         print_metric_result(const char* name, double value, const char* unit, AlertLevel level);
static void         print_summary(void);

/* ============================================================================
 *                            PATH INITIALIZATION
 * ============================================================================ */

static void init_paths(void) {
    char exe_path[PATH_MAX];
    
    /* Get executable path from /proc */
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len != -1) {
        exe_path[len] = '\0';
        char* dir = dirname(exe_path);
        char* parent = dirname(dir);
        strncpy(g_project_root, parent, PATH_MAX - 1);
    } else {
        /* Fallback to current directory */
        if (getcwd(g_project_root, sizeof(g_project_root)) == NULL) {
            strncpy(g_project_root, ".", PATH_MAX - 1);
        }
    }
}

/* ============================================================================
 *                          THRESHOLD CHECKING
 * ============================================================================ */

static AlertLevel check_threshold(double value, double warn, double crit) {
    if (value >= crit) return LEVEL_CRITICAL;
    if (value >= warn) return LEVEL_WARNING;
    return LEVEL_OK;
}

static const char* level_to_string(AlertLevel level) {
    switch (level) {
        case LEVEL_CRITICAL: return "CRITICAL";
        case LEVEL_WARNING:  return "WARNING";
        case LEVEL_OK:       return "OK";
        default:             return "UNKNOWN";
    }
}

static const char* level_to_color(AlertLevel level) {
    switch (level) {
        case LEVEL_CRITICAL: return CLR_RED;
        case LEVEL_WARNING:  return CLR_YELLOW;
        case LEVEL_OK:       return CLR_GREEN;
        default:             return CLR_WHITE;
    }
}

/* ============================================================================
 *                             ALERT LOGGING
 * ============================================================================ */

static void log_alert(const char* component, AlertLevel level, const char* message) {
    char log_path[PATH_MAX];
    snprintf(log_path, sizeof(log_path), "%s/%s", g_project_root, ALERTS_LOG);
    
    FILE* fp = fopen(log_path, "a");
    if (!fp) return;
    
    /* Get timestamp */
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", t);
    
    fprintf(fp, "[%s] [%s] %s: %s\n", ts, level_to_string(level), component, message);
    fclose(fp);
    
    /* Update stats */
    g_stats.total++;
    if (level == LEVEL_WARNING) g_stats.warnings++;
    if (level == LEVEL_CRITICAL) g_stats.criticals++;
}

/* ============================================================================
 *                          PACKET COUNTING
 * ============================================================================ */

static int count_packets(const char* filepath) {
    FILE* fp = fopen(filepath, "r");
    if (!fp) return 0;
    
    int count = 0;
    char line[256];
    
    while (fgets(line, sizeof(line), fp)) {
        /* Skip comments and empty lines */
        if (line[0] != '#' && line[0] != '\n' && strlen(line) > 1) {
            count++;
        }
    }
    
    fclose(fp);
    return count;
}

/* ============================================================================
 *                            DISPLAY FUNCTIONS
 * ============================================================================ */

static void print_analysis_header(void) {
    printf("\n");
    printf("%s╔═══════════════════════════════════════════════════════════════╗%s\n", CLR_CYAN, CLR_RESET);
    printf("%s║%s           SYSTEM METRICS ANALYSIS REPORT                     %s║%s\n", CLR_CYAN, CLR_WHITE, CLR_CYAN, CLR_RESET);
    printf("%s╚═══════════════════════════════════════════════════════════════╝%s\n", CLR_CYAN, CLR_RESET);
    printf("\n");
}

static void print_metric_result(const char* name, double value, const char* unit, AlertLevel level) {
    const char* color = level_to_color(level);
    const char* status = level_to_string(level);
    
    printf("  %s%-15s%s", CLR_WHITE, name, CLR_RESET);
    printf(" │ ");
    printf("%s%6.1f%s%s", color, value, unit, CLR_RESET);
    printf(" │ ");
    printf("%s[%s]%s\n", color, status, CLR_RESET);
}

static void print_summary(void) {
    printf("\n");
    printf("%s┌─────────────────────────────────────────────────────────────────┐%s\n", CLR_BLUE, CLR_RESET);
    printf("%s│%s  SUMMARY                                                        %s│%s\n", CLR_BLUE, CLR_WHITE, CLR_BLUE, CLR_RESET);
    printf("%s├─────────────────────────────────────────────────────────────────┤%s\n", CLR_BLUE, CLR_RESET);
    
    if (g_stats.total == 0) {
        printf("%s│%s  %s✓ All systems nominal - No alerts generated%s                   %s│%s\n", 
               CLR_BLUE, CLR_RESET, CLR_GREEN, CLR_RESET, CLR_BLUE, CLR_RESET);
    } else {
        printf("%s│%s  Total Alerts: %-3d │ Warnings: %-3d │ Critical: %-3d            %s│%s\n",
               CLR_BLUE, CLR_WHITE, g_stats.total, g_stats.warnings, g_stats.criticals, CLR_BLUE, CLR_RESET);
    }
    
    printf("%s└─────────────────────────────────────────────────────────────────┘%s\n", CLR_BLUE, CLR_RESET);
    printf("\n");
}

/* ============================================================================
 *                          METRICS ANALYSIS
 * ============================================================================ */

static void analyze_metrics(SystemMetrics* metrics) {
    char report_path[PATH_MAX];
    snprintf(report_path, sizeof(report_path), "%s/%s", g_project_root, ANALYSIS_LOG);
    
    FILE* report = fopen(report_path, "a");
    if (!report) {
        fprintf(stderr, "%s[ERROR]%s Cannot open analysis log%s\n", CLR_RED, CLR_WHITE, CLR_RESET);
        return;
    }
    
    /* Write report header */
    fprintf(report, "\n");
    fprintf(report, "════════════════════════════════════════════════════════════════\n");
    fprintf(report, "  Analysis Report - %s\n", metrics->timestamp);
    fprintf(report, "════════════════════════════════════════════════════════════════\n");
    fprintf(report, "\n");
    
    print_analysis_header();
    printf("  %sTimestamp:%s %s\n\n", CLR_CYAN, CLR_RESET, metrics->timestamp);
    printf("  %s%-15s │ %6s │ Status%s\n", CLR_CYAN, "Metric", "Value", CLR_RESET);
    printf("  ─────────────────┼────────┼─────────\n");
    
    /* Analyze CPU */
    AlertLevel cpu_level = check_threshold(metrics->cpu, CPU_WARN_THRESHOLD, CPU_CRIT_THRESHOLD);
    print_metric_result("CPU Usage", metrics->cpu, "%", cpu_level);
    fprintf(report, "  CPU Usage:     %6.1f%%  [%s]\n", metrics->cpu, level_to_string(cpu_level));
    
    if (cpu_level != LEVEL_OK) {
        char msg[128];
        snprintf(msg, sizeof(msg), "CPU at %.1f%% (threshold: %d%%)", metrics->cpu,
                 cpu_level == LEVEL_CRITICAL ? CPU_CRIT_THRESHOLD : CPU_WARN_THRESHOLD);
        log_alert("CPU", cpu_level, msg);
    }
    
    /* Analyze RAM */
    AlertLevel ram_level = check_threshold(metrics->ram, RAM_WARN_THRESHOLD, RAM_CRIT_THRESHOLD);
    print_metric_result("RAM Usage", metrics->ram, "%", ram_level);
    fprintf(report, "  RAM Usage:     %6.1f%%  [%s]\n", metrics->ram, level_to_string(ram_level));
    
    if (ram_level != LEVEL_OK) {
        char msg[128];
        snprintf(msg, sizeof(msg), "RAM at %.1f%% (threshold: %d%%)", metrics->ram,
                 ram_level == LEVEL_CRITICAL ? RAM_CRIT_THRESHOLD : RAM_WARN_THRESHOLD);
        log_alert("RAM", ram_level, msg);
    }
    
    /* Analyze Disk */
    AlertLevel disk_level = check_threshold(metrics->disk, DISK_WARN_THRESHOLD, DISK_CRIT_THRESHOLD);
    print_metric_result("Disk Usage", metrics->disk, "%", disk_level);
    fprintf(report, "  Disk Usage:    %6.1f%%  [%s]\n", metrics->disk, level_to_string(disk_level));
    
    if (disk_level != LEVEL_OK) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Disk at %.1f%% (threshold: %d%%)", metrics->disk,
                 disk_level == LEVEL_CRITICAL ? DISK_CRIT_THRESHOLD : DISK_WARN_THRESHOLD);
        log_alert("DISK", disk_level, msg);
    }
    
    /* Analyze Network */
    AlertLevel net_level = check_threshold((double)metrics->packets, NET_WARN_THRESHOLD, NET_CRIT_THRESHOLD);
    print_metric_result("Network", (double)metrics->packets, " pkts", net_level);
    fprintf(report, "  Network:       %6d pkts  [%s]\n", metrics->packets, level_to_string(net_level));
    
    if (net_level != LEVEL_OK) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Network activity: %d packets (threshold: %d)",
                 metrics->packets, net_level == LEVEL_CRITICAL ? NET_CRIT_THRESHOLD : NET_WARN_THRESHOLD);
        log_alert("NETWORK", net_level, msg);
    }
    
    /* Write summary */
    fprintf(report, "\n");
    fprintf(report, "  Summary: %d alert(s) - %d warning(s), %d critical(s)\n",
            g_stats.total, g_stats.warnings, g_stats.criticals);
    fprintf(report, "════════════════════════════════════════════════════════════════\n");
    
    fclose(report);
    
    print_summary();
}

/* ============================================================================
 *                                 MAIN
 * ============================================================================ */

int main(int argc, char* argv[]) {
    /* Check arguments */
    if (argc != 5) {
        printf("\n");
        printf("%sUsage:%s %s <cpu%%> <ram%%> <disk%%> <netlog>%s\n\n", 
               CLR_CYAN, CLR_WHITE, argv[0], CLR_RESET);
        printf("  %scpu%%%s      CPU usage percentage (0-100)\n", CLR_YELLOW, CLR_RESET);
        printf("  %sram%%%s      RAM usage percentage (0-100)\n", CLR_YELLOW, CLR_RESET);
        printf("  %sdisk%%%s     Disk usage percentage (0-100)\n", CLR_YELLOW, CLR_RESET);
        printf("  %snetlog%s    Path to network log file\n", CLR_YELLOW, CLR_RESET);
        printf("\n");
        printf("  %sThresholds:%s\n", CLR_CYAN, CLR_RESET);
        printf("    CPU:  Warning=%d%% Critical=%d%%\n", CPU_WARN_THRESHOLD, CPU_CRIT_THRESHOLD);
        printf("    RAM:  Warning=%d%% Critical=%d%%\n", RAM_WARN_THRESHOLD, RAM_CRIT_THRESHOLD);
        printf("    Disk: Warning=%d%% Critical=%d%%\n", DISK_WARN_THRESHOLD, DISK_CRIT_THRESHOLD);
        printf("    Net:  Warning=%d Critical=%d packets\n", NET_WARN_THRESHOLD, NET_CRIT_THRESHOLD);
        printf("\n");
        return 1;
    }
    
    /* Initialize paths */
    init_paths();
    
    /* Parse metrics */
    SystemMetrics metrics;
    metrics.cpu = atof(argv[1]);
    metrics.ram = atof(argv[2]);
    metrics.disk = atof(argv[3]);
    metrics.packets = count_packets(argv[4]);
    
    /* Validate ranges */
    if (metrics.cpu < 0 || metrics.cpu > 100 ||
        metrics.ram < 0 || metrics.ram > 100 ||
        metrics.disk < 0 || metrics.disk > 100) {
        printf("%s[WARNING]%s Metrics should be in range 0-100%s\n", CLR_YELLOW, CLR_WHITE, CLR_RESET);
    }
    
    /* Get timestamp */
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    strftime(metrics.timestamp, sizeof(metrics.timestamp), "%Y-%m-%d %H:%M:%S", t);
    
    /* Run analysis */
    analyze_metrics(&metrics);
    
    /* Return code based on alerts */
    if (g_stats.criticals > 0) return 2;
    if (g_stats.warnings > 0) return 1;
    return 0;
}
