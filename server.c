#include "sham.h"

int main(int argc, char *argv[])
{
    if (argc < 2 || argc > 4)
    {
        printf("Invalid Number of Arguments\n");
        return 1;
    }

    int port = atoi(argv[1]);
    int chat_mode = 0;
    float loss_rate = 0.0;

    // Parse arguments
    for (int i = 2; i < argc; i++)
    {
        if (strcmp(argv[i], "--chat") == 0)
        {
            chat_mode = 1;
        }
        else
        {
            loss_rate = atof(argv[i]);
            if (loss_rate < 0.0 || loss_rate > 1.0)
            {
                fprintf(stderr, "Loss rate must be between 0.0 and 1.0\n");
                return 1;
            }
        }
    }

    // Create and bind socket
    int sockfd = sham_socket();
    if (sockfd < 0)
    {
        return 1;
    }

    if (sham_bind(sockfd, port) < 0)
    {
        close(sockfd);
        return 1;
    }

    // printf("Server listening on port %d\n", port);
    if (chat_mode)
    {
        // printf("Chat mode enabled\n");
    }
    if (loss_rate > 0.0)
    {
        // printf("Packet loss rate: %.2f\n", loss_rate);
    }

    while (1)
    {
        struct sham_connection conn;
        init_connection(&conn, sockfd, loss_rate);
        conn.is_chat_mode = chat_mode;

        // printf("Waiting for connection...\n");

        // Accept connection
        if (sham_accept(&conn, sockfd, 1) < 0)
        {
            continue;
        }

        // printf("Connection established with client\n");

        if (chat_mode)
        {
            // Chat mode
            fd_set read_fds;
            char buffer[MAX_BUFFER_SIZE];
            char input_buffer[MAX_PACKET_SIZE];

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
                handle_timeout(&conn, 1);

                // Check for incoming data from client
                if (FD_ISSET(sockfd, &read_fds))
                {
                    int bytes_received = sham_recv_data(&conn, buffer, sizeof(buffer) - 1, 1);
                    if (bytes_received > 0)
                    {
                        buffer[bytes_received] = '\0';
                        if (strcmp(buffer, "/quit") == 0)
                        {
                            // printf("Client initiated quit\n");
                        }
                        else
                        {
                            printf("Client: %s\n", buffer);
                        }
                    }
                    else if (bytes_received == 0)
                    {
                        break;
                    }
                }

                // Check for input from server user
                if (FD_ISSET(STDIN_FILENO, &read_fds))
                {
                    if (fgets(input_buffer, sizeof(input_buffer), stdin) != NULL)
                    {
                        // Remove newline
                        input_buffer[strcspn(input_buffer, "\n")] = '\0';

                        if (strcmp(input_buffer, "/quit") == 0)
                        {
                            sham_send_data(&conn, input_buffer, strlen(input_buffer), 1);
                            // printf("Server initiated quit\n");
                            sham_close(&conn, 1);
                            break;
                        }

                        sham_send_data(&conn, input_buffer, strlen(input_buffer), 1);
                    }
                }
            }
        }
        else
        {
            // File transfer mode
            char filename[MAX_FILENAME_SIZE];
            char buffer[MAX_BUFFER_SIZE];
            FILE *output_file = NULL;
            size_t total_bytes = 0;

            // First, receive filename
            int bytes_received = sham_recv_data(&conn, filename, sizeof(filename) - 1, 1);
            if (bytes_received > 0)
            {
                filename[bytes_received] = '\0';
                // printf("Receiving file: %s\n", filename);

                output_file = fopen(filename, "wb");
                if (!output_file)
                {
                    perror("fopen");
                    sham_close(&conn, 1);
                    continue;
                }

                // Receive file data
                while (1)
                {
                    bytes_received = sham_recv_data(&conn, buffer, sizeof(buffer), 1);
                    if (bytes_received > 0)
                    {
                        // printf("%s\n", buffer);
                        fwrite(buffer, 1, bytes_received, output_file);
                        total_bytes += bytes_received;
                    }
                    else if (bytes_received == 0)
                    {
                        // Connection closed or FIN received
                        // printf("broken\n");
                        break;
                    }
                    else
                    {
                        // Error or timeout - keep trying
                        handle_timeout(&conn, 1);
                        usleep(1000);
                    }
                }

                fclose(output_file);
                // printf("File transfer completed. Received %zu bytes\n", total_bytes);

                // Calculate and print MD5
                char md5_hash[33];
                calculate_md5(filename, md5_hash);
                printf("MD5: %s\n", md5_hash);
            }
        }

        // sham_close(&conn);
        // printf("Connection closed\n");

        // if (!chat_mode)
        // {
        //     // In file transfer mode, handle one connection and exit
        //     break;
        // }
    }

    close(sockfd);
    return 0;
}
