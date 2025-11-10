#!/bin/bash

LOG_FILE="/tmp/test_app.log"
FAST_MODE=false

if [ "$1" == "--fast" ]; then
    FAST_MODE=true
    echo "Starting log generator in FAST mode. Writing to $LOG_FILE"
else
    echo "Starting log generator. Writing to $LOG_FILE"
fi

# Clean up previous log file
rm -f $LOG_FILE

# Trap SIGTERM and exit
trap 'echo "Log generator stopped."; exit 0' SIGTERM

i=0
while true; do
    echo "$(date '+%Y-%m-%d %H:%M:%S') - INFO - Log entry $i - This is a test log message from the generator." >> $LOG_FILE
    i=$((i+1))
    
    if [ "$FAST_MODE" = false ]; then
        sleep 0.1 # Adjust sleep to control log generation rate
    fi
done
