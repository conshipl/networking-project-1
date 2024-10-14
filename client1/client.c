#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/stat.h> // Added this header for struct stat
#include <unistd.h>
#include <stdint.h>
#include <pthread.h>

#define SERVER_PORT 5432
#define UDP_PORT 5433 // Same UDP port as server
#define BUFFER_SIZE 1024

int tcp_sock, udp_sock;
struct sockaddr_in udp_sin;

void receive_file(FILE *fp, int s);
void send_file(int s, FILE *fp, const char *file_name);
void *receive_messages(void *socket_desc);

int main(int argc, char *argv[]) {
    FILE *fp;
    struct sockaddr_in sin;
    struct hostent *hp;
    char buffer[BUFFER_SIZE];
    char *host;
    char command[5];
    char file_name[256];
    int bytes_read;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s host\n", argv[0]);
        exit(1);
    }

    host = argv[1];

    // Translate host name into peer's IP address
    hp = gethostbyname(host);
    if (!hp) {
        fprintf(stderr, "Client: unknown host: %s\n", host);
        exit(1);
    }

    // Build server address structure
    bzero((char *)&sin, sizeof(sin));
    sin.sin_family = AF_INET;
    bcopy(hp->h_addr, (char *)&sin.sin_addr, hp->h_length);
    sin.sin_port = htons(SERVER_PORT);

    // Create TCP socket
    if ((tcp_sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Client: socket");
        exit(1);
    }

    // Connect to the server
    if (connect(tcp_sock, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        perror("Client: connect");
        close(tcp_sock);
        exit(1);
    }

    // Create UDP socket
    if ((udp_sock = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Client: UDP socket");
        exit(1);
    }

    // Set up UDP socket address structure
    bzero((char *)&udp_sin, sizeof(udp_sin));
    udp_sin.sin_family = AF_INET;
    udp_sin.sin_port = htons(UDP_PORT);
    bcopy(hp->h_addr, (char *)&udp_sin.sin_addr, hp->h_length);

    // Create a separate thread to listen for messages from the server
    pthread_t thread_id;
    if (pthread_create(&thread_id, NULL, receive_messages, (void *)&tcp_sock) != 0) {
        perror("Failed to create thread");
        close(tcp_sock);
        exit(1);
    }

    printf("Available Commands: \n%%broadcast <message>\n%%put <file_name>\n%%get <file_name>\n%%exit\n\n");

    while (1) {
        // Get user input from command line
        memset(buffer, 0, BUFFER_SIZE);
        fgets(buffer, BUFFER_SIZE, stdin);

        // Remove newline character from input
        buffer[strcspn(buffer, "\n")] = 0;

        if (strcmp(buffer, "%exit") == 0) {
            break;
        }

        // Send command to the server
        //send(tcp_sock, buffer, strlen(buffer), 0);
        sendto(udp_sock, buffer, strlen(buffer), MSG_CONFIRM, (struct sockaddr *)&udp_sin, sizeof(udp_sin));

        // Handle 'put' command
        if (strncmp(buffer, "%put", 4) == 0) {
            sscanf(buffer + 5, "%s", file_name);
            fp = fopen(file_name, "rb");
            if (fp == NULL) {
                perror("File open error");
                continue;
            }
            send_file(tcp_sock, fp, file_name);
            fclose(fp);
            printf("File '%s' sent to server\n\n", file_name);
        }
    }

    // Close the sockets
    close(tcp_sock);
    close(udp_sock);
    return 0;
}

// Receive file from server
void receive_file(FILE *fp, int s) {
    char buffer[BUFFER_SIZE];
    int bytes_received;
    uint32_t file_size;

    // Receive the file size
    if (recv(s, &file_size, sizeof(file_size), 0) <= 0) {
        perror("Error receiving file size");
        return;
    }
    file_size = ntohl(file_size);

    // Receive file data
    while (file_size > 0) {
        bytes_received = recv(s, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) break;
        fwrite(buffer, sizeof(char), bytes_received, fp);
        file_size -= bytes_received;
    }

    if (bytes_received < 0) {
        perror("Error receiving file");
    }
}

// Send file to server
void send_file(int s, FILE *fp, const char *file_name) {
    char buffer[BUFFER_SIZE];
    int bytes_read;
    struct stat file_stat;
    uint32_t file_size;

    // Get file size
    if (stat(file_name, &file_stat) < 0) {
        perror("stat");
        fclose(fp);
        return;
    }
    file_size = htonl(file_stat.st_size); // Convert to network byte order

    // Send file size
    if (send(s, &file_size, sizeof(file_size), 0) == -1) {
        perror("Error sending file size");
        fclose(fp);
        return;
    }

    // Send file data to server
    while ((bytes_read = fread(buffer, sizeof(char), BUFFER_SIZE, fp)) > 0) {
        if (send(s, buffer, bytes_read, 0) == -1) {
            perror("Error sending file");
            fclose(fp);
            return;
        }
    }
}

// Function to receive messages from the server
void *receive_messages(void *socket_desc) {
    int sock = *(int *)socket_desc;
    char buffer[BUFFER_SIZE];
    int bytes_received;
    char type_flag[5]; // Buffer to hold "MSG" or "FILE" flag
    char file_name[256]; // Buffer to hold the file name

    while (1) {
        // First receive the message type flag
        memset(type_flag, 0, sizeof(type_flag)); // Clear the type_flag buffer
        bytes_received = recv(sock, type_flag, 4, 0); // 4 bytes for "MSG" or "FILE"
        if (bytes_received <= 0) {
            perror("Error receiving message type");
            break;
        }
        type_flag[4] = '\0'; // Null-terminate the flag string

        // MSG: flag
        if (strcmp(type_flag, "MSG:") == 0) {
            memset(buffer, 0, BUFFER_SIZE);
            bytes_received = recv(sock, buffer, BUFFER_SIZE, 0);
            if (bytes_received <= 0) {
                perror("Error receiving message");
                break;
            }
            buffer[bytes_received] = '\0'; // Null-terminate the string
            printf("\nBroadcast Message: %s\n\n", buffer);
        }

        // FILE flag
        else if (strcmp(type_flag, "FILE") == 0) {
            // Receive the file name from the server
            memset(file_name, 0, sizeof(file_name));
            bytes_received = recv(sock, file_name, sizeof(file_name), 0);
            if (bytes_received <= 0) {
                perror("Error receiving file name");
                break;
            }
            printf("Receiving file: %s\n", file_name);

            // Open the file with the received name for writing
            FILE *fp = fopen(file_name, "wb");
            if (fp == NULL) {
                perror("File open error in receive_messages");
                continue;
            }

            // Receive the file from the server
            receive_file(fp, sock);
            fclose(fp);
            printf("File '%s' received and saved\n\n", file_name);
        }
        
        // Uknown flag
        else {
            printf("Unknown message type: %s\n\n", type_flag);
        }
    }

    return NULL;
}