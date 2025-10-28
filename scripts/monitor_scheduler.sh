#!/bin/bash

while true; do
    echo "Running system report at $(date)"

    cpu_usage=$(top -bn1 | grep "Cpu(s)" | awk '{print 100 - $8}')
    ram_usage=$(free | awk '/Mem/{printf("%.2f"), $3/$2 * 100}')
    disk_usage=$(df --total | tail -1 | awk '{print $5}' | tr -d '%')

    # Capture 5-second network sample
    sudo timeout 5 tcpdump -n -i any -c 10 > logs/netdump.log 2>/dev/null

    ./build/analyze "$cpu_usage" "$ram_usage" "$disk_usage" "logs/netdump.log"

    echo "Report generated. Next check in 5 minutes."
    sleep 300
done
