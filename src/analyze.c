/**
 * @file analyze.c
 * @brief System Metrics Analyzer for Real-Time Linux Monitor
 * 
 * This module analyzes system metrics (CPU, RAM, Disk, Network) and generates
 * alerts when thresholds are exceeded. Part of CS350 Real-Time Systems Project.
 * 
 * @author CS350 Student Project
 * @date 2024
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <libgen.h>
#include <linux/limits.h>

/* Threshold constants for alert generation */
#define CPU_THRESHOLD     80    /* CPU usage percentage threshold */
#define RAM_THRESHOLD     85    /* RAM usage percentage threshold */
#define DISK_THRESHOLD    90    /* Disk usage percentage threshold */
#define PACKET_THRESHOLD  8     /* Network packets per second threshold */

/* Global path to project root directory */
static char g_project_root[PATH_MAX] = {0};

/**
 * @brief Structure to hold system metrics snapshot
 */
typedef struct {
    double cpu;           /* CPU usage percentage */
    double ram;           /* RAM usage percentage */
    double disk;          /* Disk usage percentage */
    int packet_count;     /* Network packet count */
    char timestamp[64];   /* Timestamp string */
} SystemMetrics;

/**
 * @brief Initialize project root path based on executable location
 * @param argv0 The argv[0] from main
 */
static void init_project_root(const char *argv0) {
    char exe_path[PATH_MAX];
    
    /* Try to get path from /proc/self/exe (Linux-specific) */
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len != -1) {
        exe_path[len] = '\0';
        /* Go up from build/ to project root */
        char *dir = dirname(exe_path);
        char *parent = dirname(dir);
        strncpy(g_project_root, parent, PATH_MAX - 1);
    } else {
        /* Fallback: use current directory */
        if (getcwd(g_project_root, sizeof(g_project_root)) == NULL) {
            strncpy(g_project_root, ".", PATH_MAX - 1);
        }
    }
    (void)argv0;  /* Suppress unused warning */
}

/**
 * @brief Log an alert message to the alerts log file
 * @param message Alert message to log
 */
void log_alert(const char *message) {
    char log_path[PATH_MAX];
    snprintf(log_path, sizeof(log_path), "%s/logs/alerts.log", g_project_root);
    
    FILE *fp = fopen(log_path, "a");
    if (!fp) {
        perror("Error opening alerts log");
        return;
    }
    
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char ts[64];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", t);
    
    fprintf(fp, "[%s] %s\n", ts, message);
    printf("[ALERT] %s\n", message);
    fclose(fp);
}

/**
 * @brief Count non-comment lines in network log file
 * @param netfile Path to the network log file
 * @return Number of packet entries in the log
 */
int count_packets(const char *netfile) {
    FILE *fp = fopen(netfile, "r");
    if (!fp) return 0;
    
    int count = 0;
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        /* Skip empty lines and comments */
        if (strlen(line) > 0 && line[0] != '#') {
            count++;
        }
    }
    fclose(fp);
    return count;
}

/**
 * @brief Analyze system metrics and generate alerts if thresholds exceeded
 * @param metrics Pointer to SystemMetrics structure with current values
 */
void analyze_metrics(SystemMetrics *metrics) {
    char report_path[PATH_MAX];
    snprintf(report_path, sizeof(report_path), "%s/logs/analysis_report.log", g_project_root);
    
    FILE *report = fopen(report_path, "a");
    if (!report) {
        perror("Error opening report file");
        return;
    }
    
    fprintf(report, "\n========================================\n");
    fprintf(report, "Analysis Report - %s\n", metrics->timestamp);
    fprintf(report, "========================================\n");
    fprintf(report, "CPU Usage: %.2f%%\n", metrics->cpu);
    fprintf(report, "RAM Usage: %.2f%%\n", metrics->ram);
    fprintf(report, "Disk Usage: %.2f%%\n", metrics->disk);
    fprintf(report, "Network Packets: %d\n", metrics->packet_count);
    fprintf(report, "----------------------------------------\n");
    
    int alerts = 0;
    
    /* Threshold checks - generate alerts for each exceeded metric */
    
    /* Check CPU utilization */
    if (metrics->cpu > CPU_THRESHOLD) {
        fprintf(report, "WARNING: CPU usage is HIGH (%.2f%%)\n", metrics->cpu);
        log_alert("HIGH CPU USAGE");
        alerts++;
    }
    
    /* Check RAM utilization */
    if (metrics->ram > RAM_THRESHOLD) {
        fprintf(report, "WARNING: RAM usage is HIGH (%.2f%%)\n", metrics->ram);
        log_alert("HIGH RAM USAGE");
        alerts++;
    }
    
    /* Check Disk utilization */
    if (metrics->disk > DISK_THRESHOLD) {
        fprintf(report, "WARNING: Disk usage is HIGH (%.2f%%)\n", metrics->disk);
        log_alert("HIGH DISK USAGE");
        alerts++;
    }
    
    /* Check Network activity */
    if (metrics->packet_count > PACKET_THRESHOLD) {
        fprintf(report, "WARNING: High network activity (%d packets)\n", metrics->packet_count);
        log_alert("HIGH NETWORK ACTIVITY");
        alerts++;
    }
    
    /* Summary status */
    if (alerts == 0) {
        fprintf(report, "STATUS: All systems nominal\n");
        printf("[OK] System metrics are within normal range\n");
    } else {
        printf("[ALERT] %d threshold(s) exceeded - check logs\n", alerts);
    }
    
    fprintf(report, "========================================\n");
    fclose(report);
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <cpu> <ram> <disk> <netlog>\n", argv[0]);
        fprintf(stderr, "  cpu    - CPU usage percentage (0-100)\n");
        fprintf(stderr, "  ram    - RAM usage percentage (0-100)\n");
        fprintf(stderr, "  disk   - Disk usage percentage (0-100)\n");
        fprintf(stderr, "  netlog - Path to network log file\n");
        return 1;
    }
    
    /* Initialize project root for proper path resolution */
    init_project_root(argv[0]);
    
    SystemMetrics metrics;
    metrics.cpu = atof(argv[1]);
    metrics.ram = atof(argv[2]);
    metrics.disk = atof(argv[3]);
    
    /* Validate input ranges */
    if (metrics.cpu < 0 || metrics.cpu > 100 ||
        metrics.ram < 0 || metrics.ram > 100 ||
        metrics.disk < 0 || metrics.disk > 100) {
        fprintf(stderr, "Warning: Metrics should be in range 0-100\n");
    }
    
    /* Count packets in network log */
    metrics.packet_count = count_packets(argv[4]);
    
    /* Get timestamp for the analysis report */
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(metrics.timestamp, sizeof(metrics.timestamp), "%Y-%m-%d %H:%M:%S", t);
    
    /* Display summary to console */
    printf("\n=== SYSTEM ANALYSIS ===\n");
    printf("CPU: %.2f%% | RAM: %.2f%% | Disk: %.2f%% | Packets: %d\n",
           metrics.cpu, metrics.ram, metrics.disk, metrics.packet_count);
    printf("Time: %s\n", metrics.timestamp);
    
    /* Perform analysis and generate report */
    analyze_metrics(&metrics);
    
    printf("Analysis complete. Check %s/logs/analysis_report.log for details.\n\n", g_project_root);
    
    return 0;
}
