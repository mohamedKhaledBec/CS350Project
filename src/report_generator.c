/*
 * ============================================================================
 * File:        report_generator.c
 * Project:     CS350 Real-Time System Monitor
 * Description: Professional HTML Report Generator with System Analytics
 * Author:      CS350 Student Project
 * Date:        2024
 * ============================================================================
 * 
 * Features:
 *   - Professional dark theme HTML reports
 *   - Interactive progress bars with animations
 *   - Real-time metrics from /proc filesystem
 *   - Alert history with severity indicators
 *   - Scheduler task status summary
 *   - Mobile-responsive CSS grid layout
 * 
 * ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/statvfs.h>
#include <libgen.h>
#include <linux/limits.h>

/* ============================================================================
 *                              CONSTANTS
 * ============================================================================ */

#define MAX_SAMPLES     100     /* Maximum samples for averaging     */
#define MAX_ALERTS      50      /* Maximum alerts to display         */
#define MAX_LINE_LEN    512     /* Maximum line length for parsing   */

/* File paths */
#define SCHEDULER_LOG   "logs/scheduler.log"
#define ALERTS_LOG      "logs/alerts.log"
#define DEFAULT_OUTPUT  "logs/system_report.html"

/* Thresholds */
#define CPU_WARNING     70.0
#define CPU_CRITICAL    90.0
#define RAM_WARNING     75.0
#define RAM_CRITICAL    90.0
#define DISK_WARNING    80.0
#define DISK_CRITICAL   95.0

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
    double  cpu;            /* Average CPU usage            */
    double  ram;            /* Average RAM usage            */
    double  disk;           /* Current disk usage           */
    int     samples;        /* Number of samples collected  */
} SystemMetrics;

typedef struct {
    char    timestamp[32];  /* Alert timestamp              */
    char    severity[16];   /* WARNING, CRITICAL, INFO      */
    char    message[256];   /* Alert message text           */
} AlertEntry;

typedef struct {
    char    name[32];       /* Task name                    */
    int     priority;       /* Task priority (0-3)          */
    int     interval;       /* Execution interval           */
    int     executions;     /* Total executions             */
} TaskStatus;

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
 * get_status_class()
 * Returns CSS class name based on metric value and thresholds.
 */
static const char* get_status_class(double value, double warn, double crit) {
    if (value >= crit) return "critical";
    if (value >= warn) return "warning";
    return "ok";
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
        usleep(100000);  /* Wait 100ms for second sample */
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
 * collect_metrics()
 * Collects current system metrics.
 */
static void collect_metrics(SystemMetrics* metrics) {
    metrics->cpu = get_cpu_usage();
    metrics->ram = get_ram_usage();
    metrics->disk = get_disk_usage();
    metrics->samples = 1;
}

/* ============================================================================
 *                           ALERT READING
 * ============================================================================ */

/*
 * read_alerts()
 * Reads recent alerts from log files.
 */
static int read_alerts(AlertEntry alerts[], int max_count) {
    char log_path[PATH_MAX];
    snprintf(log_path, sizeof(log_path), "%s/%s", g_project_root, ALERTS_LOG);
    
    FILE* fp = fopen(log_path, "r");
    int count = 0;
    
    if (!fp) {
        /* Try scheduler log as fallback */
        snprintf(log_path, sizeof(log_path), "%s/%s", g_project_root, SCHEDULER_LOG);
        fp = fopen(log_path, "r");
        if (!fp) return 0;
    }
    
    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), fp) && count < max_count) {
        /* Look for alert keywords */
        if (strstr(line, "CRITICAL") || strstr(line, "WARNING") || 
            strstr(line, "[ALERT]") || strstr(line, "STARVING")) {
            
            /* Parse timestamp if present */
            if (line[0] == '[') {
                char* end = strchr(line, ']');
                if (end) {
                    int len = (int)(end - line - 1);
                    if (len > 0 && len < 32) {
                        strncpy(alerts[count].timestamp, line + 1, len);
                        alerts[count].timestamp[len] = '\0';
                    }
                }
            } else {
                time_t now = time(NULL);
                struct tm* t = localtime(&now);
                strftime(alerts[count].timestamp, sizeof(alerts[count].timestamp),
                         "%Y-%m-%d %H:%M:%S", t);
            }
            
            /* Determine severity */
            if (strstr(line, "CRITICAL") || strstr(line, "STARVING")) {
                strncpy(alerts[count].severity, "CRITICAL", sizeof(alerts[count].severity));
            } else if (strstr(line, "WARNING")) {
                strncpy(alerts[count].severity, "WARNING", sizeof(alerts[count].severity));
            } else {
                strncpy(alerts[count].severity, "INFO", sizeof(alerts[count].severity));
            }
            
            /* Copy message (remove newline) */
            strncpy(alerts[count].message, line, sizeof(alerts[count].message) - 1);
            alerts[count].message[sizeof(alerts[count].message) - 1] = '\0';
            char* nl = strchr(alerts[count].message, '\n');
            if (nl) *nl = '\0';
            
            count++;
        }
    }
    
    fclose(fp);
    return count;
}

/* ============================================================================
 *                         HTML GENERATION
 * ============================================================================ */

/*
 * write_html_header()
 * Writes HTML document header with CSS styling.
 */
static void write_html_header(FILE* html, const char* timestamp) {
    fprintf(html,
        "<!DOCTYPE html>\n"
        "<html lang=\"en\">\n"
        "<head>\n"
        "    <meta charset=\"UTF-8\">\n"
        "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
        "    <title>System Monitor Report - %s</title>\n"
        "    <style>\n"
        "        :root {\n"
        "            --bg-primary: #0f172a;\n"
        "            --bg-secondary: #1e293b;\n"
        "            --bg-card: #1e293b;\n"
        "            --border: #334155;\n"
        "            --text-primary: #f8fafc;\n"
        "            --text-secondary: #94a3b8;\n"
        "            --accent-blue: #3b82f6;\n"
        "            --accent-purple: #8b5cf6;\n"
        "            --accent-pink: #ec4899;\n"
        "            --accent-green: #22c55e;\n"
        "            --accent-yellow: #f59e0b;\n"
        "            --accent-red: #ef4444;\n"
        "        }\n"
        "\n"
        "        * { margin: 0; padding: 0; box-sizing: border-box; }\n"
        "\n"
        "        body {\n"
        "            font-family: 'Segoe UI', system-ui, -apple-system, sans-serif;\n"
        "            background: var(--bg-primary);\n"
        "            color: var(--text-primary);\n"
        "            line-height: 1.6;\n"
        "            padding: 24px;\n"
        "        }\n"
        "\n"
        "        .container {\n"
        "            max-width: 1200px;\n"
        "            margin: 0 auto;\n"
        "        }\n"
        "\n"
        "        /* Header */\n"
        "        .header {\n"
        "            background: var(--bg-card);\n"
        "            border: 1px solid var(--border);\n"
        "            border-radius: 16px;\n"
        "            padding: 32px;\n"
        "            margin-bottom: 24px;\n"
        "            text-align: center;\n"
        "        }\n"
        "\n"
        "        .header h1 {\n"
        "            font-size: 2.5rem;\n"
        "            font-weight: 700;\n"
        "            background: linear-gradient(135deg, var(--accent-blue), var(--accent-purple));\n"
        "            -webkit-background-clip: text;\n"
        "            -webkit-text-fill-color: transparent;\n"
        "            background-clip: text;\n"
        "            margin-bottom: 8px;\n"
        "        }\n"
        "\n"
        "        .header .timestamp {\n"
        "            color: var(--text-secondary);\n"
        "            font-size: 0.95rem;\n"
        "        }\n"
        "\n"
        "        .header .status {\n"
        "            display: inline-block;\n"
        "            margin-top: 12px;\n"
        "            padding: 6px 16px;\n"
        "            border-radius: 20px;\n"
        "            font-size: 0.85rem;\n"
        "            font-weight: 600;\n"
        "        }\n"
        "\n"
        "        .header .status.ok { background: rgba(34, 197, 94, 0.2); color: var(--accent-green); }\n"
        "        .header .status.warning { background: rgba(245, 158, 11, 0.2); color: var(--accent-yellow); }\n"
        "        .header .status.critical { background: rgba(239, 68, 68, 0.2); color: var(--accent-red); }\n"
        "\n"
        "        /* Grid Layout */\n"
        "        .grid {\n"
        "            display: grid;\n"
        "            grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));\n"
        "            gap: 20px;\n"
        "            margin-bottom: 24px;\n"
        "        }\n"
        "\n"
        "        /* Cards */\n"
        "        .card {\n"
        "            background: var(--bg-card);\n"
        "            border: 1px solid var(--border);\n"
        "            border-radius: 16px;\n"
        "            padding: 24px;\n"
        "        }\n"
        "\n"
        "        .card h2 {\n"
        "            font-size: 1.1rem;\n"
        "            color: var(--text-secondary);\n"
        "            margin-bottom: 16px;\n"
        "            font-weight: 500;\n"
        "        }\n"
        "\n"
        "        .card .value {\n"
        "            font-size: 3rem;\n"
        "            font-weight: 700;\n"
        "            margin-bottom: 16px;\n"
        "        }\n"
        "\n"
        "        .card .value.cpu { color: var(--accent-blue); }\n"
        "        .card .value.ram { color: var(--accent-purple); }\n"
        "        .card .value.disk { color: var(--accent-pink); }\n"
        "\n"
        "        /* Progress Bars */\n"
        "        .progress-container {\n"
        "            height: 12px;\n"
        "            background: var(--bg-primary);\n"
        "            border-radius: 6px;\n"
        "            overflow: hidden;\n"
        "        }\n"
        "\n"
        "        .progress-bar {\n"
        "            height: 100%%;\n"
        "            border-radius: 6px;\n"
        "            transition: width 0.5s ease;\n"
        "        }\n"
        "\n"
        "        .progress-bar.cpu { background: linear-gradient(90deg, #2563eb, #60a5fa); }\n"
        "        .progress-bar.ram { background: linear-gradient(90deg, #7c3aed, #a78bfa); }\n"
        "        .progress-bar.disk { background: linear-gradient(90deg, #db2777, #f472b6); }\n"
        "\n"
        "        .progress-bar.warning { background: linear-gradient(90deg, #d97706, #fbbf24); }\n"
        "        .progress-bar.critical { background: linear-gradient(90deg, #dc2626, #f87171); }\n"
        "\n"
        "        /* Status Badge */\n"
        "        .card .badge {\n"
        "            display: inline-block;\n"
        "            margin-top: 12px;\n"
        "            padding: 4px 12px;\n"
        "            border-radius: 12px;\n"
        "            font-size: 0.8rem;\n"
        "            font-weight: 600;\n"
        "        }\n"
        "\n"
        "        .badge.ok { background: rgba(34, 197, 94, 0.15); color: var(--accent-green); }\n"
        "        .badge.warning { background: rgba(245, 158, 11, 0.15); color: var(--accent-yellow); }\n"
        "        .badge.critical { background: rgba(239, 68, 68, 0.15); color: var(--accent-red); }\n"
        "\n"
        "        /* Alerts Section */\n"
        "        .alerts-container { margin-bottom: 24px; }\n"
        "\n"
        "        .alert-item {\n"
        "            background: var(--bg-secondary);\n"
        "            border-left: 4px solid var(--border);\n"
        "            padding: 16px;\n"
        "            margin-bottom: 12px;\n"
        "            border-radius: 0 8px 8px 0;\n"
        "        }\n"
        "\n"
        "        .alert-item.warning { border-left-color: var(--accent-yellow); }\n"
        "        .alert-item.critical { border-left-color: var(--accent-red); }\n"
        "\n"
        "        .alert-item .alert-header {\n"
        "            display: flex;\n"
        "            justify-content: space-between;\n"
        "            align-items: center;\n"
        "            margin-bottom: 8px;\n"
        "        }\n"
        "\n"
        "        .alert-item .severity {\n"
        "            font-size: 0.75rem;\n"
        "            font-weight: 600;\n"
        "            padding: 2px 8px;\n"
        "            border-radius: 4px;\n"
        "        }\n"
        "\n"
        "        .severity.warning { background: rgba(245, 158, 11, 0.2); color: var(--accent-yellow); }\n"
        "        .severity.critical { background: rgba(239, 68, 68, 0.2); color: var(--accent-red); }\n"
        "\n"
        "        .alert-item .timestamp {\n"
        "            font-size: 0.8rem;\n"
        "            color: var(--text-secondary);\n"
        "        }\n"
        "\n"
        "        .alert-item .message {\n"
        "            font-size: 0.9rem;\n"
        "            color: var(--text-primary);\n"
        "        }\n"
        "\n"
        "        .no-alerts {\n"
        "            text-align: center;\n"
        "            padding: 40px;\n"
        "            color: var(--accent-green);\n"
        "        }\n"
        "\n"
        "        .no-alerts .icon { font-size: 3rem; margin-bottom: 12px; }\n"
        "\n"
        "        /* Footer */\n"
        "        .footer {\n"
        "            text-align: center;\n"
        "            padding: 24px;\n"
        "            color: var(--text-secondary);\n"
        "            font-size: 0.85rem;\n"
        "        }\n"
        "\n"
        "        /* Responsive */\n"
        "        @media (max-width: 768px) {\n"
        "            body { padding: 12px; }\n"
        "            .header h1 { font-size: 1.8rem; }\n"
        "            .card .value { font-size: 2.2rem; }\n"
        "        }\n"
        "    </style>\n"
        "</head>\n"
        "<body>\n"
        "<div class=\"container\">\n",
        timestamp);
}

/*
 * write_html_content()
 * Writes the main report content.
 */
static void write_html_content(FILE* html, SystemMetrics* metrics, 
                               AlertEntry alerts[], int alert_count,
                               const char* timestamp) {
    /* Determine overall status */
    const char* overall_status = "ok";
    const char* status_text = "✓ All Systems Normal";
    
    if (metrics->cpu >= CPU_CRITICAL || metrics->ram >= RAM_CRITICAL || 
        metrics->disk >= DISK_CRITICAL) {
        overall_status = "critical";
        status_text = "⚠ Critical Alerts Active";
    } else if (metrics->cpu >= CPU_WARNING || metrics->ram >= RAM_WARNING || 
               metrics->disk >= DISK_WARNING) {
        overall_status = "warning";
        status_text = "△ Warnings Present";
    }
    
    /* Header */
    fprintf(html,
        "    <div class=\"header\">\n"
        "        <h1>📊 System Monitor Report</h1>\n"
        "        <p class=\"timestamp\">Generated: %s</p>\n"
        "        <span class=\"status %s\">%s</span>\n"
        "    </div>\n\n",
        timestamp, overall_status, status_text);
    
    /* Metrics Grid */
    fprintf(html, "    <div class=\"grid\">\n");
    
    /* CPU Card */
    const char* cpu_status = get_status_class(metrics->cpu, CPU_WARNING, CPU_CRITICAL);
    fprintf(html,
        "        <div class=\"card\">\n"
        "            <h2>CPU Usage</h2>\n"
        "            <div class=\"value cpu\">%.1f%%</div>\n"
        "            <div class=\"progress-container\">\n"
        "                <div class=\"progress-bar cpu %s\" style=\"width: %.0f%%\"></div>\n"
        "            </div>\n"
        "            <span class=\"badge %s\">%s</span>\n"
        "        </div>\n",
        metrics->cpu, 
        cpu_status, metrics->cpu,
        cpu_status,
        cpu_status[0] == 'o' ? "Normal" : (cpu_status[0] == 'w' ? "Warning" : "Critical"));
    
    /* RAM Card */
    const char* ram_status = get_status_class(metrics->ram, RAM_WARNING, RAM_CRITICAL);
    fprintf(html,
        "        <div class=\"card\">\n"
        "            <h2>RAM Usage</h2>\n"
        "            <div class=\"value ram\">%.1f%%</div>\n"
        "            <div class=\"progress-container\">\n"
        "                <div class=\"progress-bar ram %s\" style=\"width: %.0f%%\"></div>\n"
        "            </div>\n"
        "            <span class=\"badge %s\">%s</span>\n"
        "        </div>\n",
        metrics->ram,
        ram_status, metrics->ram,
        ram_status,
        ram_status[0] == 'o' ? "Normal" : (ram_status[0] == 'w' ? "Warning" : "Critical"));
    
    /* Disk Card */
    const char* disk_status = get_status_class(metrics->disk, DISK_WARNING, DISK_CRITICAL);
    fprintf(html,
        "        <div class=\"card\">\n"
        "            <h2>Disk Usage</h2>\n"
        "            <div class=\"value disk\">%.1f%%</div>\n"
        "            <div class=\"progress-container\">\n"
        "                <div class=\"progress-bar disk %s\" style=\"width: %.0f%%\"></div>\n"
        "            </div>\n"
        "            <span class=\"badge %s\">%s</span>\n"
        "        </div>\n",
        metrics->disk,
        disk_status, metrics->disk,
        disk_status,
        disk_status[0] == 'o' ? "Normal" : (disk_status[0] == 'w' ? "Warning" : "Critical"));
    
    fprintf(html, "    </div>\n\n");
    
    /* Alerts Section */
    fprintf(html,
        "    <div class=\"card alerts-container\">\n"
        "        <h2>⚠️ Recent Alerts</h2>\n");
    
    if (alert_count == 0) {
        fprintf(html,
            "        <div class=\"no-alerts\">\n"
            "            <div class=\"icon\">✓</div>\n"
            "            <p>No alerts - System running smoothly</p>\n"
            "        </div>\n");
    } else {
        for (int i = 0; i < alert_count && i < 10; i++) {
            const char* alert_class = "info";
            if (strcmp(alerts[i].severity, "CRITICAL") == 0) {
                alert_class = "critical";
            } else if (strcmp(alerts[i].severity, "WARNING") == 0) {
                alert_class = "warning";
            }
            
            fprintf(html,
                "        <div class=\"alert-item %s\">\n"
                "            <div class=\"alert-header\">\n"
                "                <span class=\"severity %s\">%s</span>\n"
                "                <span class=\"timestamp\">%s</span>\n"
                "            </div>\n"
                "            <div class=\"message\">%s</div>\n"
                "        </div>\n",
                alert_class, alert_class, alerts[i].severity,
                alerts[i].timestamp, alerts[i].message);
        }
    }
    
    fprintf(html, "    </div>\n\n");
    
    /* Footer */
    fprintf(html,
        "    <div class=\"footer\">\n"
        "        <p>CS350 Real-Time System Monitor | Generated by report_generator</p>\n"
        "    </div>\n"
        "</div>\n"
        "</body>\n"
        "</html>\n");
}

/*
 * generate_report()
 * Main function to generate the complete HTML report.
 */
static void generate_report(const char* output_file) {
    FILE* html = fopen(output_file, "w");
    if (!html) {
        fprintf(stderr, "%s[ERROR]%s Cannot create report: %s%s\n", 
                "\033[1;31m", CLR_WHITE, output_file, CLR_RESET);
        return;
    }
    
    /* Collect current metrics */
    SystemMetrics metrics;
    collect_metrics(&metrics);
    
    /* Read alerts */
    AlertEntry alerts[MAX_ALERTS];
    int alert_count = read_alerts(alerts, MAX_ALERTS);
    
    /* Get timestamp */
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);
    
    /* Write HTML */
    write_html_header(html, timestamp);
    write_html_content(html, &metrics, alerts, alert_count, timestamp);
    
    fclose(html);
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
    printf("%s║%s          SYSTEM REPORT GENERATOR                             %s║%s\n", CLR_CYAN, CLR_WHITE, CLR_CYAN, CLR_RESET);
    printf("%s╚═══════════════════════════════════════════════════════════════╝%s\n", CLR_CYAN, CLR_RESET);
    printf("\n");
    
    printf("  %s→%s Collecting system metrics...\n", CLR_GREEN, CLR_RESET);
    printf("  %s→%s Reading alert history...\n", CLR_GREEN, CLR_RESET);
    printf("  %s→%s Generating HTML report...\n", CLR_GREEN, CLR_RESET);
    
    generate_report(output_path);
    
    printf("\n");
    printf("  %s✓ Report generated:%s %s%s\n", CLR_GREEN, CLR_WHITE, output_path, CLR_RESET);
    printf("\n");
    
    return 0;
}
