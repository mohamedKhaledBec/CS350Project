#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define CPU_LIMIT 80
#define RAM_LIMIT 85
#define DISK_LIMIT 90
#define NET_PACKET_LIMIT 8

void log_to_kernel(const char *msg) {
    FILE *kernel = fopen("/proc/sysmon", "w");
    if (kernel) {
        fprintf(kernel, "%s\n", msg);
        fclose(kernel);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <cpu> <ram> <disk> <netlog>\n", argv[0]);
        return 1;
    }

    double cpu = atof(argv[1]);
    double ram = atof(argv[2]);
    double disk = atof(argv[3]);
    char *netfile = argv[4];

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);

    FILE *report = fopen("logs/daily_report.log", "a");
    if (!report) {
        perror("Error opening log file");
        return 1;
    }

    fprintf(report, "\n[%s]\n", timestamp);
    fprintf(report, "CPU: %.2f%% | RAM: %.2f%% | Disk: %.2f%%\n", cpu, ram, disk);

    int abnormal = 0;

    if (cpu > CPU_LIMIT || ram > RAM_LIMIT || disk > DISK_LIMIT) {
        fprintf(report, "⚠️ System Overload detected!\n");
        log_to_kernel("High usage detected in system resources.");
        abnormal = 1;
    }

    FILE *net = fopen(netfile, "r");
    if (net) {
        int packets = 0; 
        char line[256];
        while (fgets(line, sizeof(line), net)) packets++;
        fclose(net);

        fprintf(report, "Network packets captured: %d\n", packets);
        if (packets > NET_PACKET_LIMIT) {
            fprintf(report, "⚠️ Unusual network activity detected!\n");
            log_to_kernel("Network anomaly detected by tcpdump.");
            abnormal = 1;
        }
    }

    if (!abnormal)
        fprintf(report, "✅ All systems normal.\n");

    fclose(report);
    return 0;
}
