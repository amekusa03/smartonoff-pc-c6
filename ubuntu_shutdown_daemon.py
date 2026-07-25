#!/usr/bin/env python3
import socket
import os
import sys
import subprocess

# --- Config ---
PORT = 7777
TOKEN = "secure_pc_shutdown_token_12345"
# --------------

def main():
    # Bind to all interfaces on the configured port
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    
    try:
        server.bind(('0.0.0.0', PORT))
        server.listen(5)
        print(f"Shutdown daemon listening on port {PORT}...")
    except Exception as e:
        print(f"Error binding to port {PORT}: {e}")
        sys.exit(1)

    while True:
        try:
            conn, addr = server.accept()
            print(f"Connection from {addr}")
            
            # Set timeout for receiving data (5 seconds)
            conn.settimeout(5.0)
            
            # Receive up to 1024 bytes
            data = conn.recv(1024)
            received_token = data.decode('utf-8', errors='ignore').strip()
            
            if received_token == TOKEN:
                print("Valid token received! Executing shutdown...")
                conn.sendall(b"ACK: Shutting down\n")
                conn.close()
                
                # Execute system shutdown command
                subprocess.run(["sudo", "shutdown", "-h", "now"])
                break
            else:
                print(f"Invalid token received: '{received_token}'")
                conn.sendall(b"ERR: Invalid token\n")
                conn.close()
                
        except socket.timeout:
            print("Connection timed out waiting for token.")
        except Exception as e:
            print(f"Error handling connection: {e}")

if __name__ == '__main__':
    # Ensure script is run with sudo or has shutdown privileges
    main()
