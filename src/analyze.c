/*
 * ============================================================================
 * File:        analyze.c
 * Project:     CS350 Real-Time System Monitor
 * Description: System Metrics Analyzer with Threshold-Based Alerting
 * Author:      CS350 Student Project
 * Date:        2024
 * ============================================================================
 *
 * Analyzes CPU, RAM, and Disk usage.
 * Generates WARNING / CRITICAL alerts.
 * Writes alerts to logs/alerts.log
 * Writes analysis reports to logs/analysis_report.log
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

/* Warning thresholds (%) */
#define CPU_WARN_THRESHOLD   70
#define RAM_WARN_THRESHOLD   75
#define DISK_WARN_THRESHOLD  80

/* Critical thresholds (%) */
#define CPU_CRIT_THRESHOLD   90
#define RAM_CRIT_THRESHOLD   90
#define DISK_CRIT_THRESHOLD  95

/* Log files */
#define ALERTS_LOG           "logs/alerts.log"
#define ANALYSIS_LOG         "logs/analysis_report.log"

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
    double cpu;
    double ram;
    double disk;
    char   timestamp[64];
} SystemMetrics;

typedef struct {
    int total;
    int warnings;
    int criticals;
} AlertStats;

/* ============================================================================
 *                            GLOBAL VARIABLES
 * ============================================================================ */

static char g_project_root[PATH_MAX] = {0};
static AlertStats g_stats = {0, 0, 0};

/* ============================================================================
 *                            UTILITY FUNCTIONS
 * ============================================================================ */

static void init_paths(void) {
    char exe_path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);

    if (len != -1) {
        exe_path[len] = '\0';
        char* dir = dirname(exe_path);
        char* parent = dirname(dir);
        strncpy(g_project_root, parent, PATH_MAX - 1);
    } else {
        getcwd(g_project_root, sizeof(g_project_root));
    }
}

static AlertLevel check_threshold(double value, double warn, double crit) {
    if (value >= crit) return LEVEL_CRITICAL;
    if (value >= warn) return LEVEL_WARNING;
    return LEVEL_OK;
}

static const char* level_to_string(AlertLevel level) {
    switch (level) {
        case LEVEL_CRITICAL: return "CRITICAL";
        case LEVEL_WARNING:  return "WARNING";
        default:             return "OK";
    }
}

static const char* level_to_color(AlertLevel level) {
    switch (level) {
        case LEVEL_CRITICAL: return CLR_RED;
        case LEVEL_WARNING:  return CLR_YELLOW;
        default:             return CLR_GREEN;
    }
}

/* ============================================================================
 *                             ALERT LOGGING
 * ============================================================================ */

static void log_alert(const char* component, AlertLevel level, const char* message) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s", g_project_root, ALERTS_LOG);

    FILE* fp = fopen(path, "a");
    if (!fp) return;

    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", t);

    fprintf(fp, "[%s] [%s] %s: %s\n",
            ts, level_to_string(level), component, message);

    fclose(fp);

    g_stats.total++;
    if (level == LEVEL_WARNING) g_stats.warnings++;
    if (level == LEVEL_CRITICAL) g_stats.criticals++;
}

/* ============================================================================
 *                            DISPLAY FUNCTIONS
 * ============================================================================ */

static void print_metric(const char* name, double value, const char* unit, AlertLevel level) {
    printf("  %-12s │ %s%6.1f%s │ %s[%s]%s\n",
           name,
           level_to_color(level), value, unit,
           level_to_color(level), level_to_string(level), CLR_RESET);
}

static void print_summary(void) {
    printf("\n");
    if (g_stats.total == 0) {
        printf("%s✓ No alerts generated — system stable%s\n", CLR_GREEN, CLR_RESET);
    } else {
        printf("%sAlerts:%s %d | Warnings: %d | Critical: %d\n",
               CLR_CYAN, CLR_RESET,
               g_stats.total, g_stats.warnings, g_stats.criticals);
    }
}

/* ============================================================================
 *                          METRICS ANALYSIS
 * ============================================================================ */

static void analyze_metrics(SystemMetrics* m) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s", g_project_root, ANALYSIS_LOG);

    FILE* report = fopen(path, "a");
    if (!report) return;

    fprintf(report, "\n=== Analysis Report - %s ===\n", m->timestamp);

    printf("\n%sSYSTEM METRICS ANALYSIS%s\n", CLR_CYAN, CLR_RESET);
    printf("  Metric       │ Value  │ Status\n");
    printf("  ─────────────┼────────┼─────────\n");

    AlertLevel cpu = check_threshold(m->cpu, CPU_WARN_THRESHOLD, CPU_CRIT_THRESHOLD);
    print_metric("CPU", m->cpu, "%", cpu);
    fprintf(report, "CPU: %.1f%% [%s]\n", m->cpu, level_to_string(cpu));
    if (cpu != LEVEL_OK) log_alert("CPU", cpu, "CPU usage exceeded threshold");

    AlertLevel ram = check_threshold(m->ram, RAM_WARN_THRESHOLD, RAM_CRIT_THRESHOLD);
    print_metric("RAM", m->ram, "%", ram);
    fprintf(report, "RAM: %.1f%% [%s]\n", m->ram, level_to_string(ram));
    if (ram != LEVEL_OK) log_alert("RAM", ram, "RAM usage exceeded threshold");

    AlertLevel disk = check_threshold(m->disk, DISK_WARN_THRESHOLD, DISK_CRIT_THRESHOLD);
    print_metric("Disk", m->disk, "%", disk);
    fprintf(report, "Disk: %.1f%% [%s]\n", m->disk, level_to_string(disk));
    if (disk != LEVEL_OK) log_alert("DISK", disk, "Disk usage exceeded threshold");

    fclose(report);
    print_summary();
}

/* ============================================================================
 *                                 MAIN
 * ============================================================================ */

int main(int argc, char* argv[]) {
    if (argc != 4) {
        printf("Usage: %s <cpu%%> <ram%%> <disk%%>\n", argv[0]);
        return 1;
    }

    init_paths();

    SystemMetrics m;
    m.cpu  = atof(argv[1]);
    m.ram  = atof(argv[2]);
    m.disk = atof(argv[3]);

    time_t now = time(NULL);
    strftime(m.timestamp, sizeof(m.timestamp),
             "%Y-%m-%d %H:%M:%S", localtime(&now));

    analyze_metrics(&m);

    if (g_stats.criticals) return 2;
    if (g_stats.warnings)  return 1;
    return 0;
}
