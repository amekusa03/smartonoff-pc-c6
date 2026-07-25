#!/bin/bash

# --- Config ---
PORT=7777
TOKEN="secure_pc_shutdown_token_12345"
# --------------

echo "Shutdown daemon (netcat-based) listening on port $PORT..."

while true; do
    # Receive client data using nc with a 5 second timeout
    # standard openbsd netcat uses '-l' with host/port parameters
    REQUEST=$(timeout 5 nc -l -p $PORT)
    
    # Strip any whitespace/newlines
    CLEAN_REQUEST=$(echo "$REQUEST" | tr -d '\r\n[:space:]')
    
    if [ "$CLEAN_REQUEST" = "$TOKEN" ]; then
        echo "Valid token received! Executing shutdown..."
        sudo shutdown -h now
        exit 0
    elif [ -n "$CLEAN_REQUEST" ]; then
        echo "Invalid token received: '$CLEAN_REQUEST'"
    fi
done
