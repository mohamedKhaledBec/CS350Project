CC = gcc
CFLAGS = -Wall -Wextra -pthread
BUILD_DIR = build

# Detect OS for proper commands
ifeq ($(OS),Windows_NT)
    RM = if exist $(BUILD_DIR) rmdir /s /q $(BUILD_DIR)
    RM_FILES = if exist logs\*.log del /q logs\*.log && if exist logs\*.html del /q logs\*.html && if exist ui\metrics.json del /q ui\metrics.json
    MKDIR = if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)
else
    RM = rm -rf $(BUILD_DIR)
    RM_FILES = rm -f logs/*.log logs/*.html ui/metrics.json
    MKDIR = mkdir -p $(BUILD_DIR)
endif

all: $(BUILD_DIR) scheduler analyze report_generator metrics_exporter terminal_ui ncurses_gui gtk_gui

$(BUILD_DIR):
	$(MKDIR)

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

monitor:
	cat scripts/monitor_scheduler.sh

run: scheduler
	cd build && ./scheduler

ui: terminal_ui
	@echo Starting Terminal UI (Live)...
	$(BUILD_DIR)/terminal_ui

gui: gtk_live
	@echo Starting Real-Time Monitor GUI...
	$(BUILD_DIR)/gtk_live

launch: all
	@echo "Starting monitor with UI..."
	@chmod +x launch_with_ui.sh
	@./launch_with_ui.sh

export-metrics: metrics_exporter
	$(BUILD_DIR)/metrics_exporter ui/metrics.json

generate-report: report_generator
	$(BUILD_DIR)/report_generator logs/system_report.html
	@echo HTML report generated at logs/system_report.html

.PHONY: all clean clean-logs report_generator scheduler analyze monitor run ui gui export-metrics generate-report metrics_exporter terminal_ui gtk_live launch
