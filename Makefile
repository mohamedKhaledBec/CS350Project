CC = gcc
CFLAGS = -Wall -Wextra -pthread
BUILD_DIR = build

all: $(BUILD_DIR) scheduler

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

report_generator: src/report_generator.c | $(BUILD_DIR)
	@echo "Warning: report_generator.c is empty or has no main function"
	@echo "Skipping report_generator build"

scheduler: src/scheduler.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) src/scheduler.c -o $(BUILD_DIR)/scheduler

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean report_generator scheduler
