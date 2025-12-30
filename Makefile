# ==============================================================================
# CS350 Real-Time System Monitoring Project - Build System
# ==============================================================================
# A preemptive priority scheduler with live system monitoring for Linux
# 
# Main targets:
#   make all           - Build all components
#   make run           - Launch scheduler with UI selection menu
#   make gui           - Launch GTK live monitor directly
#   make ui            - Launch terminal UI directly
#   make clean         - Remove build artifacts
#   make help          - Show all available targets
# ==============================================================================

CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c11 -pthread -D_GNU_SOURCE
CFLAGS_DEBUG = $(CFLAGS) -g -O0 -DDEBUG
CFLAGS_RELEASE = $(CFLAGS) -O2

BUILD_DIR = build
LOGS_DIR = logs

# Default to release build
CFLAGS := $(CFLAGS_RELEASE)

# Linux-only tooling
RM = rm -rf $(BUILD_DIR)
RM_FILES = rm -f logs/*.log logs/*.html ui/metrics.json
MKDIR = mkdir -p $(BUILD_DIR)
MKDIR_LOGS = mkdir -p $(LOGS_DIR)

all: setup $(BUILD_DIR) scheduler analyze report_generator metrics_exporter terminal_ui
	@echo "Build complete. Optional: run 'make gtk_live' if GTK3 is installed."

$(BUILD_DIR):
	$(MKDIR)

setup:
	$(MKDIR_LOGS)

setup-sudo:
	@echo "Caching sudo credentials (you may be prompted)..."
	sudo -v

report_generator: src/report_generator.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) src/report_generator.c -o $(BUILD_DIR)/report_generator

metrics_exporter: src/metrics_exporter.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) src/metrics_exporter.c -o $(BUILD_DIR)/metrics_exporter

terminal_ui: src/terminal_ui.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) src/terminal_ui.c -o $(BUILD_DIR)/terminal_ui

gtk_live: src/gtk_live_gui.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) src/gtk_live_gui.c -o $(BUILD_DIR)/gtk_live `pkg-config --cflags --libs gtk+-3.0`

scheduler: src/scheduler.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) src/scheduler.c -o $(BUILD_DIR)/scheduler

analyze: src/analyze.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) src/analyze.c -o $(BUILD_DIR)/analyze

clean:
	$(RM)

clean-logs:
	$(RM_FILES)

reset: clean clean-logs
	@echo "Reset complete: build/ removed and logs cleared."

monitor:
	cat scripts/monitor_scheduler.sh

run: all
	@echo "Launching monitor with UI selection (debug console, GTK, or terminal UI)..."
	@bash scripts/launch_with_ui.sh

ui: terminal_ui
	@echo Starting Terminal UI (Live)...
	$(BUILD_DIR)/terminal_ui

gui: gtk_live
	@echo Starting Real-Time Monitor GUI...
	$(BUILD_DIR)/gtk_live

launch: all
	@echo "Starting monitor with UI..."
	@chmod +x scripts/launch_with_ui.sh
	@bash scripts/launch_with_ui.sh

export-metrics: metrics_exporter
	$(BUILD_DIR)/metrics_exporter ui/metrics.json

generate-report: report_generator
	$(BUILD_DIR)/report_generator logs/system_report.html
	@echo HTML report generated at logs/system_report.html

archive-loop:
	@chmod +x scripts/archive_loop.sh
	@bash scripts/archive_loop.sh

help:
	@echo ""
	@echo "CS350 Real-Time System Monitoring Project"
	@echo "=========================================="
	@echo ""
	@echo "Build Targets:"
	@echo "  make all            - Build all core components"
	@echo "  make debug          - Build with debug symbols"
	@echo "  make gtk_live       - Build GTK GUI (requires gtk+-3.0)"
	@echo ""
	@echo "Run Targets:"
	@echo "  make run            - Launch scheduler with UI menu"
	@echo "  make gui            - Launch GTK live monitor"
	@echo "  make ui             - Launch terminal UI"
	@echo ""
	@echo "Utility Targets:"
	@echo "  make export-metrics - Export metrics to JSON"
	@echo "  make generate-report- Generate HTML report"
	@echo "  make archive-loop   - Start log archiver"
	@echo "  make setup-sudo     - Pre-cache sudo credentials"
	@echo ""
	@echo "Cleanup Targets:"
	@echo "  make clean          - Remove build directory"
	@echo "  make clean-logs     - Remove generated logs"
	@echo "  make reset          - Full cleanup (build + logs)"
	@echo ""

# Debug build target
debug: CFLAGS := $(CFLAGS_DEBUG)
debug: clean all
	@echo "Debug build complete."

.PHONY: all clean clean-logs reset report_generator scheduler analyze monitor run ui gui export-metrics generate-report metrics_exporter terminal_ui gtk_live launch setup help setup-sudo archive-loop debug
