#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h> 
#include <sys/stat.h> // Added this header for struct stat
#include <arpa/inet.h>
#include <pthread.h>

#define SERVER_PORT 5432
#define BUFFER_SIZE 1024
#define MAX_PENDING 5 // Defines the maximum length for the queue of pending connections
#define MAX_LINE 256
#define MAX_CLIENTS 100 // Maximum number of clients to track

struct client_info {
    struct sockaddr_in address;
    int socket;
    pid_t pid; // Process ID of the client handler
};

struct client_info clients[MAX_CLIENTS];
int client_count = 0;
pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;

void handle_client(int client_sock, struct sockaddr_in client_addr, pid_t pid);
void add_client(struct sockaddr_in client_addr, int client_sock, pid_t pid);
void remove_client(pid_t pid);
void print_active_clients();

int main() {
    struct sockaddr_in sin;
    struct sockaddr_in client_addr;
    int len;
    int s, client_sock;
    socklen_t addr_len = sizeof(client_addr);
    pid_t child_pid;
	char client_ip[INET_ADDRSTRLEN]; // Buffer to store the client’s IP address

    // Build address data structure
    bzero((char *)&sin, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(SERVER_PORT);

    // Setup passive open
    if ((s = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Server: socket");
        exit(1);
    }

    if ((bind(s, (struct sockaddr *)&sin, sizeof(sin))) < 0) {
        perror("Server: bind");
        exit(1);
    }

    listen(s, MAX_PENDING);
    printf("Server listening on port %d\n", SERVER_PORT);

    while (1) {
        if ((client_sock = accept(s, (struct sockaddr*)&client_addr, &addr_len)) < 0) {
            perror("Server: accept");
            continue;
        }

		// Get the client's IP address and port
        inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
        int client_port = ntohs(client_addr.sin_port);

        child_pid = fork(); // Create child process to handle a client
        if (child_pid == 0) { // Child process
            close(s); // Child doesn't need the listener socket
            handle_client(client_sock, client_addr, child_pid);
            exit(0);
        } else if (child_pid > 0) { // Parent process
            pthread_mutex_lock(&client_mutex);
            add_client(client_addr, client_sock, child_pid);
            print_active_clients();
            pthread_mutex_unlock(&client_mutex);
            close(client_sock); // Parent doesn't need the connected client socket
        } else {
            perror("Fork failed");
            close(client_sock);
            continue;
        }
    }

    close(s);
    return 0;
}

void handle_client(int client_sock, struct sockaddr_in client_addr, pid_t pid) {
    FILE *file;
    char buffer[BUFFER_SIZE];
    char command[5];
    char file_name[256];
    int bytes_read;
    struct stat file_stat;
    uint32_t file_size;

	//pid_t pid = getpid();

	char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
    int client_port = ntohs(client_addr.sin_port);

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        bytes_read = recv(client_sock, buffer, BUFFER_SIZE, 0);

        if (bytes_read == 0) { // Client disconnected
            printf("Client %s:%d has disconnected.\n\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            pthread_mutex_lock(&client_mutex); // Lock thread because of forking
            remove_client(pid); // Call to remove the client
            pthread_mutex_unlock(&client_mutex);
            break; // Exit the loop
        }

        buffer[bytes_read] = '\0'; // Null-terminate the command string
        printf("Received command: %s\n", buffer);

        sscanf(buffer, "%s %s", command, file_name);

        if (strcmp(command, "put") == 0) {
            file = fopen(file_name, "wb");
            if (file == NULL) {
                perror("File open failed in put");
                close(client_sock);
                break;
            }
            printf("File '%s' opened for writing\n", file_name);

            if (recv(client_sock, &file_size, sizeof(file_size), 0) <= 0) {
                perror("Error receiving file size");
                fclose(file);
                break;
            }
            file_size = ntohl(file_size);
            printf("Receiving file of size %u bytes\n", file_size);

            while (file_size > 0) {
                bytes_read = recv(client_sock, buffer, BUFFER_SIZE, 0);
                if (bytes_read <= 0) break;
                fwrite(buffer, sizeof(char), bytes_read, file);
                file_size -= bytes_read;
            }
            fclose(file);
            printf("File '%s' received from client\n", file_name);

        } else if (strcmp(command, "get") == 0) {
            file = fopen(file_name, "rb");
            if (file == NULL) {
                perror("File open failed in get");
                close(client_sock);
                exit(1);
            }

            if (stat(file_name, &file_stat) < 0) {
                perror("stat");
                fclose(file);
                close(client_sock);
                return;
            }
            file_size = htonl(file_stat.st_size);

            if (send(client_sock, &file_size, sizeof(file_size), 0) == -1) {
                perror("Error sending file size");
                fclose(file);
                close(client_sock);
                return;
            }

            while ((bytes_read = fread(buffer, sizeof(char), BUFFER_SIZE, file)) > 0) {
                if (send(client_sock, buffer, bytes_read, 0) == -1) {
                    perror("send");
                    fclose(file);
                    close(client_sock);
                    return;
                }
            }
            fclose(file);
            printf("File '%s' sent to client\n", file_name);
        } else {
            printf("Unknown command received: %s\n", command);
        }
    }

    close(client_sock);
    //printf("Client IP: %s, Port: %d has disconnected\n\n", client_ip, client_port);
}

// Client handling functions
void add_client(struct sockaddr_in client_addr, int client_sock, pid_t pid) {
    if (client_count < MAX_CLIENTS) {
        clients[client_count].address = client_addr;
        clients[client_count].socket = client_sock;
        clients[client_count].pid = pid;
        client_count++;

		// Print active client information
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
        int client_port = ntohs(client_addr.sin_port);
        printf("Client connected: IP %s, Port %d. Total clients: %d\n", client_ip, client_port, client_count);
    }
	else {
        printf("Max clients reached. Connection refused.\n");
        close(client_sock); // Close the socket if the max limit is reached
    }
}

void remove_client(pid_t pid) {
    for (int i = 0; i < client_count; i++) {
        if (clients[i].pid == pid) {
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(clients[i].address.sin_addr), client_ip, INET_ADDRSTRLEN);
            int client_port = ntohs(clients[i].address.sin_port);

            // Shift remaining clients up to fill the gap
            for (int j = i; j < client_count - 1; j++) {
                clients[j] = clients[j + 1];
            }
            client_count--;

            //printf("Client disconnected: IP %s, Port %d. Total clients: %d\n", client_ip, client_port, client_count);
            return;
        }
    }
}

void print_active_clients() {
    printf("Active clients (%d):\n", client_count);
    for (int i = 0; i < client_count; i++) {
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(clients[i].address.sin_addr), client_ip, INET_ADDRSTRLEN);
        printf("Client %d - IP: %s, PID: %d\n", i + 1, client_ip, clients[i].pid);
    }
	printf("\n");
}