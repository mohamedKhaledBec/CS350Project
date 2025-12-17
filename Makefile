CC = gcc
CFLAGS = -Wall -Wextra -pthread
BUILD_DIR = build

all: $(BUILD_DIR) scheduler analyze

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

report_generator: src/report_generator.c | $(BUILD_DIR)
	@echo "Warning: report_generator.c is empty or has no main function"
	@echo "Skipping report_generator build"

scheduler: src/scheduler.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) src/scheduler.c -o $(BUILD_DIR)/scheduler

analyze: src/analyze.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) src/analyze.c -o $(BUILD_DIR)/analyze

clean:
	rm -rf $(BUILD_DIR)

monitor:
	cat scripts/monitor_scheduler.sh

.PHONY: all clean report_generator scheduler analyze monitor
