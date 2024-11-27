#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <dirent.h> // for accessing local directory
#include <unistd.h> // gethostname
#include <pthread.h>

#define UDP_PORT 5432
#define TCP_PORT 5433
#define BUFFER_SIZE 1024
#define MAX_PENDING 5

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
// This function is utilized by a separate thread that listens for incoming datagrams and
// acts on them accordingly.
void *receiveDatagrams(void *socket_desc) {
    int udp_socket = *(int *)socket_desc;
    int tcp_socket;
    char buffer[BUFFER_SIZE];
    struct sockaddr_in udp_sin;
    socklen_t addr_len = sizeof(udp_sin);
    struct sockaddr_in tcp_sin;
    socklen_t addr_len_tcp = sizeof(tcp_sin);
    int bytes_received;
    char type_flag[5]; // Buffer to hold flags
    char file_name[256]; // Buffer to hold filenames

    while (1) {
        fflush(stdout);
		bzero(type_flag, sizeof(type_flag));
        bzero(buffer, BUFFER_SIZE);

		bytes_received = recvfrom(udp_socket, (char *)buffer, BUFFER_SIZE, 0 , (struct sockaddr *)&udp_sin, &addr_len);

		if (bytes_received > 0) {
            
	    	// Check if the message is a "ping" from the server
            if (strncmp(buffer, "ping", 4) == 0) {
                
				// Send an "ack" response back to the server
                printf("\nping received\n");
				bzero(buffer, BUFFER_SIZE);
				gethostname(buffer, sizeof(buffer));
				strcat(buffer, " %ack ping"); // ACKNOWLEDGE
				sendto(udp_socket, buffer, strlen(buffer), 0, (struct sockaddr *)&udp_sin, addr_len);
            }
			// Check if the message is server response to this client's get request
			else if (strncmp(buffer, "%get", 4) == 0) {
				char filename[BUFFER_SIZE];
				char ip_addr[BUFFER_SIZE];

				/* 
				* Server response will come in the form of:
				* %get <filename> <IP address of client-who-has-the-file>
				*
				* Split off into filename and ip_addr variables for later use
				*/
				sscanf(buffer, "%%get %s %s", filename, ip_addr);
				printf("Send address received: %s\n", ip_addr);

				// Create address structure for IP address received from server
				struct in_addr addr;
				inet_pton(AF_INET, ip_addr, &addr);
				bzero((char *)&tcp_sin, sizeof(tcp_sin));
				tcp_sin.sin_family = AF_INET;
				bcopy(&addr, (char *)&tcp_sin.sin_addr, sizeof(addr));
				tcp_sin.sin_port = htons(5434); // hard-coded port of client-who-has-the-file :(

				// Create socket for client-to-client TCP connection
				if ((tcp_socket = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
					perror("Client: Sending TCP socket");
					exit(1);
				}

				// Connect to client-who-has-the-file
				if (connect(tcp_socket, (struct sockaddr *)&tcp_sin, sizeof(tcp_sin)) < 0) {
					perror("Client: Connect to Host-Client");
					close(tcp_socket);
					exit(1);
				}
				
				bzero(buffer, BUFFER_SIZE);
				strcpy(buffer, "%get ");
				strcat(buffer, filename);

				// Send get request to client-who-has-the-file: %get <filename>
				send(tcp_socket, buffer, BUFFER_SIZE, 0);
				
				// Open file for writing data received from client-who-has-the-file
				FILE *file = fopen(filename, "wb");
				if (file == NULL) {
					perror("Error opening file for writing.");
					exit(1);
				}

				// Receive file from client-who-has-the-file
				int bytes_received;
				bzero(buffer, BUFFER_SIZE);
				while((bytes_received = recv(tcp_socket, buffer, BUFFER_SIZE, 0)) > 0) {
					fwrite(buffer, sizeof(char), bytes_received, file);
				}

	    	} 
            else {
	        	printf("Available server resources: %s\n", buffer);
            }
        }
    }

    return NULL;
}

/*
 * This function is utilized by a third thread that listens for incoming connections
 * on the TCP socket (get requests) and sends back the requested file.
 */
void *fileTransfer(void *socket_desc) {
    int socket = *(int *)socket_desc;
    int tcp_socket_client;
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    int bytes_received;

    listen(socket, MAX_PENDING);

    while (1) {
		fflush(stdout);
	
		// Accept new TCP connection
		if ((tcp_socket_client = accept(socket, (struct sockaddr*)&client_addr, &addr_len)) < 0) {
			perror("Client: accept");
			continue;
		}

		bytes_received = recv(tcp_socket_client, buffer, BUFFER_SIZE, 0);

		if (bytes_received > 0) {
			// Check if message is a get request
			if (strncmp(buffer, "%get", 4) == 0) {
			
				// Get requested filename
				char filename[BUFFER_SIZE];
				sscanf(buffer, "%%get %s", filename);

				// Open requested file
				FILE *file = fopen(filename, "rb");
				if (file == NULL) {
					perror("File open failed.");
					exit(1);
				}
				
				// Send requested file
				int bytes_read;
				bzero(buffer, BUFFER_SIZE);
				while ((bytes_read = fread(buffer, sizeof(char), BUFFER_SIZE, file)) > 0) {
					if (send(tcp_socket_client, buffer, bytes_read, 0) == -1) {
						perror("Error sending file to client.");
						fclose(file);
						exit(1);
					}
				}

				fclose(file);
				printf("File '%s' sent to client\n", filename);
			} 
		}
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    FILE *fp;
    struct sockaddr_in udp_sin;
    struct sockaddr_in tcp_sin;
    struct hostent *hp;
    char *host;
    char buffer[BUFFER_SIZE];
    int udp_socket;
    int tcp_socket;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s host\n", argv[0]);
        exit(1);
    }

    host = argv[1];

    // Build address structure for TCP
    bzero((char *)&tcp_sin, sizeof(tcp_sin));
    tcp_sin.sin_family = AF_INET;
    tcp_sin.sin_addr.s_addr = INADDR_ANY;
    tcp_sin.sin_port = htons(TCP_PORT);

    // Setup passive open
    if ((tcp_socket = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Client: TCP socket");
		exit(1);
    }

    if ((bind(tcp_socket, (struct sockaddr *)&tcp_sin, sizeof(tcp_sin))) < 0) {
		perror("Client: Bind TCP socket");
		exit(1);
    }

    // Create separate thread to listen for TCP connections/get requests
    pthread_t thread_id_tcp;
    if (pthread_create(&thread_id_tcp, NULL, fileTransfer, (void *)&tcp_socket) != 0) {
		perror("Failed to create TCP thread.");
		close(tcp_socket);
		exit(1);
    }

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
    pthread_cancel(thread_id_tcp);
    pthread_cancel(thread_id);
    close(tcp_socket);
    close(udp_socket);

    return 0;
}
