#!/usr/bin/env python3

import socket
import sys
import argparse
from datetime import datetime

def create_udp_socket(ip, port):
    # Create UDP socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    
    # Bind the socket to any available interface on the specified port
    server_address = (ip, port)
    print(f'Starting UDP listener on {ip}:{port}')
    try:
        sock.bind(server_address)
        print(f'Listening for UDP packets on {ip}:{port}...')
    except socket.error as e:
        print(f"Failed to bind to port {port}: {e}")
        sys.exit(1)
    
    return sock

def main():
    parser = argparse.ArgumentParser(description='UDP Packet Listener')
    parser.add_argument('--ip', default='127.0.0.1', help='IP address to listen on (default: 127.0.0.1)')
    parser.add_argument('--port', type=int, default=52021, help='Port to listen on (default: 52021)')
    args = parser.parse_args()

    BUFFER_SIZE = 4096
    
    sock = create_udp_socket(args.ip, args.port)
    
    try:
        while True:
            # Receive data
            data, address = sock.recvfrom(BUFFER_SIZE)
            timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S.%f')[:-3]
            print(f"\n[{timestamp}] Received {len(data)} bytes from {address}")
            try:
                # Try to decode as string
                decoded_data = data.decode('utf-8')
                print(f"Data: {decoded_data}")
            except UnicodeDecodeError:
                # If can't decode as string, print as bytes
                print(f"Raw data (hex): {data.hex()}")

    except KeyboardInterrupt:
        print("\nShutting down UDP listener...")
    finally:
        sock.close()

if __name__ == "__main__":
    main()
