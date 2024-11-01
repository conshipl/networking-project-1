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
    char username[50];
    in_addr ip_address;
    char status[15]; 
};

struct Resource {
    char owner_name[50];
    char resource_name[50];
    char status[15];
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

void displayUserTable(struct User user_table[MAX_USERS], int user_count) {
    printf("\nUser Name\t\tStatus\n");
    for (int i = 0; i < user_count; ++i) {
        printf("%s\t\t\t%s\n", user_table[i].username, user_table[i].status); 
    }
    fflush(stdout);   
}

void displayResourceTable(struct Resource resource_table[MAX_RESOURCES], int resource_count) {
    printf("\nResource Name\t\tResource Owner\t\tStatus\n");
    for (int j = 0; j < resource_count; ++j) {
    printf("%s\t\t%s\t\t\t%s\n", resource_table[j].resource_name, resource_table[j].owner_name, resource_table[j].status);
    }
    fflush(stdout);
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
	// Expected format: <hostname> <command> <argument1>,<argument2>,...
	// Example: LAPTOP-XJ10456 %cnct text1.txt,text2.txt,
	processDatagram(buffer, username, command, arguments);
	
	// Determine datagram command
	// If new connection
	if (strncmp(command, "%cnct", 5) == 0) {
            
	    // Add user to user_table
	    if (user_count < MAX_USERS) {
                strcpy(user_table[user_count].username, username);
		user_table[user_count].ip_address = udp_sin.sin_addr;
		char c[] = "connected";
		strcpy(user_table[user_count].status, c);
		++user_count;

		// If user has files, add them to resource_table
		if (arguments != NULL) {
		    char* token = strtok(arguments, ",");

		    while (token != NULL) {
			
			if (resource_count < MAX_RESOURCES) {
                    	    strcpy(resource_table[resource_count].owner_name, username);
		    	    strcpy(resource_table[resource_count].resource_name, token);
		    	    char a[] = "active";
		    	    strcpy(resource_table[resource_count].status, a);
		    	    ++resource_count;
			}

			token = strtok(NULL, ",");		
		    }
		}
	    }

	    displayUserTable(user_table, user_count);
	    displayResourceTable(resource_table, resource_count);
	}
	// If user wants to retrieve all available files
	else if (strncmp(command, "%all", 4) == 0) {
	    bzero(buffer, BUFFER_SIZE);
	    strcat(buffer, "%all ");

	    // Iterate through resource table and add each one to buffer
	    for (int k = 0; k < resource_count; ++k) {
		strcat(buffer, resource_table[k].resource_name);
		strcat(buffer, ",");
	    }

	    // Send user datagram with message of the form: %all text1.txt,text2.txt,test3.txt, 
	    sendto(udp_socket, buffer, strlen(buffer), 0, (struct sockaddr *)&udp_sin, sizeof(udp_sin));
	}
	else {
	    printf("Invalid command.");
	    fflush(stdout);
	}
    }
}
