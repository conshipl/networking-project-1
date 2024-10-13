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
#include <arpa/inet.h> // For UDP

#define SERVER_TCP_PORT 5432
#define SERVER_UDP_PORT 5433 // Different port for UDP commands
#define BUFFER_SIZE 1024
#define MAX_PENDING 5 // Defines the maximum length for the queue of pending connections
#define MAX_LINE 256
#define MAX_CLIENTS 10 // Maximum number of clients

int client_sockets[MAX_CLIENTS]; // Array to hold client sockets
int client_count = 0; // Current count of clients
pthread_mutex_t clients_mutex; // Mutex for thread-safe access to client_sockets

void broadcast_message(const char *message, int sender_sock);
void receive_file(int client_sock, const char *file_name);
void send_file(int client_sock, const char *file_name);

void *handle_commands(void *udp_sock_ptr, void *tcp_sock_ptr);

int main() {
    struct sockaddr_in tcp_sin, udp_sin;
    int tcp_sock, udp_sock;
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    // Initialize the mutex
    pthread_mutex_init(&clients_mutex, NULL);

    // Create TCP socket
    if ((tcp_sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Server: TCP socket");
        exit(1);
    }

    // Create UDP socket
    if ((udp_sock = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Server: UDP socket");
        exit(1);
    }

    // Setup TCP socket
    bzero((char *)&tcp_sin, sizeof(tcp_sin));
    tcp_sin.sin_family = AF_INET;
    tcp_sin.sin_addr.s_addr = INADDR_ANY;
    tcp_sin.sin_port = htons(SERVER_TCP_PORT);

    if (bind(tcp_sock, (struct sockaddr *)&tcp_sin, sizeof(tcp_sin)) < 0) {
        perror("Server: bind TCP");
        exit(1);
    }

    listen(tcp_sock, MAX_PENDING);
    printf("Server listening on TCP port %d\n", SERVER_TCP_PORT);

    // Setup UDP socket
    bzero((char *)&udp_sin, sizeof(udp_sin));
    udp_sin.sin_family = AF_INET;
    udp_sin.sin_addr.s_addr = INADDR_ANY;
    udp_sin.sin_port = htons(SERVER_UDP_PORT);

    if (bind(udp_sock, (struct sockaddr *)&udp_sin, sizeof(udp_sin)) < 0) {
        perror("Server: bind UDP");
        exit(1);
    }

    printf("Server listening on UDP port %d\n", SERVER_UDP_PORT);

    // Handle commands in a separate thread
    pthread_t command_thread;
    pthread_create(&command_thread, NULL, handle_commands, (void *)&udp_sock);

    while (1) {
        fflush(stdout);

        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_sock = accept(tcp_sock, (struct sockaddr*)&client_addr, &addr_len);
        if (client_sock < 0) {
            perror("Server: accept");
            continue;
        }
    }

    close(tcp_sock);
    close(udp_sock);
    pthread_mutex_destroy(&clients_mutex);
    return 0;
}

void *handle_commands(void *udp_sock_ptr, void *tcp_sock_ptr) {
    int udp_sock = *(int *)udp_sock_ptr;
    int tcp_sock = *(int *)tcp_sock_ptr;
    char buffer[BUFFER_SIZE];
    struct sockaddr_in client_addr;
    int bytes_read;
    socklen_t addr_len = sizeof(client_addr);

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recvfrom(udp_sock, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &addr_len);

        if (bytes_read <= 0) {
            //perror("recv");
            continue;
        }

        buffer[bytes_received] = '\0';
        printf("Received UDP command: %s\n", buffer);

        // Add the client socket to the udp_client_sockets array
        pthread_mutex_lock(&clients_mutex);
        if (client_count < MAX_CLIENTS) {
            client_sockets[client_count++] = client_addr.sin_port; // Store port or any identifier as needed
            printf("UDP Client added: %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        } else {
            printf("Max UDP clients reached. Connection refused from: %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        }
        pthread_mutex_unlock(&udp_clients_mutex);

        // Broadcast the message to all clients
        if (strncmp(buffer, "%broadcast", 5) == 0) {
            broadcast_message(buffer, cleint_addr);
        }

        // PUT command for file upload
        else if (strncmp(buffer, "%put", 4) == 0) {
            char file_name[MAX_LINE];
            sscanf(buffer, "%%put %s", file_name);
            printf("%s", file_name);
            receive_file(tcp_sock, file_name);
        }

        // GET command for file download
        else if (strncmp(buffer, "%get", 4) == 0) {
            char file_name[MAX_LINE];
            sscanf(buffer, "%%get %s", file_name);
            send_file(tcp_sock, file_name);
        }

        // Unknown command
        else {
            printf("Unknown command received: %s\n", buffer);
        }
    }

    // Remove the client from the client sockets array
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++) {
        if (client_sockets[i] == udp_sock) {
            client_sockets[i] = client_sockets[--client_count]; // Replace with last client
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    close(udp_sock);
    printf("Client disconnected: %d\n", udp_sock);
}

void broadcast_message(const char *message, int sender_sock) {
    pthread_mutex_lock(&clients_mutex); // Lock access to the clients array
    char formatted_message[BUFFER_SIZE];
    snprintf(formatted_message, sizeof(formatted_message), "MSG:%s", message + 11); // Send MSG flag with message (+11 to remove "%broadcast")
    printf("Broadcast message being sent to all active clients: '%s'\n", message + 11);

    for (int i = 0; i < client_count; i++) {
        //if (client_sockets[i] != sender_sock) {
            send(client_sockets[i], formatted_message, strlen(formatted_message), 0);
        //}
    }
    pthread_mutex_unlock(&clients_mutex); // Unlock access to the clients array
}

void receive_file(int tcp_sock, const char *file_name) {
    FILE *file = fopen(file_name, "wb");
    if (file == NULL) {
        perror("File open failed in receive_file");
        return;
    }

    // Receive file size
    uint32_t file_size;
    if (recv(tcp_sock, &file_size, sizeof(file_size), 0) <= 0) {
        perror("Error receiving file size");
        fclose(file);
        return;
    }
    file_size = ntohl(file_size); // Convert to host byte order

    // Write received file data to the created file
    char buffer[BUFFER_SIZE];
    int bytes_received;
    while (file_size > 0) {
        bytes_received = recv(tcp_sock, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            break;
        }
        fwrite(buffer, sizeof(char), bytes_received, file);
        file_size -= bytes_received;
    }
    fclose(file);
    printf("File '%s' received from client\n", file_name);
}

void send_file(int tcp_sock, const char *file_name) {
    char buffer[BUFFER_SIZE];
    int bytes_read;
    struct stat file_stat;

    // Send the FILE flag to the client
    if (send(tcp_sock, "FILE", 4, 0) == -1) {
        perror("Error sending FILE flag");
        return;
    }
    printf("FILE flag sent to client\n");

    // Send the file name to the client
    if (send(tcp_sock, file_name, strlen(file_name) + 1, 0) == -1) { // Include the null terminator
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
    if (send(tcp_sock, &file_size, sizeof(file_size), 0) == -1) {
        perror("Error sending file size");
        fclose(file);
        return;
    }

    // Send file data
    while ((bytes_read = fread(buffer, sizeof(char), BUFFER_SIZE, file)) > 0) {
        if (send(tcp_sock, buffer, bytes_read, 0) == -1) {
            perror("send");
            fclose(file);
            return;
        }
    }

    fclose(file);
    printf("File '%s' sent to client\n", file_name);
}