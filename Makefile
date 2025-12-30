# ==============================================================================
# CS350 Real-Time System Monitoring Project - Build System
# ==============================================================================
#
# A preemptive priority scheduler with live system monitoring for Linux.
# Features real-time metrics, GTK3 GUI, and starvation detection.
#
# Main Targets:
#   make all           - Build all core components
#   make gui           - Build and launch GTK3 GUI
#   make run           - Launch scheduler with monitoring
#   make clean         - Remove build artifacts
#   make help          - Show all available targets
#
# ==============================================================================

# ==============================================================================
#                             COMPILER SETTINGS
# ==============================================================================

CC              = gcc
CFLAGS_BASE     = -Wall -Wextra -Wpedantic -std=c11 -pthread -D_GNU_SOURCE
CFLAGS_DEBUG    = $(CFLAGS_BASE) -g -O0 -DDEBUG
CFLAGS_RELEASE  = $(CFLAGS_BASE) -O2

# Default to release build
CFLAGS          = $(CFLAGS_RELEASE)

# GTK3 flags (only used for GUI target)
GTK_FLAGS       = $(shell pkg-config --cflags --libs gtk+-3.0 2>/dev/null)

# ==============================================================================
#                             DIRECTORY SETTINGS
# ==============================================================================

BUILD_DIR       = build
LOGS_DIR        = logs
CONFIG_DIR      = config
SRC_DIR         = src

# ==============================================================================
#                             FILE SETTINGS
# ==============================================================================

# Source files
SRC_SCHEDULER   = $(SRC_DIR)/scheduler.c
SRC_ANALYZE     = $(SRC_DIR)/analyze.c
SRC_REPORT      = $(SRC_DIR)/report_generator.c
SRC_METRICS     = $(SRC_DIR)/metrics_exporter.c
SRC_GTK_GUI     = $(SRC_DIR)/gtk_gui.c

# Output executables
OUT_SCHEDULER   = $(BUILD_DIR)/scheduler
OUT_ANALYZE     = $(BUILD_DIR)/analyze
OUT_REPORT      = $(BUILD_DIR)/report_generator
OUT_METRICS     = $(BUILD_DIR)/metrics_exporter
OUT_GTK_GUI     = $(BUILD_DIR)/gtk_gui

# ==============================================================================
#                             PLATFORM COMMANDS
# ==============================================================================

RM              = rm -rf
MKDIR           = mkdir -p

# ==============================================================================
#                             BUILD TARGETS
# ==============================================================================

.PHONY: all clean clean-logs reset help debug gui run setup check-gtk

# Default target: build all core components (excluding GTK which needs libs)
all: setup $(OUT_SCHEDULER) $(OUT_ANALYZE) $(OUT_REPORT) $(OUT_METRICS)
	@echo ""
	@echo "╔═══════════════════════════════════════════════════════════════╗"
	@echo "║  BUILD COMPLETE                                               ║"
	@echo "╚═══════════════════════════════════════════════════════════════╝"
	@echo ""
	@echo "  Core components built successfully."
	@echo "  Run 'make gui' to launch the system monitor."
	@echo ""

# Create required directories
setup:
	@$(MKDIR) $(BUILD_DIR)
	@$(MKDIR) $(LOGS_DIR)
	@$(MKDIR) $(CONFIG_DIR)

# ==============================================================================
#                          COMPONENT BUILD RULES
# ==============================================================================

# Core scheduler with starvation detection
$(OUT_SCHEDULER): $(SRC_SCHEDULER) | setup
	@echo "  [CC] scheduler.c"
	@$(CC) $(CFLAGS) $< -o $@

# System metrics analyzer
$(OUT_ANALYZE): $(SRC_ANALYZE) | setup
	@echo "  [CC] analyze.c"
	@$(CC) $(CFLAGS) $< -o $@

# HTML report generator
$(OUT_REPORT): $(SRC_REPORT) | setup
	@echo "  [CC] report_generator.c"
	@$(CC) $(CFLAGS) $< -o $@

# JSON metrics exporter for web UI
$(OUT_METRICS): $(SRC_METRICS) | setup
	@echo "  [CC] metrics_exporter.c"
	@$(CC) $(CFLAGS) $< -o $@

# GTK3 GUI (requires gtk+-3.0 development libraries)
$(OUT_GTK_GUI): $(SRC_GTK_GUI) | setup check-gtk
	@echo "  [CC] gtk_gui.c (with GTK3)"
	@$(CC) $(CFLAGS) $< -o $@ $(GTK_FLAGS)

# Check if GTK3 is available
check-gtk:
	@pkg-config --exists gtk+-3.0 2>/dev/null || \
		(echo "ERROR: GTK3 not found. Install with: sudo apt install libgtk-3-dev" && exit 1)

# ==============================================================================
#                           CONVENIENCE TARGETS
# ==============================================================================

# Build and launch GTK GUI
gtk_gui: $(OUT_GTK_GUI)
	@echo "Launching GTK3 System Monitor..."
	@$(OUT_GTK_GUI)

# Alias for gtk_gui
gui: gtk_gui

# Run analyzer with test data
analyze: $(OUT_ANALYZE)
	@$(OUT_ANALYZE) 45.2 62.8 58.3 $(LOGS_DIR)/net_log.txt

# Generate HTML report
report: $(OUT_REPORT)
	@$(OUT_REPORT) $(LOGS_DIR)/system_report.html

# Export metrics to JSON
export-metrics: $(OUT_METRICS)
	@$(OUT_METRICS) ui/metrics.json

# ==============================================================================
#                           DEBUG BUILD TARGET
# ==============================================================================

debug: CFLAGS = $(CFLAGS_DEBUG)
debug: clean all
	@echo "Debug build complete (symbols enabled, no optimization)"

# ==============================================================================
#                           CLEANUP TARGETS
# ==============================================================================

clean:
	@echo "Cleaning build directory..."
	@$(RM) $(BUILD_DIR)

clean-logs:
	@echo "Cleaning log files..."
	@$(RM) $(LOGS_DIR)/*.log $(LOGS_DIR)/*.html ui/metrics.json

reset: clean clean-logs
	@echo "Full reset complete"

# ==============================================================================
#                          LOG MANAGEMENT TARGETS
# ==============================================================================

# Archive logs that exceed size threshold
archive-logs:
	@echo "Archiving logs..."
	@chmod +x scripts/archive_logs.sh
	@bash scripts/archive_logs.sh

# Force archive all logs regardless of size
archive-all:
	@echo "Force archiving all logs..."
	@chmod +x scripts/archive_logs.sh
	@bash scripts/archive_logs.sh --force

# View log summary
view-logs:
	@chmod +x scripts/view_logs.sh
	@bash scripts/view_logs.sh all

# View specific log (usage: make view-log LOG=scheduler)
view-log:
	@chmod +x scripts/view_logs.sh
	@bash scripts/view_logs.sh $(LOG) -n 30

# List archived logs
list-archives:
	@chmod +x scripts/view_logs.sh
	@bash scripts/view_logs.sh --archive

# ==============================================================================
#                             HELP TARGET
# ==============================================================================

help:
	@echo ""
	@echo "╔═══════════════════════════════════════════════════════════════╗"
	@echo "║  CS350 REAL-TIME SYSTEM MONITOR - BUILD SYSTEM                ║"
	@echo "╚═══════════════════════════════════════════════════════════════╝"
	@echo ""
	@echo "  BUILD TARGETS:"
	@echo "    make all          Build all core components"
	@echo "    make gui          Build and launch GTK3 system monitor"
	@echo "    make debug        Build with debug symbols"
	@echo "    make setup        Create required directories"
	@echo ""
	@echo "  LOG MANAGEMENT:"
	@echo "    make archive-logs   Archive logs exceeding size threshold"
	@echo "    make archive-all    Force archive all logs"
	@echo "    make view-logs      View summary of all logs"
	@echo "    make view-log LOG=X View specific log (scheduler/metrics/etc)"
	@echo "    make list-archives  List all archived log files"
	@echo ""
	@echo "  CLEANUP:"
	@echo "    make clean        Remove build artifacts"
	@echo "    make clean-logs   Remove all log files"
	@echo "    make reset        Full cleanup (build + logs)"
	@echo ""
	@echo "  REQUIREMENTS:"
	@echo "    - GCC with C11 support"
	@echo "    - Linux with /proc filesystem"
	@echo "    - GTK3 development libraries (for GUI)"
	@echo "      Install: sudo apt install libgtk-3-dev"
	@echo ""
