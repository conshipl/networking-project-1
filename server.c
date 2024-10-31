#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define UDP_PORT 5432
#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    FILE *fp;
    struct sockaddr_in udp_sin;
    socklen_t addr_len = sizeof(udp_sin);
    struct hostent *hp;
    char *host;
    char buffer[BUFFER_SIZE];
    int udp_socket;

    // Create UDP socket
    if ((udp_socket = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Client: UDP socket");
        exit(1);
    }

    // Set up UDP socket address structure
    bzero((char *)&udp_sin, sizeof(udp_sin));
    udp_sin.sin_family = AF_INET;
    udp_sin.sin_addr.s_addr = INADDR_ANY;
    udp_sin.sin_port = htons(UDP_PORT);

    // Bind the UDP socket
    if (bind(udp_socket, (struct sockaddr *)&udp_sin, sizeof(udp_sin)) < 0) {
        perror("Server: UDP bind");
	exit(1);
    }

    while (1) {
	bzero(buffer, BUFFER_SIZE);
	recvfrom(udp_socket, (char *)buffer, BUFFER_SIZE, 0, (struct sockaddr *)&udp_sin, &addr_len);
	printf("%s\n", buffer);
    }
}
