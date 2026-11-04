#!/bin/sh

TEMP_FILE="/sys/class/thermal/thermal_zone0/temp"

while true; do
    temp_raw=$(cat "$TEMP_FILE")
    temp_c=$((temp_raw / 1000))
    timestamp=$(date '+%Y-%m-%d %H:%M:%S')
    echo "[$timestamp] cpu tmp: ${temp_c}��C"
    sleep 5
done