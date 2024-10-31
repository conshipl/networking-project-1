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

void *receiveDatagrams(void *socket_desc) {
    int socket = *(int *)socket_desc;
    char buffer[BUFFER_SIZE];
    int bytes_received;
    char type_flag[5]; // Buffer to hold flags
    char file_name[256]; // Buffer to hold filenames

    while (1) {
	fflush(stdout);
	bzero(type_flag, sizeof(type_flag));

	//bytes_received = recvfrom(socket, (char *)buffer, BUFFER_SIZE, 0, 
	break;
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

    printf("Available Commands: \n\n\t%%files \t\t\t--Retrieve all available files\n\t%%get <file_name> \t--Get the specified file\n\t%%exit \t\t\t--Quit program\n\n");

    // Listen for user input and send commands to server
    while (1) {
	bzero(buffer, BUFFER_SIZE);
	fgets(buffer, BUFFER_SIZE, stdin);

	// Remove newline character from input
	buffer[strcspn(buffer, "\n")] = 0;

	if (strcmp(buffer, "%exit") == 0) {
	    break;
	}

	sendto(udp_socket, buffer, strlen(buffer), 0, (struct sockaddr *)&udp_sin, sizeof(udp_sin));
    }

    pthread_cancel(thread_id);
    close(udp_socket);

    return 0;
}
