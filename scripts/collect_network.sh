#!/bin/bash
# Network Statistics Monitoring Script

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LOG_DIR="$(dirname "$SCRIPT_DIR")/logs"
NET_LOG="$LOG_DIR/net_log.txt"

mkdir -p "$LOG_DIR"
TIMESTAMP=$(date '+%Y-%m-%d %H:%M:%S')

INTERFACE=$(ip route | grep default | awk '{print $5}' | head -1)
if [ -z "$INTERFACE" ]; then
    INTERFACE=$(ls /sys/class/net | grep -v lo | head -1)
fi

RX_BYTES=$(cat /sys/class/net/"$INTERFACE"/statistics/rx_bytes 2>/dev/null || echo "0")
TX_BYTES=$(cat /sys/class/net/"$INTERFACE"/statistics/tx_bytes 2>/dev/null || echo "0")
RX_PACKETS=$(cat /sys/class/net/"$INTERFACE"/statistics/rx_packets 2>/dev/null || echo "0")
TX_PACKETS=$(cat /sys/class/net/"$INTERFACE"/statistics/tx_packets 2>/dev/null || echo "0")

RX_MB=$(echo "scale=2; $RX_BYTES / 1048576" | bc 2>/dev/null || echo "0")
TX_MB=$(echo "scale=2; $TX_BYTES / 1048576" | bc 2>/dev/null || echo "0")

echo "[$TIMESTAMP] Network Stats - Interface: $INTERFACE | RX Packets: $RX_PACKETS | TX Packets: $TX_PACKETS | RX: ${RX_MB}MB | TX: ${TX_MB}MB" >> "$NET_LOG"