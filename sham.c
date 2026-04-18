#define _GNU_SOURCE     // Enable GNU extensions and functions
#define _DEFAULT_SOURCE // Enable usleep and other functions
#include <stdarg.h>     // Include stdarg.h first for variadic functions
#include "sham.h"

void append_log(int n, const char *log, ...)
{
    const char *env = getenv("RUDP_LOG");
    if (env && strcmp(env, "1") == 0)
    {
        char buffer[512];
        va_list args;
        va_start(args, log);
        vsnprintf(buffer, sizeof(buffer), log, args); // format into buffer
        va_end(args);
        char time_buffer[30];
        struct timeval tv;
        time_t curtime;
        gettimeofday(&tv, NULL);
        curtime = tv.tv_sec;
        FILE *log_file;
        if (n == 0)
            log_file = fopen("client_log.txt", "a");
        if (n == 1)
            log_file = fopen("server_log.txt", "a");
        if (log_file)
        {
            strftime(time_buffer, 30, "%Y-%m-%d %H:%M:%S", localtime(&curtime));
            fprintf(log_file, "[%s.%06ld] [LOG] %s\n", time_buffer, tv.tv_usec, buffer);
            fclose(log_file);
        }
    }
    else
    {
        return;
    }
}

// Initialize a SHAM connection
void init_connection(struct sham_connection *conn, int sockfd, float loss_rate)
{
    memset(conn, 0, sizeof(struct sham_connection));
    conn->sockfd = sockfd;
    conn->state = STATE_CLOSED;
    conn->seq_num = 0;
    conn->initial_seq = conn->seq_num;
    conn->window_size = MAX_BUFFER_SIZE;
    conn->peer_window_size = MAX_BUFFER_SIZE;
    conn->loss_rate = loss_rate;

    // Initialize buffers
    for (int i = 0; i < MAX_WINDOW_SIZE; i++)
    {
        conn->send_buffer[i].in_use = 0;
        conn->recv_buffer[i].in_use = 0;
    }
}

// Create a SHAM socket
int sham_socket(void)
{
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("socket");
        return -1;
    }
    return sockfd;
}

// Bind socket to port
int sham_bind(int sockfd, int port)
{
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        return -1;
    }
    return 0;
}

// Check if packet should be dropped for loss simulation
int should_drop_packet(float loss_rate)
{
    if (loss_rate <= 0.0)
        return 0;
    float val = ((float)rand() / RAND_MAX);
    // printf("Random value: %f, Loss rate: %f\n", val, loss_rate);
    return val < loss_rate;
}

// Calculate time difference in milliseconds
long get_time_diff_ms(struct timeval *start, struct timeval *end)
{
    return (end->tv_sec - start->tv_sec) * 1000 +
           (end->tv_usec - start->tv_usec) / 1000;
}

// Create a SHAM packet
int create_packet(struct sham_packet *packet, uint32_t seq, uint32_t ack,
                  uint16_t flags, uint16_t window, const char *data, size_t data_len)
{
    if (data_len > MAX_PACKET_SIZE)
    {
        return -1;
    }

    packet->header.seq_num = htonl(seq);
    packet->header.ack_num = htonl(ack);
    packet->header.flags = htons(flags);
    packet->header.window_size = htons(window);

    if (data && data_len > 0)
    {
        memcpy(packet->data, data, data_len);
        packet->data_len = data_len;
    }
    else
    {
        packet->data_len = 0;
    }

    return 0;
}

// Send a packet
int send_packet(struct sham_connection *conn, struct sham_packet *packet, int user)
{
    // Simulate packet loss
    if (packet->header.flags == 0)
    {
        if (should_drop_packet(conn->loss_rate))
        {
            append_log(user, "DROP DATA SEQ=%u", ntohl(packet->header.seq_num));
            return packet->data_len + sizeof(struct sham_header);
        }
    }

    size_t total_len = sizeof(struct sham_header) + packet->data_len;
    ssize_t sent = sendto(conn->sockfd, packet, total_len, 0,
                          (struct sockaddr *)&conn->peer_addr, conn->peer_len);

    if (sent < 0)
    {
        perror("sendto");
        return -1;
    }

    return sent;
}

// Receive a packet
int receive_packet(struct sham_connection *conn, struct sham_packet *packet)
{
    conn->peer_len = sizeof(conn->peer_addr);
    ssize_t received = recvfrom(conn->sockfd, packet, sizeof(struct sham_packet), 0,
                                (struct sockaddr *)&conn->peer_addr, &conn->peer_len);

    if (received < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return 0; // No data available
        }
        perror("recvfrom");
        return -1;
    }

    if (received < sizeof(struct sham_header))
    {
        return -1; // Invalid packet
    }

    // Convert network byte order to host byte order
    packet->header.seq_num = ntohl(packet->header.seq_num);
    packet->header.ack_num = ntohl(packet->header.ack_num);
    packet->header.flags = ntohs(packet->header.flags);
    packet->header.window_size = ntohs(packet->header.window_size);

    packet->data_len = received - sizeof(struct sham_header);

    return received;
}

// Check if sequence number is within window
int is_in_window(uint32_t seq, uint32_t base, uint32_t window_size)
{
    return (seq >= base && seq < base + window_size);
}

// Process acknowledgment
void process_ack(struct sham_connection *conn, uint32_t ack_num)
{
    // printf("Processing ACK: ack_num=%u\n", ack_num);
    // Update peer window size from the ACK packet
    // Mark acknowledged packets in send buffer
    for (int i = 0; i < MAX_WINDOW_SIZE; i++)
    {
        if (conn->send_buffer[i].in_use)
        {
            uint32_t pkt_seq = ntohl(conn->send_buffer[i].packet.header.seq_num);
            uint32_t pkt_end = pkt_seq + conn->send_buffer[i].packet.data_len;
            // printf("Checking packet %d: seq=%u, end=%u, ack=%u\n", i, pkt_seq, pkt_end, ack_num);
            if (pkt_end <= ack_num)
            {
                // printf("Packet acknowledged: seq=%u\n", pkt_seq);
                conn->send_buffer[i].in_use = 0;
            }
        }
    }

    // Update send base - don't update seq_num here, it's managed by sender
    // if (ack_num > conn->seq_num) {
    //     conn->seq_num = ack_num;
    // }
}

// Handle timeouts and retransmissions
void handle_timeout(struct sham_connection *conn, int user)
{
    struct timeval current_time;
    gettimeofday(&current_time, NULL);

    for (int i = 0; i < MAX_WINDOW_SIZE; i++)
    {
        if (conn->send_buffer[i].in_use)
        {
            long elapsed = get_time_diff_ms(&conn->send_buffer[i].timestamp, &current_time);
            if (elapsed >= TIMEOUT_MS)
            {
                append_log(user, "TIMEOUT SEQ=%u", ntohl(conn->send_buffer[i].packet.header.seq_num));
                // Retransmit packet
                send_packet(conn, &conn->send_buffer[i].packet, user);
                append_log(user, "RTX DATA SEQ=%u LEN=%zu", ntohl(conn->send_buffer[i].packet.header.seq_num), conn->send_buffer[i].packet.data_len);
                gettimeofday(&conn->send_buffer[i].timestamp, NULL);
            }
        }
    }
}

// Three-way handshake for client
int sham_connect(struct sham_connection *conn, const char *server_ip, int server_port, int user)
{
    // Set up server address
    memset(&conn->peer_addr, 0, sizeof(conn->peer_addr));
    conn->peer_addr.sin_family = AF_INET;
    conn->peer_addr.sin_port = htons(server_port);
    conn->peer_len = sizeof(conn->peer_addr);

    if (inet_pton(AF_INET, server_ip, &conn->peer_addr.sin_addr) <= 0)
    {
        fprintf(stderr, "Invalid server IP address\n");
        return -1;
    }

    // Set socket to non-blocking
    int flags = fcntl(conn->sockfd, F_GETFL, 0);
    fcntl(conn->sockfd, F_SETFL, flags | O_NONBLOCK);

    // Step 1: Send SYN
    struct sham_packet syn_packet;
    create_packet(&syn_packet, conn->seq_num, 0, SYN_FLAG, conn->window_size, NULL, 0);
    send_packet(conn, &syn_packet, user);
    append_log(user, "SND SYN SEQ=%u", conn->seq_num);
    conn->state = STATE_SYN_SENT;

    // Wait for SYN-ACK
    struct timeval start_time, current_time;
    gettimeofday(&start_time, NULL);

    while (conn->state != STATE_ESTABLISHED)
    {
        gettimeofday(&current_time, NULL);
        if (get_time_diff_ms(&start_time, &current_time) > TIMEOUT_MS)
        {
            // Retransmit SYN
            send_packet(conn, &syn_packet, user);
            gettimeofday(&start_time, NULL);
        }

        struct sham_packet recv_packet;
        int result = receive_packet(conn, &recv_packet);
        if (result > 0)
        {
            uint16_t flags = recv_packet.header.flags;
            if ((flags & (SYN_FLAG | ACK_FLAG)) == (SYN_FLAG | ACK_FLAG))
            {
                // Received SYN-ACK
                conn->peer_initial_seq = recv_packet.header.seq_num;
                conn->ack_num = recv_packet.header.seq_num + 1;
                conn->peer_window_size = recv_packet.header.window_size;
                append_log(user, "FLOW WIN UPDATE=%d", recv_packet.header.window_size);

                // Step 3: Send ACK
                struct sham_packet ack_packet;
                create_packet(&ack_packet, conn->seq_num + 1, conn->ack_num,
                              ACK_FLAG, conn->window_size, NULL, 0);
                send_packet(conn, &ack_packet, user);

                conn->seq_num++;
                conn->state = STATE_ESTABLISHED;
                break;
            }
        }

        usleep(10000); // 10ms delay
    }

    return 0;
}

// Accept connection for server (simplified)
int sham_accept(struct sham_connection *conn, int listen_sock, int user)
{
    conn->sockfd = listen_sock;
    conn->state = STATE_LISTEN;

    // Set socket to non-blocking
    int flags = fcntl(conn->sockfd, F_GETFL, 0);
    fcntl(conn->sockfd, F_SETFL, flags | O_NONBLOCK);

    struct sham_packet recv_packet;

    // Wait for SYN
    while (conn->state != STATE_ESTABLISHED)
    {
        int result = receive_packet(conn, &recv_packet);
        if (result > 0)
        {
            uint16_t flags = recv_packet.header.flags;

            if (conn->state == STATE_LISTEN && (flags & SYN_FLAG))
            {
                // Received SYN
                conn->peer_initial_seq = recv_packet.header.seq_num;
                conn->ack_num = recv_packet.header.seq_num + 1;
                conn->peer_window_size = recv_packet.header.window_size;
                append_log(user, "RCV SYN SEQ=%u", recv_packet.header.seq_num);
                append_log(user, "FLOW WIN UPDATE=%d", recv_packet.header.window_size);

                // Send SYN-ACK
                struct sham_packet synack_packet;
                create_packet(&synack_packet, conn->seq_num, conn->ack_num,
                              SYN_FLAG | ACK_FLAG, conn->window_size, NULL, 0);
                send_packet(conn, &synack_packet, user);
                append_log(user, "SND SYN-ACK SEQ=%u ACK=%u", conn->seq_num, conn->ack_num);

                conn->state = STATE_SYN_RECEIVED;
            }
            else if (conn->state == STATE_SYN_RECEIVED && (flags & ACK_FLAG))
            {
                // Received final ACK
                if (recv_packet.header.ack_num == conn->seq_num + 1)
                {
                    append_log(user, "RCV ACK FOR SYN");
                    conn->seq_num++;
                    conn->state = STATE_ESTABLISHED;
                }
            }
        }

        usleep(10000); // 10ms delay
    }

    return 0;
}

// Send data
int sham_send_data(struct sham_connection *conn, const char *data, size_t len, int user)
{
    if (conn->state != STATE_ESTABLISHED)
    {
        return -1;
    }

    size_t bytes_sent = 0;
    while (bytes_sent < len)
    {
        // Check available window slots
        int free_slots = 0;
        for (int i = 0; i < MAX_WINDOW_SIZE; i++)
        {
            if (!conn->send_buffer[i].in_use)
            {
                free_slots++;
            }
        }

        if (free_slots == 0)
        {
            // Handle ACKs and timeouts
            struct sham_packet recv_packet;
            int result = receive_packet(conn, &recv_packet);
            if (result > 0 && (recv_packet.header.flags & ACK_FLAG))
            {
                process_ack(conn, recv_packet.header.ack_num);
            }
            handle_timeout(conn, user);
            usleep(1000); // 1ms delay
            continue;
        }

        // Send next chunk
        size_t chunk_size = (len - bytes_sent > MAX_PACKET_SIZE) ? MAX_PACKET_SIZE : len - bytes_sent;

        // Find free buffer slot
        for (int i = 0; i < MAX_WINDOW_SIZE; i++)
        {
            if (!conn->send_buffer[i].in_use)
            {
                // printf("Sending packet: seq=%u, ack=%u, len=%zu\n",
                //    conn->seq_num, conn->ack_num, chunk_size);
                create_packet(&conn->send_buffer[i].packet, conn->seq_num,
                              conn->ack_num, 0, conn->window_size,
                              data + bytes_sent, chunk_size);

                send_packet(conn, &conn->send_buffer[i].packet, user);
                append_log(user, "SND DATA SEQ=%u LEN=%zu", conn->seq_num, chunk_size);
                gettimeofday(&conn->send_buffer[i].timestamp, NULL);
                conn->send_buffer[i].in_use = 1;

                conn->seq_num += chunk_size;
                bytes_sent += chunk_size;
                break;
            }
        }
    }

    // Wait for all ACKs
    while (1)
    {
        int pending = 0;
        for (int i = 0; i < MAX_WINDOW_SIZE; i++)
        {
            if (conn->send_buffer[i].in_use)
            {
                pending++;
            }
        }
        // printf("pending %d\n", pending);
        if (pending == 0)
            break;

        struct sham_packet recv_packet;
        int result = receive_packet(conn, &recv_packet);
        // printf("in send_data ACK packet: result=%d seq=%u, ack=%u, len=%zu\n",
        //    result, recv_packet.header.seq_num, recv_packet.header.ack_num, recv_packet.data_len);
        if (result > 0 && (recv_packet.header.flags & ACK_FLAG))
        {
            append_log(user, "RCV ACK=%u", recv_packet.header.ack_num);
            process_ack(conn, recv_packet.header.ack_num);
        }
        handle_timeout(conn, user);
        usleep(10000);
    }

    return bytes_sent;
}

// Receive data
int sham_recv_data(struct sham_connection *conn, char *buffer, size_t max_len, int user)
{
    while (1)
    {
        int duplicate = 0;

        if (conn->state != STATE_ESTABLISHED)
        {
            return -1;
        }

        struct sham_packet recv_packet;
        int result = receive_packet(conn, &recv_packet);
        if (result < 0)
        {
            return result;
        }

        // Check for FIN
        if (recv_packet.header.flags & FIN_FLAG)
        {
            // printf("reveived fin1\n");
            conn->ack_num = recv_packet.header.seq_num + 1;
            struct sham_packet ack_packet;
            create_packet(&ack_packet, conn->seq_num, conn->ack_num,
                          ACK_FLAG, conn->window_size, NULL, 0);
            send_packet(conn, &ack_packet, user);
            // printf("sent ack for fin1\n");
            usleep(1000);

            // printf("sent fin2\n");
            struct sham_packet fin_packet;
            create_packet(&fin_packet, conn->seq_num, conn->ack_num,
                          FIN_FLAG, conn->window_size, NULL, 0);
            send_packet(conn, &fin_packet, user);
            usleep(1000);
            conn->state = STATE_CLOSED;
            return 0; // Connection closing
        }

        if (recv_packet.data_len > 0 && result != 0)
        {
            if (recv_packet.header.seq_num != conn->ack_num)
            {
                for (int i = 0; i < MAX_WINDOW_SIZE; i++)
                {
                    if (conn->recv_buffer[i].in_use &&
                        conn->recv_buffer[i].packet.header.seq_num == recv_packet.header.seq_num)
                        duplicate=1;
                }
                if(duplicate ==1)
                continue;
                append_log(user, "RCV DATA SEQ=%u LEN=%zu", recv_packet.header.seq_num, recv_packet.data_len);
                for (int i = 0; i < MAX_WINDOW_SIZE; i++)
                {
                    if (conn->recv_buffer[i].in_use == 0)
                    {
                        memcpy(&conn->recv_buffer[i].packet, &recv_packet, sizeof(struct sham_packet));
                        conn->recv_buffer[i].in_use = 1;
                        break;
                    }
                }
                continue;
            }

            size_t copy_len;
            if (recv_packet.header.seq_num == conn->ack_num)
            {
                conn->ack_num = recv_packet.header.seq_num + recv_packet.data_len;
                copy_len = (recv_packet.data_len > max_len) ? max_len : recv_packet.data_len;
                memcpy(buffer, recv_packet.data, copy_len);
                append_log(user, "RCV DATA SEQ=%u LEN=%zu", recv_packet.header.seq_num, recv_packet.data_len);
                // Check for next in-order packets in buffer
                for (int j = 0; j < 2; j++)
                {
                    for (int i = 0; i < MAX_WINDOW_SIZE; i++)
                    {
                        if (conn->recv_buffer[i].in_use &&
                            conn->recv_buffer[i].packet.header.seq_num == conn->ack_num)
                        {
                            size_t next_copy_len = (conn->recv_buffer[i].packet.data_len > max_len - copy_len) ? (max_len - copy_len) : conn->recv_buffer[i].packet.data_len;
                            memcpy(buffer + copy_len, conn->recv_buffer[i].packet.data, next_copy_len);
                            conn->ack_num += conn->recv_buffer[i].packet.data_len;
                            copy_len += next_copy_len;
                            conn->recv_buffer[i].in_use = 0;
                        }
                    }
                }
            }

            struct sham_packet ack_packet;
            // printf("in recv_data: result=%d seq=%u, sending ack=%u, len=%zu\n",
            //    result, recv_packet.header.seq_num, conn->ack_num, recv_packet.data_len);
            create_packet(&ack_packet, conn->seq_num, conn->ack_num,
                          ACK_FLAG, conn->window_size, NULL, 0);
            send_packet(conn, &ack_packet, user);
            append_log(user, "SND ACK=%u", conn->ack_num);
            // Copy data to buffer
            return copy_len;
        }
    }
}

// Close connection with 4-way handshake
int sham_close(struct sham_connection *conn, int user)
{
    if (conn->state != STATE_ESTABLISHED)
    {
        close(conn->sockfd);
        return 0;
    }
    while (1)
    {
        int found = 0;
        for (int i = 0; i < MAX_WINDOW_SIZE; i++)
        {
            if (conn->send_buffer[i].in_use)
            {
                found = 1;
            }
        }
        if (found)
        {
            handle_timeout(conn, user);
            usleep(10000);
        }
        else
            break;
    }

    // Send FIN
    struct sham_packet fin_packet;
    create_packet(&fin_packet, conn->seq_num, conn->ack_num,
                  FIN_FLAG, conn->window_size, NULL, 0);
    send_packet(conn, &fin_packet, user);
    // printf("sent fin1\n");
    conn->state = STATE_FIN_WAIT_1;
    conn->seq_num++;

    while (conn->state != STATE_CLOSED)
    {
        struct sham_packet recv_packet;
        int result = receive_packet(conn, &recv_packet);
        if (result > 0)
        {
            uint16_t flags = recv_packet.header.flags;

            if (conn->state == STATE_FIN_WAIT_1 && (flags & ACK_FLAG))
            {
                // printf("received ack\n");
                if (recv_packet.header.ack_num == conn->seq_num)
                {
                    conn->state = STATE_FIN_WAIT_2;
                }
            }
            else if (conn->state == STATE_FIN_WAIT_2 && (flags & FIN_FLAG))
            {
                // printf("received fin2\n");
                conn->ack_num = recv_packet.header.seq_num + 1;

                // Send final ACK
                struct sham_packet ack_packet;
                create_packet(&ack_packet, conn->seq_num, conn->ack_num,
                              ACK_FLAG, conn->window_size, NULL, 0);
                send_packet(conn, &ack_packet, user);

                conn->state = STATE_CLOSED;
            }
        }
        usleep(10000); // 10ms delay
    }
    return 0;
}

// Calculate MD5 checksum
void calculate_md5(const char *filename, char *md5_string)
{
    FILE *file = fopen(filename, "rb");
    if (!file)
    {
        strcpy(md5_string, "error");
        return;
    }

    MD5_CTX md5_ctx;
    MD5_Init(&md5_ctx);

    unsigned char buffer[1024];
    size_t bytes_read;

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0)
    {
        MD5_Update(&md5_ctx, buffer, bytes_read);
    }

    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5_Final(digest, &md5_ctx);

    for (int i = 0; i < MD5_DIGEST_LENGTH; i++)
    {
        sprintf(md5_string + (i * 2), "%02x", digest[i]);
    }
    md5_string[32] = '\0';

    fclose(file);
}
