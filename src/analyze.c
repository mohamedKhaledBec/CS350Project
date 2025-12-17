#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CPU_THRESHOLD 80
#define RAM_THRESHOLD 85
#define DISK_THRESHOLD 90
#define PACKET_THRESHOLD 8

typedef struct {
    double cpu;
    double ram;
    double disk;
    int packet_count;
    char timestamp[64];
} SystemMetrics;

void log_alert(const char *message) {
    FILE *fp = fopen("../logs/alerts.log", "a");
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

int count_packets(const char *netfile) {
    FILE *fp = fopen(netfile, "r");
    if (!fp) return 0;
    
    int count = 0;
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (strlen(line) > 0 && line[0] != '#') {
            count++;
        }
    }
    fclose(fp);
    return count;
}

void analyze_metrics(SystemMetrics *metrics) {
    FILE *report = fopen("../logs/analysis_report.log", "a");
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
    
    // Check CPU
    if (metrics->cpu > CPU_THRESHOLD) {
        fprintf(report, "WARNING: CPU usage is HIGH (%.2f%%)\n", metrics->cpu);
        log_alert("HIGH CPU USAGE");
        alerts++;
    }
    
    // Check RAM
    if (metrics->ram > RAM_THRESHOLD) {
        fprintf(report, "WARNING: RAM usage is HIGH (%.2f%%)\n", metrics->ram);
        log_alert("HIGH RAM USAGE");
        alerts++;
    }
    
    // Check Disk
    if (metrics->disk > DISK_THRESHOLD) {
        fprintf(report, "WARNING: Disk usage is HIGH (%.2f%%)\n", metrics->disk);
        log_alert("HIGH DISK USAGE");
        alerts++;
    }
    
    // Check Network
    if (metrics->packet_count > PACKET_THRESHOLD) {
        fprintf(report, "WARNING: High network activity (%d packets)\n", metrics->packet_count);
        log_alert("HIGH NETWORK ACTIVITY");
        alerts++;
    }
    
    if (alerts == 0) {
        fprintf(report, "STATUS: All systems nominal\n");
        printf("[OK] System metrics are within normal range\n");
    }
    
    fprintf(report, "========================================\n");
    fclose(report);
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <cpu> <ram> <disk> <netlog>\n", argv[0]);
        return 1;
    }
    
    SystemMetrics metrics;
    metrics.cpu = atof(argv[1]);
    metrics.ram = atof(argv[2]);
    metrics.disk = atof(argv[3]);
    
    // Count packets in network log
    metrics.packet_count = count_packets(argv[4]);
    
    // Get timestamp
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(metrics.timestamp, sizeof(metrics.timestamp), "%Y-%m-%d %H:%M:%S", t);
    
    printf("\n=== SYSTEM ANALYSIS ===\n");
    printf("CPU: %.2f%% | RAM: %.2f%% | Disk: %.2f%% | Packets: %d\n",
           metrics.cpu, metrics.ram, metrics.disk, metrics.packet_count);
    printf("Time: %s\n", metrics.timestamp);
    
    // Analyze and generate report
    analyze_metrics(&metrics);
    
    printf("Analysis complete. Check ../logs/analysis_report.log for details.\n\n");
    
    return 0;
}
