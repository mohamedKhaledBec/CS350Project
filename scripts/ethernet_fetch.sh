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

# Get packet counts from /proc/net/dev (always available, no sudo needed)
total_rx=0
total_tx=0

while read -r line; do
    # Skip header lines and loopback
    if [[ "$line" == *"Inter-"* ]] || [[ "$line" == *"face"* ]] || [[ "$line" == *"lo:"* ]]; then
        continue
    fi
    # Parse: interface: rx_bytes rx_packets ... tx_bytes tx_packets
    if [[ "$line" =~ ^[[:space:]]*([^:]+):[[:space:]]*([0-9]+)[[:space:]]+([0-9]+) ]]; then
        rx_packets=$(echo "$line" | awk '{print $3}')
        tx_packets=$(echo "$line" | awk '{print $11}')
        total_rx=$((total_rx + rx_packets))
        total_tx=$((total_tx + tx_packets))
    fi
done < /proc/net/dev

total_packets=$((total_rx + total_tx))

# Save metrics in parseable format
{
    echo "=== NETWORK METRICS ==="
    echo "TIMESTAMP=$(date '+%Y-%m-%d %H:%M:%S')"
    echo "RX_PACKETS=$total_rx"
    echo "TX_PACKETS=$total_tx"
    echo "TOTAL_PACKETS=$total_packets"
} > "$PROJECT_DIR/logs/network_metrics.log"

echo "RX Packets: $total_rx"
echo "TX Packets: $total_tx"
echo "Total: $total_packets"

# Also save detailed interface stats
ip -s link 2>/dev/null > "$PROJECT_DIR/logs/netdump.log"
echo "Network data captured at $(date)" >> "$PROJECT_DIR/data/ethernet_data.txt"

echo "Network fetch completed."