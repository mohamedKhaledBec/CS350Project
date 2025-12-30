/*
 * ============================================================================
 * File:        gtk_gui.c
 * Project:     CS350 Real-Time System Monitor
 * Description: GTK3 Graphical User Interface
 * Author:      CS350 Student Project
 * Date:        2024
 * ============================================================================
 * 
 * Features:
 *   - Real-time system metrics (CPU, RAM, Disk, Network)
 *   - Scheduler task control with priority/interval adjustment
 *   - Starvation warning indicators
 *   - Modern dark theme with color-coded visual feedback
 *   - System performance monitoring via /proc filesystem
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
#include <sys/statvfs.h>

/* ============================================================================
 *                              CONSTANTS
 * ============================================================================ */

#define REFRESH_INTERVAL_MS     1000    /* GUI refresh interval (milliseconds)  */
#define STARVATION_THRESHOLD    30      /* Seconds before starvation warning    */
#define NUM_TASKS               4       /* Number of scheduler tasks            */

/* Configuration file paths */
#define PRIORITY_CONFIG_FILE    "config/priority_override.conf"
#define INTERVAL_CONFIG_FILE    "config/interval_override.conf"

/* Alert thresholds */
#define CPU_WARNING_THRESHOLD   70.0
#define CPU_CRITICAL_THRESHOLD  90.0
#define RAM_WARNING_THRESHOLD   75.0
#define RAM_CRITICAL_THRESHOLD  90.0
#define DISK_WARNING_THRESHOLD  80.0
#define DISK_CRITICAL_THRESHOLD 95.0

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
    GtkWidget*      cpu_progress;
    GtkWidget*      cpu_status;
    GtkWidget*      ram_progress;
    GtkWidget*      ram_status;
    GtkWidget*      disk_progress;
    GtkWidget*      disk_status;
    GtkWidget*      network_label;
    
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

/* ============================================================================
 *                           TASK DEFINITIONS
 * ============================================================================ */

static const char* TASK_NAMES[NUM_TASKS] = {
    "System Monitor",
    "Ethernet Fetch",
    "Analyzer",
    "Report Generator"
};

static const int DEFAULT_INTERVALS[NUM_TASKS] = {5, 10, 15, 10};

/* ============================================================================
 *                         SYSTEM METRICS FUNCTIONS
 * ============================================================================ */

/*
 * get_cpu_usage()
 * Reads /proc/stat to calculate current CPU usage percentage.
 * Uses delta between readings for accurate measurement.
 */
static double get_cpu_usage(void) {
    static long prev_idle = 0;
    static long prev_total = 0;
    
    FILE* fp = fopen("/proc/stat", "r");
    if (!fp) return 0.0;
    
    char buffer[256];
    if (fgets(buffer, sizeof(buffer), fp) == NULL) {
        fclose(fp);
        return 0.0;
    }
    fclose(fp);
    
    long user, nice, system, idle, iowait, irq, softirq;
    if (sscanf(buffer, "cpu %ld %ld %ld %ld %ld %ld %ld",
               &user, &nice, &system, &idle, &iowait, &irq, &softirq) != 7) {
        return 0.0;
    }
    
    long total = user + nice + system + idle + iowait + irq + softirq;
    long total_idle = idle + iowait;
    
    /* First reading - no delta available */
    if (prev_total == 0) {
        prev_idle = total_idle;
        prev_total = total;
        return 0.0;
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
 * Reads /proc/meminfo to calculate RAM usage percentage.
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
 * Uses statvfs() to get root filesystem usage percentage.
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
 * Reads /proc/net/dev to get network packet counts.
 */
static long g_prev_packet_count = 0;
static time_t g_prev_packet_time = 0;

static int get_network_packets(void) {
    FILE* fp = fopen("/proc/net/dev", "r");
    if (!fp) return 0;
    
    char line[256];
    long total_packets = 0;
    
    /* Skip header lines */
    if (fgets(line, sizeof(line), fp) == NULL) { fclose(fp); return 0; }
    if (fgets(line, sizeof(line), fp) == NULL) { fclose(fp); return 0; }
    
    while (fgets(line, sizeof(line), fp)) {
        /* Skip loopback interface */
        if (strstr(line, "lo:")) continue;
        
        char iface[32];
        long rx_packets, tx_packets;
        if (sscanf(line, "%[^:]: %*d %ld %*d %*d %*d %*d %*d %*d %*d %ld",
                   iface, &rx_packets, &tx_packets) >= 2) {
            total_packets += (rx_packets + tx_packets);
        }
    }
    fclose(fp);
    
    /* Calculate packets per second */
    time_t now = time(NULL);
    int packets_per_sec = 0;
    
    if (g_prev_packet_time > 0) {
        long time_delta = (long)(now - g_prev_packet_time);
        if (time_delta > 0) {
            long packet_delta = total_packets - g_prev_packet_count;
            packets_per_sec = (int)(packet_delta / time_delta);
        }
    }
    
    g_prev_packet_count = total_packets;
    g_prev_packet_time = now;
    
    return packets_per_sec;
}

/* ============================================================================
 *                       CONFIGURATION MANAGEMENT
 * ============================================================================ */

/*
 * load_task_config()
 * Loads task priorities and intervals from config files.
 */
static void load_task_config(TaskRow tasks[], int count) {
    /* Initialize defaults */
    for (int i = 0; i < count; i++) {
        tasks[i].id = i + 1;
        strncpy(tasks[i].name, TASK_NAMES[i], sizeof(tasks[i].name) - 1);
        tasks[i].name[sizeof(tasks[i].name) - 1] = '\0';
        tasks[i].priority = i;  /* Default: task 1 has priority 0 (highest) */
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
 * Updates visual indicators for task starvation status.
 */
static void update_starvation_indicators(AppWidgets* widgets) {
    for (int i = 0; i < NUM_TASKS; i++) {
        if (widgets->tasks[i].status_label == NULL) continue;
        
        /* Simulate last_run increment (in real system, read from scheduler) */
        widgets->tasks[i].last_run++;
        
        /* Reset on simulated execution based on priority */
        if (widgets->update_count % (widgets->tasks[i].interval + 
            widgets->tasks[i].priority * 2) == 0) {
            widgets->tasks[i].last_run = 0;
        }
        
        /* Update status label */
        if (widgets->tasks[i].last_run >= STARVATION_THRESHOLD) {
            gtk_label_set_markup(GTK_LABEL(widgets->tasks[i].status_label),
                "<span foreground='#ef4444'>⚠ STARVING</span>");
        } else if (widgets->tasks[i].last_run >= STARVATION_THRESHOLD / 2) {
            gtk_label_set_markup(GTK_LABEL(widgets->tasks[i].status_label),
                "<span foreground='#f59e0b'>○ At Risk</span>");
        } else {
            gtk_label_set_markup(GTK_LABEL(widgets->tasks[i].status_label),
                "<span foreground='#22c55e'>● OK</span>");
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
static const char* get_status_color(double value, double warn, double crit) {
    if (value >= crit) return "#ef4444";  /* Red */
    if (value >= warn) return "#f59e0b";  /* Yellow */
    return "#22c55e";                      /* Green */
}

/*
 * update_display()
 * Timer callback to refresh all GUI elements with current system metrics.
 */
static gboolean update_display(gpointer data) {
    AppWidgets* widgets = (AppWidgets*)data;
    widgets->update_count++;
    
    /* Get current system metrics */
    double cpu = get_cpu_usage();
    double ram = get_ram_usage();
    double disk = get_disk_usage();
    int packets = get_network_packets();
    
    /* Update CPU */
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(widgets->cpu_progress), cpu / 100.0);
    gchar* cpu_text = g_strdup_printf("%.1f%%", cpu);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(widgets->cpu_progress), cpu_text);
    g_free(cpu_text);
    
    const char* cpu_color = get_status_color(cpu, CPU_WARNING_THRESHOLD, CPU_CRITICAL_THRESHOLD);
    gchar* cpu_status = g_strdup_printf("<span foreground='%s'>●</span>", cpu_color);
    gtk_label_set_markup(GTK_LABEL(widgets->cpu_status), cpu_status);
    g_free(cpu_status);
    
    /* Update RAM */
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(widgets->ram_progress), ram / 100.0);
    gchar* ram_text = g_strdup_printf("%.1f%%", ram);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(widgets->ram_progress), ram_text);
    g_free(ram_text);
    
    const char* ram_color = get_status_color(ram, RAM_WARNING_THRESHOLD, RAM_CRITICAL_THRESHOLD);
    gchar* ram_status = g_strdup_printf("<span foreground='%s'>●</span>", ram_color);
    gtk_label_set_markup(GTK_LABEL(widgets->ram_status), ram_status);
    g_free(ram_status);
    
    /* Update Disk */
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(widgets->disk_progress), disk / 100.0);
    gchar* disk_text = g_strdup_printf("%.1f%%", disk);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(widgets->disk_progress), disk_text);
    g_free(disk_text);
    
    const char* disk_color = get_status_color(disk, DISK_WARNING_THRESHOLD, DISK_CRITICAL_THRESHOLD);
    gchar* disk_status = g_strdup_printf("<span foreground='%s'>●</span>", disk_color);
    gtk_label_set_markup(GTK_LABEL(widgets->disk_status), disk_status);
    g_free(disk_status);
    
    /* Update Network */
    gchar* net_text = g_strdup_printf("📶 %d packets/sec", packets);
    gtk_label_set_text(GTK_LABEL(widgets->network_label), net_text);
    g_free(net_text);
    
    /* Update starvation indicators */
    update_starvation_indicators(widgets);
    
    /* Update task info display */
    gchar* task_info = g_strdup_printf(
        "╔══════════════════════════════════════════════════════════╗\n"
        "║  SCHEDULER TASK STATUS                                   ║\n"
        "╠══════════════════════════════════════════════════════════╣\n"
        "║  ID │ Task Name          │ Priority │ Interval │ Status ║\n"
        "╠══════════════════════════════════════════════════════════╣\n"
        "║  1  │ %-18s │    P%d    │   %3ds   │  %s   ║\n"
        "║  2  │ %-18s │    P%d    │   %3ds   │  %s   ║\n"
        "║  3  │ %-18s │    P%d    │   %3ds   │  %s   ║\n"
        "║  4  │ %-18s │    P%d    │   %3ds   │  %s   ║\n"
        "╚══════════════════════════════════════════════════════════╝",
        widgets->tasks[0].name, widgets->tasks[0].priority, widgets->tasks[0].interval,
        widgets->tasks[0].last_run < STARVATION_THRESHOLD/2 ? "OK" : "⚠",
        widgets->tasks[1].name, widgets->tasks[1].priority, widgets->tasks[1].interval,
        widgets->tasks[1].last_run < STARVATION_THRESHOLD/2 ? "OK" : "⚠",
        widgets->tasks[2].name, widgets->tasks[2].priority, widgets->tasks[2].interval,
        widgets->tasks[2].last_run < STARVATION_THRESHOLD/2 ? "OK" : "⚠",
        widgets->tasks[3].name, widgets->tasks[3].priority, widgets->tasks[3].interval,
        widgets->tasks[3].last_run < STARVATION_THRESHOLD/2 ? "OK" : "⚠"
    );
    gtk_text_buffer_set_text(widgets->tasks_buffer, task_info, -1);
    g_free(task_info);
    
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
    GtkWidget* cpu_label = gtk_label_new("CPU Usage");
    gtk_widget_set_halign(cpu_label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), cpu_label, 0, 0, 1, 1);
    
    widgets->cpu_progress = gtk_progress_bar_new();
    gtk_widget_set_name(widgets->cpu_progress, "cpu-bar");
    gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(widgets->cpu_progress), TRUE);
    gtk_widget_set_hexpand(widgets->cpu_progress, TRUE);
    gtk_grid_attach(GTK_GRID(grid), widgets->cpu_progress, 1, 0, 1, 1);
    
    widgets->cpu_status = gtk_label_new("●");
    gtk_grid_attach(GTK_GRID(grid), widgets->cpu_status, 2, 0, 1, 1);
    
    /* RAM Row */
    GtkWidget* ram_label = gtk_label_new("RAM Usage");
    gtk_widget_set_halign(ram_label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), ram_label, 0, 1, 1, 1);
    
    widgets->ram_progress = gtk_progress_bar_new();
    gtk_widget_set_name(widgets->ram_progress, "ram-bar");
    gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(widgets->ram_progress), TRUE);
    gtk_widget_set_hexpand(widgets->ram_progress, TRUE);
    gtk_grid_attach(GTK_GRID(grid), widgets->ram_progress, 1, 1, 1, 1);
    
    widgets->ram_status = gtk_label_new("●");
    gtk_grid_attach(GTK_GRID(grid), widgets->ram_status, 2, 1, 1, 1);
    
    /* Disk Row */
    GtkWidget* disk_label = gtk_label_new("Disk Usage");
    gtk_widget_set_halign(disk_label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), disk_label, 0, 2, 1, 1);
    
    widgets->disk_progress = gtk_progress_bar_new();
    gtk_widget_set_name(widgets->disk_progress, "disk-bar");
    gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(widgets->disk_progress), TRUE);
    gtk_widget_set_hexpand(widgets->disk_progress, TRUE);
    gtk_grid_attach(GTK_GRID(grid), widgets->disk_progress, 1, 2, 1, 1);
    
    widgets->disk_status = gtk_label_new("●");
    gtk_grid_attach(GTK_GRID(grid), widgets->disk_status, 2, 2, 1, 1);
    
    /* Network Row */
    GtkWidget* net_label = gtk_label_new("Network");
    gtk_widget_set_halign(net_label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), net_label, 0, 3, 1, 1);
    
    widgets->network_label = gtk_label_new("📶 0 packets/sec");
    gtk_widget_set_halign(widgets->network_label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), widgets->network_label, 1, 3, 2, 1);
    
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
    
    GtkWidget* scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scroll), 180);
    gtk_container_add(GTK_CONTAINER(frame), scroll);
    
    widgets->tasks_textview = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(widgets->tasks_textview), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(widgets->tasks_textview), TRUE);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(widgets->tasks_textview), 8);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(widgets->tasks_textview), 8);
    widgets->tasks_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(widgets->tasks_textview));
    gtk_container_add(GTK_CONTAINER(scroll), widgets->tasks_textview);
    
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
    
    /* Metrics section */
    GtkWidget* metrics = create_metrics_section(widgets);
    gtk_box_pack_start(GTK_BOX(main_box), metrics, FALSE, FALSE, 0);
    
    /* Scheduler control section */
    GtkWidget* scheduler = create_scheduler_section(widgets);
    gtk_box_pack_start(GTK_BOX(main_box), scheduler, FALSE, FALSE, 0);
    
    /* Task info section */
    GtkWidget* task_info = create_task_info_section(widgets);
    gtk_box_pack_start(GTK_BOX(main_box), task_info, TRUE, TRUE, 0);
    
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
