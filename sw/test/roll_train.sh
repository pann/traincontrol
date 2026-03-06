#!/bin/bash
./modpoll -r1 -c1 -t4 192.168.100.56 0 > /dev/null

#!/bin/bash

# Validate argument count
if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <dir: 0|1> <speed: 0-100>"
    exit 1
fi

DIR="$1"
SPEED="$2"
IP="${3:-192.168.100.56}"   # Default IP if not provided


# Validate DIR (must be 0 or 1)
if ! [[ "$DIR" =~ ^[01]$ ]]; then
    echo "Error: dir must be 0 or 1"
    exit 1
fi

# Validate SPEED (number 0-1000)
if ! [[ "$SPEED" =~ ^[0-9]+$ ]] || [ "$SPEED" -lt 0 ] || [ "$SPEED" -gt 1000 ]; then
    echo "Error: speed must be a number between 0 and 100"
    exit 1
fi

# Print direction
if [ "$DIR" -eq 1 ]; then
    echo "Setting reverse motion"
else
    echo "Setting forward motion"
fi

echo "Setting speed to $SPEED%"
SPEED=$((SPEED * 10)) # speed is sent as 0-1000

# Function to handle Ctrl+C
cleanup() {
    echo "Stopping..."
    until ./modpoll -o 0.5 -r1 -c1 -t0 192.168.100.56 0 > /dev/null; do
    	echo "Failed to stop train, retrying"
    	sleep 0.5
    done
    
    exit 0
}

trap cleanup SIGINT

# Run command
./modpoll -r1 -c1 -t4 $IP $DIR > /dev/null
./modpoll -r2 -c1 -t4 $IP $SPEED > /dev/null
./modpoll -o 0.5 -r1 -c1 -t0 $IP 1 > /dev/null

# Wait forever until Ctrl+C
while true; do
    sleep 0.5
done
