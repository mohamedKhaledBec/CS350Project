#!/bin/bash

# Get the script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

# Create logs directory if it doesn't exist
mkdir -p "$PROJECT_DIR/logs"

echo "=== SYSTEM MONITOR ==="
echo "Running system report at $(date)"

# Get CPU usage (handle cases where top might fail)
cpu_usage=$(top -bn1 2>/dev/null | grep "Cpu(s)" | awk '{print 100 - $8}' || echo "0")

# Get RAM usage
ram_usage=$(free 2>/dev/null | awk '/Mem/{printf("%.2f"), $3/$2 * 100}' || echo "0")

# Get Disk usage
disk_usage=$(df --total 2>/dev/null | tail -1 | awk '{print $5}' | tr -d '%' || echo "0")

echo "CPU: $cpu_usage%"
echo "RAM: $ram_usage%"
echo "Disk: $disk_usage%"
echo "Report generated at $(date)" >> "$PROJECT_DIR/logs/system_report.log"
echo "System monitoring complete."
