#ifndef SHAM_H
#define SHAM_H

#define _GNU_SOURCE      // Enable GNU extensions and functions
#define _DEFAULT_SOURCE  // Enable usleep and other functions

#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include <openssl/md5.h>
#include <time.h>
#include <stdarg.h>

// S.H.A.M. Header Structure
struct sham_header {
    uint32_t seq_num;      // Sequence Number
    uint32_t ack_num;      // Acknowledgment Number
    uint16_t flags;        // Control flags (SYN, ACK, FIN)
    uint16_t window_size;  // Flow control window size
};

// Flag definitions
#define SYN_FLAG 0x1  // Synchronise
#define ACK_FLAG 0x2  // Acknowledge
#define FIN_FLAG 0x4  // Finish

// Protocol constants
#define MAX_PACKET_SIZE 1024
#define MAX_WINDOW_SIZE 4
#define TIMEOUT_MS 100
#define MAX_BUFFER_SIZE 4096
#define MAX_FILENAME_SIZE 256

// Connection states
typedef enum {
    STATE_CLOSED,
    STATE_LISTEN,
    STATE_SYN_SENT,
    STATE_SYN_RECEIVED,
    STATE_ESTABLISHED,
    STATE_FIN_WAIT_1,
    STATE_FIN_WAIT_2,
    STATE_CLOSE_WAIT,
    STATE_LAST_ACK,
    STATE_TIME_WAIT
} connection_state_t;

// Packet structure for sending/receiving
struct sham_packet {
    struct sham_header header;
    char data[MAX_PACKET_SIZE];
    size_t data_len;
};

// Buffer entry for sliding window
struct buffer_entry {
    struct sham_packet packet;
    struct timeval timestamp;
    int in_use;
};

// Connection structure
struct sham_connection {
    int sockfd;
    struct sockaddr_in peer_addr;
    socklen_t peer_len;
    
    // Sequence numbers
    uint32_t seq_num;
    uint32_t ack_num;
    uint32_t initial_seq;
    uint32_t peer_initial_seq;
    
    // Flow control
    uint16_t window_size;
    uint16_t peer_window_size;
    
    // Sliding window buffers
    struct buffer_entry send_buffer[MAX_WINDOW_SIZE];
    struct buffer_entry recv_buffer[MAX_WINDOW_SIZE];
    
    // Window management
    int send_base;
    int next_seq;
    int recv_base;
    
    // State
    connection_state_t state;
    
    // File transfer
    FILE* file;
    char filename[MAX_FILENAME_SIZE];
    size_t bytes_sent;
    size_t bytes_received;
    
    // Loss simulation
    float loss_rate;
    
    // Chat mode
    int is_chat_mode;
};

// Function prototypes
int sham_socket(void);
int sham_bind(int sockfd, int port);
int sham_connect(struct sham_connection* conn, const char* server_ip, int server_port, int user);
int sham_accept(struct sham_connection* conn, int listen_sock, int user);
int sham_send_data(struct sham_connection* conn, const char* data, size_t len, int user);
int sham_recv_data(struct sham_connection* conn, char* buffer, size_t max_len, int user);
int sham_close(struct sham_connection* conn, int user);

// Utility functions
void init_connection(struct sham_connection* conn, int sockfd, float loss_rate);
int create_packet(struct sham_packet* packet, uint32_t seq, uint32_t ack, 
                  uint16_t flags, uint16_t window, const char* data, size_t data_len);
int send_packet(struct sham_connection* conn, struct sham_packet* packet, int user);
int receive_packet(struct sham_connection* conn, struct sham_packet* packet);
int should_drop_packet(float loss_rate);
long get_time_diff_ms(struct timeval* start, struct timeval* end);
void handle_timeout(struct sham_connection* conn, int user);
void process_ack(struct sham_connection* conn, uint32_t ack_num);
int is_in_window(uint32_t seq, uint32_t base, uint32_t window_size);
uint32_t generate_initial_seq(void);
void calculate_md5(const char* filename, char* md5_string);

#endif // SHAM_H