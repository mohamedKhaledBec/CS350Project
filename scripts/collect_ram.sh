#!/bin/bash
# RAM Monitoring Script

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LOG_DIR="$(dirname "$SCRIPT_DIR")/logs"
RAM_LOG="$LOG_DIR/ram_log.txt"
THRESHOLD=80

mkdir -p "$LOG_DIR"
TIMESTAMP=$(date '+%Y-%m-%d %H:%M:%S')

RAM_USAGE=$(free | grep Mem | awk '{printf("%.2f"), ($3/$2) * 100.0}')
RAM_TOTAL=$(free -h | grep Mem | awk '{print $2}')
RAM_USED=$(free -h | grep Mem | awk '{print $3}')

echo "[$TIMESTAMP] RAM Usage: ${RAM_USAGE}% (Used: $RAM_USED / Total: $RAM_TOTAL)" >> "$RAM_LOG"
