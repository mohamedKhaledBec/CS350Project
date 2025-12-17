#!/bin/bash
# Script to fetch ethernet/network data and write to data/ethernet_data.txt

# Get the script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

echo "Project directory: $PROJECT_DIR"

# Create data directory if it doesn't exist
mkdir -p "$PROJECT_DIR/data"
mkdir -p "$PROJECT_DIR/logs"

# Install tcpdump if not available
if ! command -v tcpdump &> /dev/null; then
    echo "Installing tcpdump..."
    sudo apt-get update && sudo apt-get install -y tcpdump
fi

echo "Fetching network data at $(date)"

# Capture network packets
if command -v tcpdump &> /dev/null; then
    echo "Running tcpdump for 5 seconds..."
    sudo timeout 5 tcpdump -n -i any -c 10 2>&1 | tee "$PROJECT_DIR/logs/netdump.log"
    echo ""
    echo "Network data captured at $(date)" | tee -a "$PROJECT_DIR/data/ethernet_data.txt"
    cat "$PROJECT_DIR/logs/netdump.log" >> "$PROJECT_DIR/data/ethernet_data.txt"
    echo "Data saved to: $PROJECT_DIR/data/ethernet_data.txt"
else
    echo "tcpdump not available. Skipping network capture."
fi

echo "Network fetch completed."