#!/bin/bash
# Disk Monitoring Script

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LOG_DIR="$(dirname "$SCRIPT_DIR")/logs"
DISK_LOG="$LOG_DIR/disk_log.txt"

mkdir -p "$LOG_DIR"
TIMESTAMP=$(date '+%Y-%m-%d %H:%M:%S')

DISK_USAGE=$(df -h / | tail -1 | awk '{print $5}' | sed 's/%//')
DISK_AVAILABLE=$(df -h / | tail -1 | awk '{print $4}')
DISK_USED=$(df -h / | tail -1 | awk '{print $3}')
DISK_TOTAL=$(df -h / | tail -1 | awk '{print $2}')

echo "[$TIMESTAMP] Disk Usage: ${DISK_USAGE}% (Used: $DISK_USED / Total: $DISK_TOTAL / Available: $DISK_AVAILABLE)" >> "$DISK_LOG"
