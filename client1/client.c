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
#define BUFFER_SIZE 1024

void receive_file(FILE *fp, int s);
void send_file(int s, FILE *fp, const char *file_name);
void *receive_messages(void *socket_desc);

int main(int argc, char *argv[]) {
    FILE *fp;
    int s;
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

    // Build address data structure
    bzero((char *)&sin, sizeof(sin));
    sin.sin_family = AF_INET;
    bcopy(hp->h_addr, (char *)&sin.sin_addr, hp->h_length);
    sin.sin_port = htons(SERVER_PORT);

    // Active open
    if ((s = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Client: socket");
        exit(1);
    }

    if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        perror("Client: connect");
        close(s);
        exit(1);
    }

	// Create a separate thread to listen for messages from the server
    pthread_t thread_id;
    if (pthread_create(&thread_id, NULL, receive_messages, (void *)&s) != 0) {
        perror("Failed to create thread");
        close(s);
        exit(1);
    }

    while (1) {
        // Get user input from command line
        printf("Enter command (send <message>, put <file_name>, get <file_name>, exit): ");
        memset(buffer, 0, BUFFER_SIZE);
        fgets(buffer, BUFFER_SIZE, stdin);

        // Remove newline character from input
        buffer[strcspn(buffer, "\n")] = 0;

        if (strcmp(buffer, "exit") == 0) {
            break;
        }

        // Send command to the server
        send(s, buffer, strlen(buffer), 0);

        // Handle 'put' command
        if (strncmp(buffer, "put", 3) == 0) {
            sscanf(buffer, "put %s", file_name);
            fp = fopen(file_name, "rb");
            if (fp == NULL) {
                perror("File open error");
                continue;
            }
            send_file(s, fp, file_name);
            fclose(fp);
            printf("File '%s' sent to server.\n", file_name);
        }

        // Handle 'get' command
        else if (strncmp(buffer, "get", 3) == 0) {
            sscanf(buffer, "get %s", file_name);
            fp = fopen(file_name, "wb");
            if (fp == NULL) {
                perror("File open error");
                continue;
            }
            // Receive file from server
            receive_file(fp, s);
            printf("File '%s' received from server.\n", file_name);
            fclose(fp);
        }
    }

    // Close the socket
    close(s);
    return 0;
}

// Function to receive file from server
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

// Function to send file to server
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

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        bytes_received = recv(sock, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            perror("Error receiving message");
            break;
        }
        buffer[bytes_received] = '\0'; // Null-terminate the string
        printf("Broadcast message: %s\n", buffer);
    }

    return NULL;
}