#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define SERVER_PORT 5432

#define MAX_LINE 256

int main(int argc, char * argv[])
{
	FILE *fp;
	struct hostent *hp;
	struct sockaddr_in sin;
	char *host;
	char command_buf[MAX_LINE]; // dedicated char buffer for commands
	char file_buf[MAX_LINE]; // dedicated char buff for file transfer
	int command_socket;
	int file_socket;
	int len;

	if (argc==2) 
	{
		host = argv[1];
	}
	
	else 
	{
		fprintf(stderr, "usage: simplex-talk host\n");
		exit(1);
	}
	
	/* translate host name into peer's IP address */
	hp = gethostbyname(host);
	
	if (!hp) 
	{
		fprintf(stderr, "simplex-talk: unknown host: %s\n", host);
		exit(1);
	}
	
	/* build address data structure */
	bzero((char *)&sin, sizeof(sin));
	sin.sin_family = AF_INET;
	bcopy(hp->h_addr, (char *)&sin.sin_addr, hp->h_length);
	sin.sin_port = htons(SERVER_PORT);

	/* active open */
	if ((command_socket = socket(PF_INET, SOCK_STREAM, 0)) < 0) 
	{
		perror("simplex-talk: socket");
		exit(1);
	}

	// connect to server
	if (connect(command_socket, (struct sockaddr *)&sin, sizeof(sin)) < 0)
	{
		perror("simplex-talk: connect");
		close(command_socket);
		exit(1);
	}

	// while the user continues to enter commands
	while (fgets(command_buf, sizeof(command_buf), stdin)) // stores user input into command buffer
	{
		// strip the newline off the end of the user input
		command_buf[strcspn(command_buf, "\n")] = 0;

		// send the command to the server
		send(command_socket, command_buf, sizeof(command_buf), 0);

		if (strcmp(command_buf, "EXIT") == 0)
		{
			printf("Exiting client program.");
			break;
		}
		
		int counter = 0;
        	char* token = strtok(command_buf, " ");
		char* command;
		char* fileName;
        
		/*
		 * The command will be in the form of 'get fileName' or 'put fileName'
		 * This will separate the two pieces of the command and store them in
		 * their appropriate variable
		 */
        	while(token != NULL && counter < 2)
        	{
            		if (counter == 0)
			{
				command = token;
			}
			else
			{
				fileName = token;
			}
            		
			token = strtok(NULL, " ");
            		counter++;
        	}
        
		// determine which command the user input
		if (strcmp(command, "get") == 0)
        	{
            		fp = fopen(fileName, "w");
			
			// open file socket
			if ((file_socket = socket(PF_INET, SOCK_STREAM, 0)) < 0)
        		{
                		perror("simplex-talk: socket");
                		exit(1);
        		}
			
			// connect to server
			if (connect(file_socket, (struct sockaddr *)&sin, sizeof(sin)) < 0)
			{
				perror("simplex-talk:connect");
				close(file_socket);
				exit(1);
			}

			// while the server continues sending data
			while((len = recv(file_socket, file_buf, sizeof(file_buf), 0))) // store the info in the file buffer
			{
				printf("%s", file_buf);
				fprintf(fp, "%s", file_buf); // write the file buffer to the file
				bzero(file_buf, MAX_LINE); // clear buffer for next receive
			}

			close(file_socket);
			fclose(fp);
        	}
        	else if (strcmp(command, "put") == 0)
        	{
        		fp = fopen(fileName, "r");
		
			if ((file_socket = socket(PF_INET, SOCK_STREAM, 0)) < 0)
        		{
                		perror("simplex-talk: socket");
                		exit(1);
        		}

        		if (connect(file_socket, (struct sockaddr *)&sin, sizeof(sin)) < 0)
        		{
                		perror("simplex-talk: connect");
                		close(file_socket);
                		exit(1);
        		}

			// while the client has more data to send from the file
			while (fgets(file_buf, MAX_LINE, fp)) // store the info in the file buffer
        		{
                		printf("%s", file_buf);
				send(file_socket, file_buf, sizeof(file_buf), 0); // send the info to the server
                		bzero(file_buf, MAX_LINE);
        		}

        		close(file_socket);
        		fclose(fp);
        	}
        	else
        	{
            	printf("Unknown command\n");
        	}

		// clear command buffer for next command input
        	bzero(command_buf, 256);
		
	}
	
	// user has enter exit, close socket
	close(command_socket);
}
