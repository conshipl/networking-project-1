#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <dirent.h> // for accessing local directory
#include <unistd.h> // gethostname
#include <pthread.h>

#define UDP_PORT 5432
#define BUFFER_SIZE 1024

/*
 * Opens current directory (./) and iterates through all files within; if
 * file type is .txt, add it to the buffer.
 */
void registerLocalFiles(char* buffer) {
    DIR *d;
    struct dirent *dir;
    d = opendir(".");
    
    strcat(buffer, " ");

    if (d) {
	while ((dir = readdir(d)) != NULL) {
	    if (strstr(dir->d_name, ".txt")) {
		strcat(buffer, dir->d_name);
		strcat(buffer, ",");
	    }
	}

	closedir(d);
    }
}

// Main thread: accepts user input commands and sends them to the server.
//
// This function is utilized by a separate thread that listens for incoming datagrams and acts on them accordingly.
void *receiveDatagrams(void *socket_desc) {
    int socket = *(int *)socket_desc;
    char buffer[BUFFER_SIZE];
    struct sockaddr_in udp_sin;
    socklen_t addr_len = sizeof(udp_sin);
    int bytes_received;
    char type_flag[5]; // Buffer to hold flags
    char file_name[256]; // Buffer to hold filenames

    while (1) {
	    fflush(stdout);
	    bzero(type_flag, sizeof(type_flag));

	    bytes_received = recvfrom(socket, (char *)buffer, BUFFER_SIZE, 0 , (struct sockaddr *)&udp_sin, &addr_len);

        if (bytes_received > 0) {
            // Check if the message is a "ping" from the server
            if (strcmp(buffer, "ping") == 0) {
                // Send an "ack" response back to the server
                printf("\nping received\n");
                strcpy(buffer, "ack");
                sendto(socket, buffer, strlen(buffer), 0, (struct sockaddr*)&udp_sin, addr_len);
            } 
            else {
	            printf("Received buffer contents: %s\n", buffer);
            }
        }
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    FILE *fp;
    struct sockaddr_in udp_sin;
    struct hostent *hp;
    char *host;
    char buffer[BUFFER_SIZE];
    int udp_socket;

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

    // Create UDP socket
    if ((udp_socket = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Client: UDP socket");
        exit(1);
    }

    // Set up UDP socket address structure
    bzero((char *)&udp_sin, sizeof(udp_sin));
    udp_sin.sin_family = AF_INET;
    udp_sin.sin_port = htons(UDP_PORT);
    bcopy(hp->h_addr, (char *)&udp_sin.sin_addr, hp->h_length);

    // Send hostname and available files to server
    bzero(buffer, BUFFER_SIZE);
    gethostname(buffer, sizeof(buffer));
    strcat(buffer, " %cnct"); // CONNECT
    registerLocalFiles(buffer);
    sendto(udp_socket, buffer, strlen(buffer), 0, (struct sockaddr *)&udp_sin, sizeof(udp_sin));

    // Create separate thread to listen for datagrams from the server
    pthread_t thread_id;
    if (pthread_create(&thread_id, NULL, receiveDatagrams, (void *)&udp_socket) != 0) {
        perror("Failed to create thread.");
        close(udp_socket);
        exit(1);
    }

    printf("Available Commands: \n\n\t%%all files \t\t--Retrieve all available files\n\t%%get <file_name> \t--Get the specified file\n\t%%exit \t\t\t--Quit program\n\n");

    // Listen for user input and send commands to server
    while (1) {
	    bzero(buffer, BUFFER_SIZE);
	    fgets(buffer, BUFFER_SIZE, stdin);

	    // Remove newline character from input
	    buffer[strcspn(buffer, "\n")] = 0;

        if (strcmp(buffer, "%exit") == 0) {
            break;
        }
	
	    // Prepend hostname onto command and arguments, i.e. %all files -> LAPTOP-MZ82187 %all files
	    char hostname[BUFFER_SIZE];
	    bzero(hostname, BUFFER_SIZE);
	    gethostname(hostname, sizeof(hostname));
	    strcat(hostname, " ");
	    strcat(hostname, buffer);

	    // Send command to server
	    sendto(udp_socket, hostname, strlen(hostname), 0, (struct sockaddr *)&udp_sin, sizeof(udp_sin));
    }

    // When user exits, close separate listener thread
    pthread_cancel(thread_id);
    close(udp_socket);

    return 0;
}
