#!/bin/bash

# Get the script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

# Create logs directory if it doesn't exist
mkdir -p "$PROJECT_DIR/logs"

echo "=== SYSTEM MONITOR ==="
echo "Running system report at $(date)"

# Get CPU usage from /proc/stat (more reliable than top)
# Read CPU stats twice with a small delay to calculate usage
read_cpu_stats() {
    awk '/^cpu / {print $2+$3+$4+$5+$6+$7+$8, $5}' /proc/stat
}

cpu_stats1=$(read_cpu_stats)
sleep 0.5
cpu_stats2=$(read_cpu_stats)

total1=$(echo "$cpu_stats1" | awk '{print $1}')
idle1=$(echo "$cpu_stats1" | awk '{print $2}')
total2=$(echo "$cpu_stats2" | awk '{print $1}')
idle2=$(echo "$cpu_stats2" | awk '{print $2}')

total_diff=$((total2 - total1))
idle_diff=$((idle2 - idle1))

if [ "$total_diff" -gt 0 ]; then
    cpu_usage=$(awk "BEGIN {printf \"%.1f\", 100 * ($total_diff - $idle_diff) / $total_diff}")
else
    cpu_usage="0.0"
fi

# Get RAM usage
ram_usage=$(free 2>/dev/null | awk '/Mem/{printf("%.1f"), $3/$2 * 100}' || echo "0")

# Get Disk usage
disk_usage=$(df --total 2>/dev/null | tail -1 | awk '{print $5}' | tr -d '%' || echo "0")

echo "CPU: $cpu_usage%"
echo "RAM: $ram_usage%"
echo "Disk: $disk_usage%"

# Save metrics to log file in parseable format
{
    echo "=== SYSTEM METRICS ==="
    echo "TIMESTAMP=$(date '+%Y-%m-%d %H:%M:%S')"
    echo "CPU=$cpu_usage"
    echo "RAM=$ram_usage"
    echo "DISK=$disk_usage"
} > "$PROJECT_DIR/logs/system_metrics.log"

echo "Report generated at $(date)" >> "$PROJECT_DIR/logs/system_report.log"
echo "System monitoring complete."
