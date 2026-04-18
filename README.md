# SHAM Protocol Implementation

## Simple Handshake And Messaging Protocol

A custom reliable transport protocol implementation built on top of UDP, featuring TCP-like reliability mechanisms including three-way handshake, sliding window flow control, and four-way connection termination. The protocol supports both file transfer and real-time chat communication.

## Features

### Core Protocol Features
- **Reliable Data Transfer**: Implements reliability over UDP using sequence numbers and acknowledgments
- **Three-Way Handshake**: TCP-style connection establishment (SYN, SYN-ACK, ACK)
- **Four-Way Termination**: Graceful connection closing (FIN, ACK, FIN, ACK)
- **Sliding Window Protocol**: Flow control with configurable window size (default: 4 packets)
- **Timeout & Retransmission**: Automatic packet retransmission on timeout (100ms default)
- **Packet Loss Simulation**: Built-in support for testing reliability under packet loss conditions
- **MD5 Checksum**: File integrity verification using MD5 hash

### Application Features
- **File Transfer Mode**: Reliable file transfer with integrity verification
- **Chat Mode**: Real-time bidirectional messaging with `/quit` command support
- **Logging**: Optional detailed protocol logging for debugging (via `RUDP_LOG` environment variable)

## Architecture

### Protocol Structure

#### SHAM Header
```c
struct sham_header {
    uint32_t seq_num;      // Sequence Number
    uint32_t ack_num;      // Acknowledgment Number
    uint16_t flags;        // Control flags (SYN, ACK, FIN)
    uint16_t window_size;  // Flow control window size
};
```

#### Control Flags
- `SYN_FLAG (0x1)`: Synchronize - connection establishment
- `ACK_FLAG (0x2)`: Acknowledgment
- `FIN_FLAG (0x4)`: Finish - connection termination

#### Protocol Constants
- Maximum packet size: 1024 bytes
- Maximum window size: 4 packets
- Timeout: 100ms
- Maximum buffer size: 4096 bytes

### Connection States
The protocol implements a state machine with the following states:
- `STATE_CLOSED`: No connection
- `STATE_LISTEN`: Server waiting for connections
- `STATE_SYN_SENT`: Client sent SYN, waiting for SYN-ACK
- `STATE_SYN_RECEIVED`: Server received SYN, sent SYN-ACK
- `STATE_ESTABLISHED`: Connection established, data transfer possible
- `STATE_FIN_WAIT_1`, `STATE_FIN_WAIT_2`: Connection closing stages
- `STATE_CLOSE_WAIT`, `STATE_LAST_ACK`: Passive close states
- `STATE_TIME_WAIT`: Final state before closing

## Prerequisites

### System Requirements
- Linux operating system
- GCC compiler
- OpenSSL development libraries

### Installing Dependencies

#### Ubuntu/Debian
```bash
sudo apt-get update
sudo apt-get install -y libssl-dev build-essential
```

Or use the Makefile target:
```bash
make install-deps
```

#### Fedora/RHEL
```bash
sudo dnf install openssl-devel gcc make
```

#### Arch Linux
```bash
sudo pacman -S openssl gcc make
```

## Building

Clone or navigate to the project directory and compile:

```bash
make
```

This will create two executables:
- `server`: The server application
- `client`: The client application

### Clean Build
```bash
make clean
make
```

## Usage

### File Transfer Mode

#### Server
Start the server on a specific port:
```bash
./server <port> [loss_rate]
```

**Parameters:**
- `<port>`: Port number to listen on (required)
- `[loss_rate]`: Optional packet loss rate (0.0 to 1.0) for testing reliability

**Example:**
```bash
# Basic server on port 8080
./server 8080

# Server with 10% packet loss simulation
./server 8080 0.1
```

#### Client
Send a file to the server:
```bash
./client <server_ip> <server_port> <input_file> <output_filename> [loss_rate]
```

**Parameters:**
- `<server_ip>`: IP address of the server
- `<server_port>`: Port number of the server
- `<input_file>`: Path to the file to send
- `<output_filename>`: Name to save the file as on the server
- `[loss_rate]`: Optional packet loss rate (0.0 to 1.0)

**Example:**
```bash
# Send document.pdf to server, save as received.pdf
./client 127.0.0.1 8080 document.pdf received.pdf

# Same transfer with 5% packet loss simulation
./client 127.0.0.1 8080 document.pdf received.pdf 0.05
```

### Chat Mode

#### Server
Start the server in chat mode:
```bash
./server <port> --chat [loss_rate]
```

**Example:**
```bash
# Chat server on port 8080
./server 8080 --chat

# Chat server with 10% packet loss
./server 8080 --chat 0.1
```

#### Client
Connect to chat server:
```bash
./client <server_ip> <server_port> --chat [loss_rate]
```

**Example:**
```bash
# Connect to chat server
./client 127.0.0.1 8080 --chat

# Connect with packet loss simulation
./client 127.0.0.1 8080 --chat 0.05
```

#### Chat Commands
- Type any message and press Enter to send
- Type `/quit` to gracefully close the connection
- Either party can initiate `/quit`

## Advanced Features

### Protocol Logging

Enable detailed protocol logging for debugging:

```bash
# Enable logging
export RUDP_LOG=1

# Run server (logs to server_log.txt)
./server 8080

# Run client (logs to client_log.txt)
./client 127.0.0.1 8080 --chat
```

Log files will contain:
- SYN/ACK/FIN packet exchanges
- Sequence and acknowledgment numbers
- Packet loss and retransmission events
- Timestamp information

### Packet Loss Simulation

Test protocol reliability under adverse network conditions:

```bash
# Server with 20% packet loss
./server 8080 0.2

# Client with 15% packet loss
./client 127.0.0.1 8080 test.txt output.txt 0.15
```

The protocol will automatically handle retransmissions to ensure reliable delivery.

## File Transfer Example

Complete workflow for transferring a file:

**Terminal 1 (Server):**
```bash
# Start server
./server 8080

# Wait for connection and file transfer
# MD5 checksum will be displayed after transfer completes
```

**Terminal 2 (Client):**
```bash
# Send file
./client 127.0.0.1 8080 myfile.txt transferred_file.txt

# File will be transferred reliably
# Connection closes automatically after transfer
```

**Verify integrity:**
```bash
# On client side
md5sum myfile.txt

# Compare with MD5 output on server side
```

## Chat Example

Complete workflow for chat communication:

**Terminal 1 (Server):**
```bash
# Start chat server
./server 8080 --chat

# Wait for client connection
# Type messages to send to client
# Type /quit to close connection
```

**Terminal 2 (Client):**
```bash
# Connect to chat server
./client 127.0.0.1 8080 --chat

# Type messages to send to server
# Type /quit to close connection
```

## Implementation Details

### Flow Control
- Uses sliding window protocol with window size of 4 packets
- Sender buffers unacknowledged packets for potential retransmission
- Receiver buffers out-of-order packets for in-order delivery
- Window size advertised in each packet header

### Reliability Mechanisms
1. **Sequence Numbers**: Every data packet has a unique sequence number
2. **Acknowledgments**: Receiver sends ACK for successfully received packets
3. **Timeouts**: Sender retransmits if ACK not received within 100ms
4. **Checksums**: MD5 verification for file transfer integrity

### Connection Management
- **Establishment**: Three-way handshake (SYN → SYN-ACK → ACK)
- **Data Transfer**: Reliable ordered delivery using sequence numbers
- **Termination**: Four-way handshake (FIN → ACK → FIN → ACK)

## Project Structure

```
FileTransfer_and_Chat/
├── sham.h           # Protocol header definitions and function declarations
├── sham.c           # Core protocol implementation
├── server.c         # Server application (file transfer & chat)
├── client.c         # Client application (file transfer & chat)
├── Makefile         # Build configuration
└── README.md        # This file
```

## Troubleshooting

### Connection Timeouts
- Verify server is running and listening on correct port
- Check firewall settings: `sudo ufw allow <port>`
- Ensure IP address is correct (use `127.0.0.1` for localhost)
- Try reducing packet loss rate if testing with loss simulation

### File Transfer Issues
- Ensure sufficient disk space on server
- Verify file permissions for reading input file
- Check MD5 checksum to confirm successful transfer
- Enable logging to debug protocol behavior

### Build Errors
```bash
# Missing OpenSSL
sudo apt-get install libssl-dev

# Permission denied
chmod +x server client

# Clean and rebuild
make clean && make
```

### Port Already in Use
```bash
# Find process using port
lsof -i :<port>

# Kill process if needed
kill -9 <PID>

# Or use a different port
./server 8081
```

## Performance Considerations

- **Packet Size**: Default 1024 bytes balances throughput and fragmentation
- **Window Size**: Default 4 packets provides good performance on most networks
- **Timeout**: 100ms timeout suitable for LAN; may need adjustment for WAN
- **Buffer Size**: 4KB buffer accommodates multiple packets for efficiency

## Testing Scenarios

### Basic Functionality Test
```bash
# Terminal 1
./server 8080

# Terminal 2
echo "Hello SHAM Protocol" > test.txt
./client 127.0.0.1 8080 test.txt output.txt

# Verify
cat output.txt
```

### Reliability Test
```bash
# Test with 30% packet loss
./server 8080 0.3                                    # Terminal 1
./client 127.0.0.1 8080 largefile.bin out.bin 0.3   # Terminal 2

# Verify integrity with MD5
```

### Chat Test
```bash
./server 8080 --chat              # Terminal 1
./client 127.0.0.1 8080 --chat    # Terminal 2
# Exchange messages and test /quit
```

## Known Limitations

- Single concurrent connection per server instance
- No congestion control implementation
- Fixed timeout value (not adaptive)
- Chat mode does not support multi-line messages
- Maximum file size limited by available memory
- No encryption or authentication

## Future Enhancements

- [ ] Multiple concurrent connections
- [ ] Adaptive timeout based on RTT measurement
- [ ] Congestion control algorithm
- [ ] Selective acknowledgment (SACK)
- [ ] Encryption and authentication
- [ ] Connection persistence
- [ ] Larger window sizes for high-bandwidth networks
- [ ] Compression support

## License

This project is an educational implementation of a reliable transport protocol.

## Authors

Developed as part of Operating Systems Networks coursework.

## Acknowledgments

- Protocol design inspired by TCP (RFC 793)
- OpenSSL library for MD5 checksum implementation
- UDP sockets for underlying unreliable transport