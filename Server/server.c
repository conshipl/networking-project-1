#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define UDP_PORT 5432
#define BUFFER_SIZE 1024
#define MAX_USERS 10
#define MAX_RESOURCES 100

struct User {
    char *username;
    in_addr ip_address;
    char *status; 
};

struct Resource {
    char *owner_name;
    char *resource_name;
    char *status;
};

void processDatagram(char* buffer, char* username, char* command, char* arguments) {
    bzero(username, sizeof(username));
    bzero(command, sizeof(command));
    bzero(arguments, sizeof(arguments));
	
    char* token = strtok(buffer, " ");
    strcpy(username, token);

    token = strtok(NULL, " ");
    strcpy(command, token);

    if (token != NULL) {
      token = strtok(NULL, " ");
      strcpy(arguments, token);
    }
}

int main(int argc, char *argv[]) {
    FILE *fp;
    struct sockaddr_in udp_sin;
    socklen_t addr_len = sizeof(udp_sin);
    struct hostent *hp;
    char username[50];
    char command[5];
    char arguments[200];
    char buffer[BUFFER_SIZE];
    int udp_socket;
    
    struct User user_table[MAX_USERS];
    int user_count = 0;
    struct Resource resource_table[MAX_RESOURCES];
    int resource_count = 0;


    // Create UDP socket
    if ((udp_socket = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Server: UDP socket");
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
	// Receive datagram
	bzero(buffer, BUFFER_SIZE);
	recvfrom(udp_socket, (char *)buffer, BUFFER_SIZE, 0, (struct sockaddr *)&udp_sin, &addr_len);
	
	printf("%s\n", buffer);
	fflush(stdout);
	
	// Split datagram into parts
	processDatagram(buffer, username, command, arguments);
	
	// Determine datagram command
	// If new connection
	if (strncmp(command, "%cnct", 5) == 0) {
            
	    if (user_count < MAX_USERS) {
                user_table[user_count].username = username;
		user_table[user_count].ip_address = udp_sin.sin_addr;
		char c[] = "connected";
		user_table[user_count].status = c;
		++user_count;

		if (resource_count < MAX_RESOURCES) {
                    resource_table[resource_count].owner_name = username;
		    char s[] = "something";
		    resource_table[resource_count].resource_name = s;
		    char a[] = "active";
		    resource_table[resource_count].status = a;
		    ++resource_count;
		}
	    }

	    printf("\nUser Name\t\tStatus\n");
            for (int i = 0; i < user_count; ++i) {
                printf("%s\t\t\t%s\n", user_table[i].username, user_table[i].status); 
    	    }
            fflush(stdout);

	    printf("\nResource Name\t\tResource Owner\t\tStatus\n");
	    for (int j = 0; j < resource_count; ++j) {
		printf("%s\t\t%s\t\t\t%s\n", resource_table[j].resource_name, resource_table[j].owner_name, resource_table[j].status);
	    }
	    fflush(stdout);
	}
    }
}
