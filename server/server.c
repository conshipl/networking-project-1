#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h> 
#include <sys/stat.h> // Added this header for struct stat

#define SERVER_PORT 5432
#define BUFFER_SIZE 1024
#define MAX_PENDING 5 // Defines the maximum length for the queue of pending connections
#define MAX_LINE 256

void handle_client(int client_sock);

int main() {
	struct sockaddr_in sin;
	struct sockaddr_in client_addr;
	int len;
	int s, client_sock; // s: server socket, client_sock: client socket
	socklen_t addr_len = sizeof(client_addr);
	pid_t child_pid;
	char buf[MAX_LINE];
	
	/* build address data structure */
	bzero((char *)&sin, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons(SERVER_PORT);
	
	/* setup passive open */
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
	fflush(stdout);

	while(1) {
		printf("Waiting for connection...\n");
		fflush(stdout);

		if ((client_sock = accept(s, (struct sockaddr*)&client_addr, &addr_len)) < 0) {
			perror("Server: accept");
			continue;
		}

		child_pid = fork(); // Create child process to handle a client, this enables simultaneous connections
		if (child_pid == 0) { // Child process
			close(s); // Child doesn't need the listener socket
			handle_client(client_sock);
			exit(1);
		} else if (child_pid > 0) { // Parent process
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

void handle_client(int client_sock) {
	FILE *file;
	char buffer[BUFFER_SIZE];
	char command[5];
	char file_name[256];
	int bytes_read;
	struct stat file_stat;
	uint32_t file_size;

	while (1) {
		memset(buffer, 0, BUFFER_SIZE);
		bytes_read = recv(client_sock, buffer, BUFFER_SIZE, 0); // Gets bytes read from receiving data from client

		if (bytes_read <= 0) {
			perror("recv");
			break;
		}

		buffer[bytes_read] = '\0'; // Null-terminate the command string

		printf("Received command: %s\n", buffer);

		// Parse command
		sscanf(buffer, "%s %s", command, file_name);

		// PUT
		if (strcmp(command, "put") == 0) { // Client wants to upload a file (PUT)
			file = fopen(file_name, "wb"); // Open file for writing in binary mode
			if (file == NULL) {
				perror("File open failed in put");
				close(client_sock);
				break;
			}
			printf("File '%s' opened for writing\n", file_name);

			if (recv(client_sock, &file_size, sizeof(file_size), 0) <= 0) { // Receives the file size before the file data
				perror("Error receiving file size");
				fclose(file);
				break;
			}
			file_size = ntohl(file_size); // Convert to host byte order
			printf("Receiving file of size %u bytes\n", file_size);

			// Receive file data from client
			while (file_size > 0) {
				bytes_read = recv(client_sock, buffer, BUFFER_SIZE, 0);
				if (bytes_read <= 0) break;
				fwrite(buffer, sizeof(char), bytes_read, file); // Write file data into a file created on the server
				file_size -= bytes_read;
			}
			fclose(file);
			printf("File '%s' received from client\n", file_name);
		
		} 
			
		// GET
		else if (strcmp(command, "get") == 0) { // Client wants to download a file (GET)
			file = fopen(file_name, "rb"); // Open file for reading in binary mode
			if (file == NULL) {
				perror("File open failed in get");
				close(client_sock);
				exit(1);
			}

			// Get file size
			if (stat(file_name, &file_stat) < 0) {
				perror("stat");
				fclose(file);
				close(client_sock);
				return;
			}
			file_size = htonl(file_stat.st_size);

			// Send file size
			if (send(client_sock, &file_size, sizeof(file_size), 0) == -1) {
				perror("Error sending file size");
				fclose(file);
				close(client_sock);
				return;
			}

			// Send file data to client
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
			// Unknown command
			printf("Unknown command received: %s\n", command);
		}
	}

	// Clear buffer and command for the next iteration
	memset(buffer, 0, sizeof(buffer));
	memset(command, 0, sizeof(command));
	memset(file_name, 0, sizeof(file_name));

    close(client_sock);
    printf("A client disconnected\n");
}