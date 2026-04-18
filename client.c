#include "sham.h"

int main(int argc, char *argv[])
{
    if (argc < 4)
    {
        printf("Invalid Number of Arguments\n");
        return 1;
    }

    const char *server_ip = argv[1];
    int server_port = atoi(argv[2]);
    int chat_mode = 0;
    const char *input_file = NULL;
    const char *output_filename = NULL;
    float loss_rate = 0.0;

    if (strcmp(argv[3], "--chat") == 0)
    {
        chat_mode = 1;
        if (argc >= 5)
        {
            loss_rate = atof(argv[4]);
            if (loss_rate < 0.0 || loss_rate > 1.0)
            {
                fprintf(stderr, "Loss rate must be between 0.0 and 1.0\n");
                return 1;
            }
        }
    }
    else if (argc >= 6)
    {
        loss_rate = atof(argv[5]);
        if (loss_rate < 0.0 || loss_rate > 1.0)
        {
            fprintf(stderr, "Loss rate must be between 0.0 and 1.0\n");
            return 1;
        }
        input_file = argv[3];
        output_filename = argv[4];
    }
    else
    {
        input_file = argv[3];
        output_filename = argv[4];
    }

    // Create socket
    int sockfd = sham_socket();
    if (sockfd < 0)
    {
        return 1;
    }

    struct sham_connection conn;
    init_connection(&conn, sockfd, loss_rate);
    conn.is_chat_mode = chat_mode;

    // printf("Connecting to %s:%d\n", server_ip, server_port);
    if (chat_mode)
    {
        // printf("Chat mode enabled\n");
    }
    if (loss_rate > 0.0)
    {
        // printf("Packet loss rate: %.2f\n", loss_rate);
    }

    // Connect to server
    if (sham_connect(&conn, server_ip, server_port, 0) < 0)
    {
        fprintf(stderr, "Failed to connect to server\n");
        close(sockfd);
        return 1;
    }

    // printf("Connected to server\n");

    if (chat_mode)
    {
        // Chat mode
        fd_set read_fds;
        char buffer[MAX_BUFFER_SIZE];
        char input_buffer[MAX_PACKET_SIZE];

        // printf("Chat started. Type /quit to exit.\n");

        while (1)
        {
            FD_ZERO(&read_fds);
            FD_SET(STDIN_FILENO, &read_fds);
            FD_SET(sockfd, &read_fds);

            struct timeval timeout;
            timeout.tv_sec = 0;
            timeout.tv_usec = 10000; // 10ms timeout

            int activity = select(sockfd + 1, &read_fds, NULL, NULL, &timeout);

            if (activity < 0)
            {
                perror("select");
                break;
            }

            // Handle timeout for retransmissions
            handle_timeout(&conn, 0);

            // Check for incoming data from server
            if (FD_ISSET(sockfd, &read_fds))
            {
                int bytes_received = sham_recv_data(&conn, buffer, sizeof(buffer) - 1, 0);
                if (bytes_received > 0)
                {
                    buffer[bytes_received] = '\0';
                    if (strcmp(buffer, "/quit") == 0)
                    {
                        // printf("Server initiated quit\n");
                    }
                    else
                    {
                        printf("Server: %s\n", buffer);
                    }
                }
                else if (bytes_received == 0)
                {
                    break;
                }
            }

            // Check for input from client user
            if (FD_ISSET(STDIN_FILENO, &read_fds))
            {
                if (fgets(input_buffer, sizeof(input_buffer), stdin) != NULL)
                {
                    // Remove newline
                    input_buffer[strcspn(input_buffer, "\n")] = '\0';

                    if (strcmp(input_buffer, "/quit") == 0)
                    {
                        sham_send_data(&conn, input_buffer, strlen(input_buffer), 0);
                        // printf("Client initiated quit\n");
                        break;
                    }

                    sham_send_data(&conn, input_buffer, strlen(input_buffer), 0);
                }
            }
        }
    }
    else
    {
        // File transfer mode
        FILE *file = fopen(input_file, "rb");
        if (!file)
        {
            perror("fopen");
            sham_close(&conn, 0);
            close(sockfd);
            return 1;
        }

        // printf("Sending file: %s as %s\n", input_file, output_filename);

        // Send output filename first
        sham_send_data(&conn, output_filename, strlen(output_filename), 0);

        // Send file data
        char buffer[MAX_BUFFER_SIZE];
        size_t bytes_read;
        size_t total_sent = 0;

        while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0)
        {
            // printf("%s\n", buffer);
            int sent = sham_send_data(&conn, buffer, bytes_read, 0);
            if (sent < 0)
            {
                fprintf(stderr, "Error sending data\n");
                break;
            }
            total_sent += bytes_read;
            // printf("Sent %zu bytes (total: %zu)\n", bytes_read, total_sent);
        }

        fclose(file);
        // printf("File transfer completed. Sent %zu bytes\n", total_sent);
    }

    sham_close(&conn, 0);
    close(sockfd);
    // printf("Connection closed\n");

    return 0;
}
