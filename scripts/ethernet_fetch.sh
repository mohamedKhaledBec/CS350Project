#!/bin/bash
# Script to fetch ethernet/network data and write to data/ethernet_data.txt

# Get the script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

echo "Project directory: $PROJECT_DIR"

# Create data directory if it doesn't exist
mkdir -p "$PROJECT_DIR/data"
mkdir -p "$PROJECT_DIR/logs"

# Capture network packets without prompting for sudo install
echo "Fetching network data at $(date)"

if command -v tcpdump >/dev/null 2>&1 && sudo -n true >/dev/null 2>&1; then
    echo "Running tcpdump for 5 seconds..."
    sudo timeout 5 tcpdump -n -i any -c 10 2>/dev/null | tee "$PROJECT_DIR/logs/netdump.log"
    echo "Network data captured at $(date)" | tee -a "$PROJECT_DIR/data/ethernet_data.txt"
    cat "$PROJECT_DIR/logs/netdump.log" >> "$PROJECT_DIR/data/ethernet_data.txt"
    echo "Data saved to: $PROJECT_DIR/data/ethernet_data.txt"
else
    echo "tcpdump unavailable or sudo not cached; writing interface stats instead." | tee "$PROJECT_DIR/logs/netdump.log"
    ip -s link 2>/dev/null | tee -a "$PROJECT_DIR/logs/netdump.log"
    echo "Interface stats captured at $(date)" | tee -a "$PROJECT_DIR/data/ethernet_data.txt"
    cat "$PROJECT_DIR/logs/netdump.log" >> "$PROJECT_DIR/data/ethernet_data.txt"
fi

echo "Network fetch completed."