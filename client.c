#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <dirent.h> // for accessing local directory
#include <unistd.h> // gethostname

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
    
    if (d) {
	while ((dir = readdir(d)) != NULL) {
	    if (strstr(dir->d_name, ".txt")) {
		strcat(buffer, ",");
		strcat(buffer, dir->d_name);
	    }
	}

	closedir(d);
    }
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
    registerLocalFiles(buffer);
    printf("%s\n", buffer);
    sendto(udp_socket, buffer, strlen(buffer), 0, (struct sockaddr *)&udp_sin, sizeof(udp_sin));

    while (1) {
	
    }
}
