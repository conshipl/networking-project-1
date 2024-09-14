#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/stat.h> // Added this header for struct stat

#define SERVER_PORT 5432

#define BUFFER_SIZE 256

/*
i can add more comments to explain the code after i get some sleep
*/

void send_file(FILE *fp, int s, char* file_name);
void receive_file(FILE *fp, int s);

int main(int argc, char * argv[])
{
	FILE *fp;
	int s;
    struct sockaddr_in sin;
	struct hostent *hp;
    char buffer[BUFFER_SIZE];
	char *host;
	char command[5];
	char file_name[256];
	int bytes_read;
	struct stat file_stat;
	uint32_t file_size;

	if (argc != 2) 
	{
		fprintf(stderr, "Usage: %s host\n", argv[0]);
        exit(1);
	}

	host = argv[1];
	
	/* translate host name into peer's IP address */
	hp = gethostbyname(host);
	
	if (!hp) 
	{
		fprintf(stderr, "Client: unknown host: %s\n", host);
		exit(1);
	}
	
	/* build address data structure */
	bzero((char *)&sin, sizeof(sin));
	sin.sin_family = AF_INET;
	bcopy(hp->h_addr, (char *)&sin.sin_addr, hp->h_length);
	sin.sin_port = htons(SERVER_PORT);

	/* active open */
	if ((s = socket(PF_INET, SOCK_STREAM, 0)) < 0) 
	{
		perror("Client: socket");
		exit(1);
	}

	if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0)
	{
		perror("Client: connect");
		close(s);
		exit(1);
	}

	while (1) {
        // Get user input from command line
        printf("Enter command: ");
        memset(buffer, 0, BUFFER_SIZE);
        fgets(buffer, BUFFER_SIZE, stdin);

        // Remove newline character from input
        buffer[strcspn(buffer, "\n")] = 0;

		if (strcmp(buffer, "exit") == 0) {
            break;
        }

		 if (sscanf(buffer, "%s %s", command, file_name) != 2) {
            fprintf(stderr, "Invalid input format. Please enter in the format: <command> <file_name>\n");
            continue;
        }

		// Send command to the server
        snprintf(buffer, BUFFER_SIZE, "%s %s", command, file_name);
        send(s, buffer, strlen(buffer), 0);
        
		// GET
		if (strcmp(command, "get") == 0) {
			fp = fopen(file_name, "wb");
			if (fp == NULL) {
				perror("File open error");
				continue;
			}
			// Receive file from server
			receive_file(fp, s);
			printf("File received: %s\n", file_name);
			fclose(fp);

		// PUT
		} else if (strcmp(command, "put") == 0) {
			// Open the file to send
			fp = fopen(file_name, "rb");
			if (fp == NULL) {
				perror("File open error");
				continue;
			}

			//send_file(fp, s, file_name);

			// Get file size
			if (stat(file_name, &file_stat) < 0) {
				perror("stat");
				fclose(fp);
				close(s);
				return;
			}

			file_size = htonl(file_stat.st_size);

			// Send file size
			if (send(s, &file_size, sizeof(file_size), 0) == -1) {
				perror("Error sending file size");
				fclose(fp);
				close(s);
				return;
			}

			// Send file data to server
			while ((bytes_read = fread(buffer, sizeof(char), BUFFER_SIZE, fp)) > 0) {
				if (send(s, buffer, bytes_read, 0) == -1) {
					perror("Error sending file");
					fclose(fp);
					close(s);
				}
			}
			printf("File '%s' sent to server.\n", file_name);
			fclose(fp);


		} else {
			printf("Unknown command.\n");
		}
	}

	// Close the socket
	close(s);
	return 0;
}

// Function to send file to server (not used at the moment)
void send_file(FILE *fp, int s, char* file_name) {
	struct stat file_stat;
    char buffer[BUFFER_SIZE];
	int bytes_read;
	uint32_t file_size;

	// Get file size
	if (stat(file_name, &file_stat) < 0) {
		perror("stat");
		fclose(fp);
		close(s);
		return;
	}
	file_size = htonl(file_stat.st_size);

	// Send file size
	if (send(s, &file_size, sizeof(file_size), 0) == -1) {
		perror("Error sending file size");
		fclose(fp);
		close(s);
		return;
	}

	// Send file data to server
	while ((bytes_read = fread(buffer, sizeof(char), BUFFER_SIZE, fp)) > 0) {
		if (send(s, buffer, bytes_read, 0) == -1) {
			perror("Error sending file");
			fclose(fp);
			close(s);
		}
	}
	printf("File '%s' sent to server.\n", file_name);
	fclose(fp);
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
