#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/stat.h> // Added this header for struct stat
#include <pthread.h> // For thread support

#define SERVER_PORT 5432
#define UDP_PORT 5433 // UDP port
#define BUFFER_SIZE 1024
#define MAX_PENDING 5 // Defines the maximum length for the queue of pending connections
#define MAX_LINE 256
#define MAX_CLIENTS 10 // Maximum number of clients

int client_sockets[MAX_CLIENTS]; // Array to hold client sockets
int client_count = 0; // Current count of clients
pthread_mutex_t clients_mutex; // Mutex for thread-safe access to client_sockets
int tcp_sock, udp_sock;

void *handle_client(void *client_sock_ptr);
void broadcast_message(const char *message);
void receive_file(int client_sock, const char *file_name);
void send_file(int client_sock, const char *file_name);

typedef struct {
    int tcp_sock_struct;
    int udp_sock_struct;
} client_args_t;

int main() {
    struct sockaddr_in sin;
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int tcp_sock_client;

    pthread_mutex_init(&clients_mutex, NULL); // Initialize the mutex

    // Build address data structure
    bzero((char *)&sin, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(SERVER_PORT);

    // Setup passive open
    if ((tcp_sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Server: socket");
        exit(1);
    }

    if ((bind(tcp_sock, (struct sockaddr *)&sin, sizeof(sin))) < 0) {
        perror("Server: bind");
        exit(1);
    }

    listen(tcp_sock, MAX_PENDING);

    // Create UDP socket
    if ((udp_sock = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Server: UDP socket");
        exit(1);
    }

    // Set up UDP socket address structure
    struct sockaddr_in udp_sin;
    bzero((char *)&udp_sin, sizeof(udp_sin));
    udp_sin.sin_family = AF_INET;
    udp_sin.sin_addr.s_addr = INADDR_ANY;
    udp_sin.sin_port = htons(UDP_PORT);

    // Bind the UDP socket
    if (bind(udp_sock, (struct sockaddr *)&udp_sin, sizeof(udp_sin)) < 0) {
        perror("Server: UDP bind");
        exit(1);
    }

    printf("Server listening on TCP port %d and UDP port %d\n", SERVER_PORT, UDP_PORT);
    printf("Waiting for connections...\n");
    fflush(stdout);

    while (1) {
        fflush(stdout);

        if ((tcp_sock_client = accept(tcp_sock, (struct sockaddr*)&client_addr, &addr_len)) < 0) {
            perror("Server: accept");
            continue;
        }

        client_args_t *args = (client_args_t *)malloc(sizeof(client_args_t));
        args->tcp_sock_struct = tcp_sock_client;
        args->udp_sock_struct = udp_sock;

        // Add the new client socket to the array
        pthread_mutex_lock(&clients_mutex);
        if (client_count < MAX_CLIENTS) {
            client_sockets[client_count++] = tcp_sock_client;
            printf("Client connected: %d\n", tcp_sock_client);
        } else {
            printf("Max clients connected. Connection refused for: %d\n", tcp_sock_client);
            close(tcp_sock_client);
            pthread_mutex_unlock(&clients_mutex);
            continue;
        }
        pthread_mutex_unlock(&clients_mutex);

        // Create a thread to handle the client
        pthread_t client_thread;
        if (pthread_create(&client_thread, NULL, handle_client, (void *)args) < 0) {
            perror("Could not create client thread");
            return 1;
        }
    }

    close(tcp_sock);
    close(udp_sock);
    pthread_mutex_destroy(&clients_mutex);
    return 0;
}

void *handle_client(void *args_ptr) {
    client_args_t *args = (client_args_t *)args_ptr;
    int tcp_sock_client = args->tcp_sock_struct;
    int udp_sock_client = args->udp_sock_struct;
    char buffer[BUFFER_SIZE];
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int bytes_read_udp;
    int bytes_read_tcp;

    while (1) {
        fflush(stdout);
        memset(buffer, 0, BUFFER_SIZE);
        bytes_read_udp = recvfrom(udp_sock_client, (char *)buffer, BUFFER_SIZE, MSG_WAITALL, (struct sockaddr *)&client_addr, &addr_len);
        //bytes_read_tcp = recv(tcp_sock_client, buffer, BUFFER_SIZE, 0);
        if (bytes_read_udp > 0) {
            buffer[bytes_read_udp] = '\0'; // Null-terminate the command string
            printf("Received command from client %d: %s\n", tcp_sock_client, buffer);

            // Broadcast the message to all clients
            if (strncmp(buffer, "%broadcast", 10) == 0) {
                broadcast_message(buffer);
            }

            // PUT command for file upload
            else if (strncmp(buffer, "%put", 4) == 0) {
                char file_name[MAX_LINE];
                sscanf(buffer, "%%put %s", file_name);
                receive_file(tcp_sock_client, file_name);
            }

            // GET command for file download
            else if (strncmp(buffer, "%get", 4) == 0) {
                char file_name[MAX_LINE];
                sscanf(buffer, "%%get %s", file_name);
                send_file(tcp_sock_client, file_name);
            }

            // Unknown command
            else {
                printf("Unknown command received: %s\n", buffer);
            }
        }
        else if (bytes_read_tcp > 0) {
            // buffer[bytes_read_udp] = '\0'; // Null-terminate the command string
            // printf("Received command from client %d: %s\n", tcp_sock, buffer);

            // // Broadcast the message to all clients
            // if (strncmp(buffer, "%broadcast", 5) == 0) {
            //     broadcast_message(buffer);
            // }

            // // PUT command for file upload
            // else if (strncmp(buffer, "%put", 4) == 0) {
            //     char file_name[MAX_LINE];
            //     sscanf(buffer, "%%put %s", file_name);
            //     printf("%s", file_name);
            //     receive_file(tcp_sock_client, file_name);
            // }

            // // GET command for file download
            // else if (strncmp(buffer, "%get", 4) == 0) {
            //     char file_name[MAX_LINE];
            //     sscanf(buffer, "%%get %s", file_name);
            //     send_file(tcp_sock_client, file_name);
            // }

            // // Unknown command
            // else {
            //     printf("Unknown command received: %s\n", buffer);
            // }
        }
        else if (bytes_read_udp <= 0 && bytes_read_tcp <= 0) {
            printf("No data received\n");
            break;
        }
    }

    // Remove the client from the client sockets array
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++) {
        if (client_sockets[i] == tcp_sock) {
            client_sockets[i] = client_sockets[--client_count]; // Replace with last client
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    close(tcp_sock);
    close(udp_sock);
    printf("Client disconnected: %d\n", tcp_sock);
    
    return NULL;
}

void broadcast_message(const char *message) {
    pthread_mutex_lock(&clients_mutex); // Lock access to the clients array
    char formatted_message[BUFFER_SIZE];
    snprintf(formatted_message, sizeof(formatted_message), "MSG:%s", message + 11); // Send MSG flag with message (+11 to remove "%broadcast")
    printf("Broadcast message being sent to all active clients: '%s'\n", message + 11);

    for (int i = 0; i < client_count; i++) {
            send(client_sockets[i], formatted_message, strlen(formatted_message), 0);
    }
    pthread_mutex_unlock(&clients_mutex); // Unlock access to the clients array
}

void receive_file(int client_sock, const char *file_name) {
    FILE *file = fopen(file_name, "wb");
    if (file == NULL) {
        perror("File open failed in receive_file");
        return;
    }

    // Receive file size
    uint32_t file_size;
    if (recv(client_sock, &file_size, sizeof(file_size), 0) <= 0) {
        perror("Error receiving file size");
        fclose(file);
        return;
    }
    file_size = ntohl(file_size); // Convert to host byte order

    // Write received file data to the created file
    char buffer[BUFFER_SIZE];
    int bytes_received;
    while (file_size > 0) {
        bytes_received = recv(client_sock, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            break;
        }
        fwrite(buffer, sizeof(char), bytes_received, file);
        file_size -= bytes_received;
    }
    fclose(file);
    printf("File '%s' received from client\n", file_name);
}

void send_file(int client_sock, const char *file_name) {
    char buffer[BUFFER_SIZE];
    int bytes_read;
    struct stat file_stat;

    // Send the FILE flag to the client
    if (send(client_sock, "FILE", 4, 0) == -1) {
        perror("Error sending FILE flag");
        return;
    }
    printf("FILE flag sent to client\n");

    // Send the file name to the client
    if (send(client_sock, file_name, strlen(file_name) + 1, 0) == -1) { // Include the null terminator
        perror("Error sending file name");
        return;
    }
    printf("File name '%s' sent to client\n", file_name);

    FILE *file = fopen(file_name, "rb");
    if (file == NULL) {
        perror("File open failed in send_file");
        return;
    }

    if (stat(file_name, &file_stat) < 0) {
        perror("stat");
        fclose(file);
        return;
    }

    uint32_t file_size = htonl(file_stat.st_size); // Convert to network byte order

    // Send file size
    if (send(client_sock, &file_size, sizeof(file_size), 0) == -1) {
        perror("Error sending file size");
        fclose(file);
        return;
    }

    // Send file data
    while ((bytes_read = fread(buffer, sizeof(char), BUFFER_SIZE, file)) > 0) {
        if (send(client_sock, buffer, bytes_read, 0) == -1) {
            perror("send");
            fclose(file);
            return;
        }
    }

    fclose(file);
    printf("File '%s' sent to client\n", file_name);
}