#!/bin/bash
# CPU Monitoring Script

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LOG_DIR="$(dirname "$SCRIPT_DIR")/logs"
CPU_LOG="$LOG_DIR/cpu_log.txt"
THRESHOLD=80

mkdir -p "$LOG_DIR"
TIMESTAMP=$(date '+%Y-%m-%d %H:%M:%S')

# Get CPU usage (100 - idle%)
CPU_IDLE=$(top -bn2 -d 0.5 | grep "Cpu(s)" | tail -1 | sed "s/.*, *\([0-9.]*\) id.*/\1/")
if [ -z "$CPU_IDLE" ]; then
    CPU_IDLE=$(top -bn1 | grep "Cpu(s)" | sed "s/.*, *\([0-9.]*\) id.*/\1/")
fi

# If still empty, default to 0
if [ -z "$CPU_IDLE" ]; then
    CPU_IDLE=0
fi

CPU_USAGE=$(awk "BEGIN {printf \"%.1f\", 100 - $CPU_IDLE}")

echo "[$TIMESTAMP] CPU Usage: ${CPU_USAGE}%" >> "$CPU_LOG"
