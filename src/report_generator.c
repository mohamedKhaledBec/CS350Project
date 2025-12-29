// report_generator.c
// Generates comprehensive HTML reports for system monitoring

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dirent.h>

#define MAX_LINE 1024
#define MAX_ALERTS 100

typedef struct {
    char timestamp[64];
    char message[256];
} Alert;

typedef struct {
    char timestamp[64];
    double cpu;
    double ram;
    double disk;
    int packets;
    char status[64];
} MetricEntry;

void read_alerts(Alert alerts[], int *count) {
    FILE *fp = fopen("../logs/alerts.log", "r");
    *count = 0;
    
    if (!fp) {
        printf("No alerts log found.\n");
        return;
    }
    
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), fp) && *count < MAX_ALERTS) {
        // Parse: [timestamp] message
        if (sscanf(line, "[%63[^]]] %255[^\n]", 
                   alerts[*count].timestamp, 
                   alerts[*count].message) == 2) {
            (*count)++;
        }
    }
    fclose(fp);
}

void read_metrics(MetricEntry entries[], int *count, int max_entries) {
    FILE *fp = fopen("../logs/analysis_report.log", "r");
    *count = 0;
    
    if (!fp) {
        printf("No analysis report found.\n");
        return;
    }
    
    char line[MAX_LINE];
    MetricEntry current;
    int in_section = 0;
    
    while (fgets(line, sizeof(line), fp) && *count < max_entries) {
        if (strstr(line, "Analysis Report -")) {
            in_section = 1;
            sscanf(line, "Analysis Report - %63[^\n]", current.timestamp);
        } else if (in_section && strstr(line, "CPU Usage:")) {
            sscanf(line, "CPU Usage: %lf%%", &current.cpu);
        } else if (in_section && strstr(line, "RAM Usage:")) {
            sscanf(line, "RAM Usage: %lf%%", &current.ram);
        } else if (in_section && strstr(line, "Disk Usage:")) {
            sscanf(line, "Disk Usage: %lf%%", &current.disk);
        } else if (in_section && strstr(line, "Network Packets:")) {
            sscanf(line, "Network Packets: %d", &current.packets);
        } else if (in_section && strstr(line, "STATUS:")) {
            sscanf(line, "STATUS: %63[^\n]", current.status);
            entries[*count] = current;
            (*count)++;
            in_section = 0;
        } else if (in_section && strstr(line, "========")) {
            if (strlen(current.status) == 0) {
                strcpy(current.status, "Warnings present");
            }
            entries[*count] = current;
            (*count)++;
            in_section = 0;
            memset(&current, 0, sizeof(MetricEntry));
        }
    }
    fclose(fp);
}

void generate_html_report(const char *output_file) {
    FILE *html = fopen(output_file, "w");
    if (!html) {
        perror("Error creating HTML report");
        return;
    }
    
    Alert alerts[MAX_ALERTS];
    int alert_count = 0;
    read_alerts(alerts, &alert_count);
    
    MetricEntry metrics[100];
    int metric_count = 0;
    read_metrics(metrics, &metric_count, 100);
    
    // Get current time
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char report_time[64];
    strftime(report_time, sizeof(report_time), "%Y-%m-%d %H:%M:%S", t);
    
    // Write HTML
    fprintf(html, "<!DOCTYPE html>\n");
    fprintf(html, "<html lang=\"en\">\n");
    fprintf(html, "<head>\n");
    fprintf(html, "    <meta charset=\"UTF-8\">\n");
    fprintf(html, "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n");
    fprintf(html, "    <title>System Monitoring Report</title>\n");
    fprintf(html, "    <style>\n");
    fprintf(html, "        * { margin: 0; padding: 0; box-sizing: border-box; }\n");
    fprintf(html, "        body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;\n");
    fprintf(html, "               background: linear-gradient(135deg, #667eea 0%%, #764ba2 100%%);\n");
    fprintf(html, "               min-height: 100vh; padding: 20px; }\n");
    fprintf(html, "        .container { max-width: 1400px; margin: 0 auto; }\n");
    fprintf(html, "        .header { background: white; padding: 30px; border-radius: 10px;\n");
    fprintf(html, "                  box-shadow: 0 10px 40px rgba(0,0,0,0.2); margin-bottom: 20px; }\n");
    fprintf(html, "        h1 { color: #667eea; font-size: 2.5em; margin-bottom: 10px; }\n");
    fprintf(html, "        .timestamp { color: #666; font-size: 0.9em; }\n");
    fprintf(html, "        .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));\n");
    fprintf(html, "                gap: 20px; margin-bottom: 20px; }\n");
    fprintf(html, "        .card { background: white; padding: 25px; border-radius: 10px;\n");
    fprintf(html, "                box-shadow: 0 5px 20px rgba(0,0,0,0.1); }\n");
    fprintf(html, "        .card h2 { color: #667eea; margin-bottom: 15px; font-size: 1.5em; }\n");
    fprintf(html, "        .metric { display: flex; justify-content: space-between; align-items: center;\n");
    fprintf(html, "                  padding: 10px 0; border-bottom: 1px solid #eee; }\n");
    fprintf(html, "        .metric:last-child { border-bottom: none; }\n");
    fprintf(html, "        .metric-name { font-weight: 600; color: #333; }\n");
    fprintf(html, "        .metric-value { font-size: 1.2em; font-weight: bold; }\n");
    fprintf(html, "        .good { color: #10b981; }\n");
    fprintf(html, "        .warning { color: #f59e0b; }\n");
    fprintf(html, "        .danger { color: #ef4444; }\n");
    fprintf(html, "        .alert-item { background: #fef2f2; border-left: 4px solid #ef4444;\n");
    fprintf(html, "                      padding: 15px; margin-bottom: 10px; border-radius: 5px; }\n");
    fprintf(html, "        .alert-time { font-size: 0.85em; color: #666; margin-bottom: 5px; }\n");
    fprintf(html, "        .alert-msg { color: #991b1b; font-weight: 600; }\n");
    fprintf(html, "        table { width: 100%%; border-collapse: collapse; }\n");
    fprintf(html, "        th, td { padding: 12px; text-align: left; border-bottom: 1px solid #ddd; }\n");
    fprintf(html, "        th { background: #667eea; color: white; font-weight: 600; }\n");
    fprintf(html, "        tr:hover { background: #f9fafb; }\n");
    fprintf(html, "        .badge { display: inline-block; padding: 4px 12px; border-radius: 20px;\n");
    fprintf(html, "                 font-size: 0.85em; font-weight: 600; }\n");
    fprintf(html, "        .badge-success { background: #d1fae5; color: #065f46; }\n");
    fprintf(html, "        .badge-warning { background: #fef3c7; color: #92400e; }\n");
    fprintf(html, "        .summary-stats { display: flex; justify-content: space-around;\n");
    fprintf(html, "                         margin: 20px 0; }\n");
    fprintf(html, "        .stat-box { text-align: center; }\n");
    fprintf(html, "        .stat-number { font-size: 2.5em; font-weight: bold; color: #667eea; }\n");
    fprintf(html, "        .stat-label { color: #666; margin-top: 5px; }\n");
    fprintf(html, "        @media print { body { background: white; } }\n");
    fprintf(html, "    </style>\n");
    fprintf(html, "</head>\n");
    fprintf(html, "<body>\n");
    fprintf(html, "    <div class=\"container\">\n");
    
    // Header
    fprintf(html, "        <div class=\"header\">\n");
    fprintf(html, "            <h1>🖥️ Real-Time System Monitoring Report</h1>\n");
    fprintf(html, "            <p class=\"timestamp\">Generated: %s</p>\n", report_time);
    fprintf(html, "        </div>\n");
    
    // Summary Stats
    fprintf(html, "        <div class=\"card\">\n");
    fprintf(html, "            <div class=\"summary-stats\">\n");
    fprintf(html, "                <div class=\"stat-box\">\n");
    fprintf(html, "                    <div class=\"stat-number\">%d</div>\n", metric_count);
    fprintf(html, "                    <div class=\"stat-label\">Total Scans</div>\n");
    fprintf(html, "                </div>\n");
    fprintf(html, "                <div class=\"stat-box\">\n");
    fprintf(html, "                    <div class=\"stat-number\">%d</div>\n", alert_count);
    fprintf(html, "                    <div class=\"stat-label\">Alerts Triggered</div>\n");
    fprintf(html, "                </div>\n");
    
    // Calculate average CPU from last 10 entries
    double avg_cpu = 0;
    int start = metric_count > 10 ? metric_count - 10 : 0;
    for (int i = start; i < metric_count; i++) {
        avg_cpu += metrics[i].cpu;
    }
    if (metric_count > start) avg_cpu /= (metric_count - start);
    
    fprintf(html, "                <div class=\"stat-box\">\n");
    fprintf(html, "                    <div class=\"stat-number\">%.1f%%</div>\n", avg_cpu);
    fprintf(html, "                    <div class=\"stat-label\">Avg CPU (Recent)</div>\n");
    fprintf(html, "                </div>\n");
    fprintf(html, "            </div>\n");
    fprintf(html, "        </div>\n");
    
    // Latest Metrics
    if (metric_count > 0) {
        MetricEntry latest = metrics[metric_count - 1];
        fprintf(html, "        <div class=\"grid\">\n");
        fprintf(html, "            <div class=\"card\">\n");
        fprintf(html, "                <h2>📊 Latest Metrics</h2>\n");
        fprintf(html, "                <div class=\"metric\">\n");
        fprintf(html, "                    <span class=\"metric-name\">CPU Usage</span>\n");
        fprintf(html, "                    <span class=\"metric-value %s\">%.1f%%</span>\n",
                latest.cpu > 80 ? "danger" : (latest.cpu > 60 ? "warning" : "good"), latest.cpu);
        fprintf(html, "                </div>\n");
        fprintf(html, "                <div class=\"metric\">\n");
        fprintf(html, "                    <span class=\"metric-name\">RAM Usage</span>\n");
        fprintf(html, "                    <span class=\"metric-value %s\">%.1f%%</span>\n",
                latest.ram > 85 ? "danger" : (latest.ram > 70 ? "warning" : "good"), latest.ram);
        fprintf(html, "                </div>\n");
        fprintf(html, "                <div class=\"metric\">\n");
        fprintf(html, "                    <span class=\"metric-name\">Disk Usage</span>\n");
        fprintf(html, "                    <span class=\"metric-value %s\">%.1f%%</span>\n",
                latest.disk > 90 ? "danger" : (latest.disk > 75 ? "warning" : "good"), latest.disk);
        fprintf(html, "                </div>\n");
        fprintf(html, "                <div class=\"metric\">\n");
        fprintf(html, "                    <span class=\"metric-name\">Network Packets</span>\n");
        fprintf(html, "                    <span class=\"metric-value\">%d</span>\n", latest.packets);
        fprintf(html, "                </div>\n");
        fprintf(html, "            </div>\n");
        fprintf(html, "        </div>\n");
    }
    
    // Recent Alerts
    if (alert_count > 0) {
        fprintf(html, "        <div class=\"card\">\n");
        fprintf(html, "            <h2>⚠️ Recent Alerts</h2>\n");
        int show_count = alert_count > 10 ? 10 : alert_count;
        for (int i = alert_count - show_count; i < alert_count; i++) {
            fprintf(html, "            <div class=\"alert-item\">\n");
            fprintf(html, "                <div class=\"alert-time\">%s</div>\n", alerts[i].timestamp);
            fprintf(html, "                <div class=\"alert-msg\">%s</div>\n", alerts[i].message);
            fprintf(html, "            </div>\n");
        }
        fprintf(html, "        </div>\n");
    }
    
    // Historical Data Table
    if (metric_count > 0) {
        fprintf(html, "        <div class=\"card\">\n");
        fprintf(html, "            <h2>📈 Historical Data</h2>\n");
        fprintf(html, "            <table>\n");
        fprintf(html, "                <tr>\n");
        fprintf(html, "                    <th>Timestamp</th>\n");
        fprintf(html, "                    <th>CPU</th>\n");
        fprintf(html, "                    <th>RAM</th>\n");
        fprintf(html, "                    <th>Disk</th>\n");
        fprintf(html, "                    <th>Packets</th>\n");
        fprintf(html, "                    <th>Status</th>\n");
        fprintf(html, "                </tr>\n");
        
        int show_count = metric_count > 20 ? 20 : metric_count;
        for (int i = metric_count - show_count; i < metric_count; i++) {
            fprintf(html, "                <tr>\n");
            fprintf(html, "                    <td>%s</td>\n", metrics[i].timestamp);
            fprintf(html, "                    <td>%.1f%%</td>\n", metrics[i].cpu);
            fprintf(html, "                    <td>%.1f%%</td>\n", metrics[i].ram);
            fprintf(html, "                    <td>%.1f%%</td>\n", metrics[i].disk);
            fprintf(html, "                    <td>%d</td>\n", metrics[i].packets);
            
            int has_warnings = (strstr(metrics[i].status, "nominal") != NULL);
            fprintf(html, "                    <td><span class=\"badge %s\">%s</span></td>\n",
                    has_warnings ? "badge-success" : "badge-warning",
                    has_warnings ? "OK" : "Warning");
            fprintf(html, "                </tr>\n");
        }
        fprintf(html, "            </table>\n");
        fprintf(html, "        </div>\n");
    }
    
    fprintf(html, "    </div>\n");
    fprintf(html, "</body>\n");
    fprintf(html, "</html>\n");
    
    fclose(html);
    printf("HTML report generated: %s\n", output_file);
}

int main(int argc, char *argv[]) {
    const char *output = (argc > 1) ? argv[1] : "../logs/system_report.html";
    
    printf("Generating HTML report...\n");
    generate_html_report(output);
    printf("Report generation complete!\n");
    
    return 0;
}
