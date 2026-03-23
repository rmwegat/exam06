#!/bin/zsh

# Variables
SERVER="./mini_serv"
PORT=12345
CLIENTS=50  # Number of clients to simulate
MESSAGES=50  # Number of messages each client sends
SERVER_LOG="server_stress.log"
CLIENT_LOG_PREFIX="client_stress"
NC_CMD="/usr/bin/nc"  # Only the command, without arguments

# Start the server
echo "Starting server on port $PORT..."
$SERVER $PORT > $SERVER_LOG 2>&1 &
SERVER_PID=$!
sleep 1

# Check if the server started successfully
if ! ps -p $SERVER_PID > /dev/null; then
    echo "Failed to start the server. Check $SERVER_LOG for details."
    exit 1
fi
echo "Server started with PID $SERVER_PID."

# Start clients and send messages
CLIENT_PIDS=()
for i in $(seq 1 $CLIENTS); do
    LOG_FILE="${CLIENT_LOG_PREFIX}_${i}.log"
    echo "Connecting client $i..."
    (
        for j in $(seq 1 $MESSAGES); do
            echo "Message $j from client $i" | $NC_CMD -N 127.0.0.1 $PORT
            sleep 0.1  # Small delay between messages
        done
    ) > $LOG_FILE 2>&1 &
    CLIENT_PIDS+=($!)
    sleep 0.05  # Stagger client connections slightly
done

# Wait for all clients to finish
for PID in $CLIENT_PIDS; do
    wait $PID
done

# Stop the server
echo "Stopping the server..."
kill $SERVER_PID
wait $SERVER_PID 2>/dev/null

# Display logs
echo "Server log:"
cat $SERVER_LOG
echo

for i in $(seq 1 $CLIENTS); do
    LOG_FILE="${CLIENT_LOG_PREFIX}_${i}.log"
    echo "Client $i log:"
    cat $LOG_FILE
    echo
done

# Clean up
rm -f $SERVER_LOG ${CLIENT_LOG_PREFIX}_*.log
echo "Stress test completed."