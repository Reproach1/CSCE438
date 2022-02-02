#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/select.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "interface.h"

int quit_flag = 0;

void str_overwrite_stdout() {
  printf("%s", "> ");
  fflush(stdout);
}

void str_trim_lf (char* arr, int length) {
  int i;
  for (i = 0; i < length; i++) { // trim \n
    if (arr[i] == '\n') {
      arr[i] = '\0';
      break;
    }
  }
}

/*
 * TODO: IMPLEMENT BELOW THREE FUNCTIONS
 */
int connect_to(const char *host, const int port);
struct Reply process_command(const int sockfd, char* command);
void process_chatmode(const char* host, const int port);
void* send_messages(void* s);
void* read_messages(void* s);

int main(int argc, char** argv) 
{
	if (argc != 3) {
		fprintf(stderr,
				"usage: enter host address and port number\n");
		exit(1);
	}

    display_title();
    
	while (1) {
	
		int sockfd = connect_to(argv[1], atoi(argv[2]));
    
		char command[MAX_DATA];
        get_command(command, MAX_DATA);
        
        printf("%s\n", command);

		struct Reply reply = process_command(sockfd, command);
		display_reply(command, reply);
		
		touppercase(command, strlen(command) - 1);
		if (strncmp(command, "JOIN", 4) == 0 && reply.status == SUCCESS) {
			printf("Now you are in the chatmode\n");
			process_chatmode(argv[1], reply.port);
		}
	
		close(sockfd);
    }

    return 0;
}

/*
 * Connect to the server using given host and port information
 *
 * @parameter host    host address given by command line argument
 * @parameter port    port given by command line argument
 * 
 * @return socket fildescriptor
 */
int connect_to(const char *host, const int port)
{
	// ------------------------------------------------------------
	// GUIDE :
	// In this function, you are suppose to connect to the server.
	// After connection is established, you are ready to send or
	// receive the message to/from the server.
	// 
	// Finally, you should return the socket fildescriptor
	// so that other functions such as "process_command" can use it
	// ------------------------------------------------------------

    // below is just dummy code for compilation, remove it.
	int sockfd = -1;
	struct sockaddr_in server_addr;
	
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		perror("socket()");
		exit(EXIT_FAILURE);
	}
	
	server_addr.sin_addr.s_addr = inet_addr(host);
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	
	if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
		perror("connect()");
		exit(EXIT_FAILURE);
	}
	
	return sockfd;
}

/* 
 * Send an input command to the server and return the result
 *
 * @parameter sockfd   socket file descriptor to commnunicate
 *                     with the server
 * @parameter command  command will be sent to the server
 *
 * @return    Reply    
 */
struct Reply process_command(const int sockfd, char* command)
{
	// ------------------------------------------------------------
	// GUIDE 1:
	// In this function, you are supposed to parse a given command
	// and create your own message in order to communicate with
	// the server. Surely, you can use the input command without
	// any changes if your server understand it. The given command
    // will be one of the followings:
	//
	// CREATE <name>
	// DELETE <name>
	// JOIN <name>
    // LIST
	//
	// -  "<name>" is a chatroom name that you want to create, delete,
	// or join.
	// 
	// - CREATE/DELETE/JOIN and "<name>" are separated by one space.
	// ------------------------------------------------------------


	// ------------------------------------------------------------
	// GUIDE 2:
	// After you create the message, you need to send it to the
	// server and receive a result from the server.
	// ------------------------------------------------------------


	// ------------------------------------------------------------
	// GUIDE 3:
	// Then, you should create a variable of Reply structure
	// provided by the interface and initialize it according to
	// the result.
	//
	// For example, if a given command is "JOIN room1"
	// and the server successfully created the chatroom,
	// the server will reply a message including information about
	// success/failure, the number of members and port number.
	// By using this information, you should set the Reply variable.
	// the variable will be set as following:
	//
	// Reply reply;
	// reply.status = SUCCESS;
	// reply.num_member = number;
	// reply.port = port;
	// 
	// "number" and "port" variables are just an integer variable
	// and can be initialized using the message fomr the server.
	//
	// For another example, if a given command is "CREATE room1"
	// and the server failed to create the chatroom becuase it
	// already exists, the Reply varible will be set as following:
	//
	// Reply reply;
	// reply.status = FAILURE_ALREADY_EXISTS;
    // 
    // For the "LIST" command,
    // You are suppose to copy the list of chatroom to the list_room
    // variable. Each room name should be seperated by comma ','.
    // For example, if given command is "LIST", the Reply variable
    // will be set as following.
    //
    // Reply reply;
    // reply.status = SUCCESS;
    // strcpy(reply.list_room, list);
    // 
    // "list" is a string that contains a list of chat rooms such 
    // as "r1,r2,r3,"
	// ------------------------------------------------------------

	// REMOVE below code and write your own Reply.
	struct Reply *reply;
	
	char buffer[sizeof(struct Reply)];
	
	send(sockfd, command, MAX_DATA, 0);
	
	read(sockfd, buffer, sizeof(struct Reply));
	
	reply = (struct Reply*)buffer;
	
	return *reply;
}

/* 
 * Get into the chat mode
 * 
 * @parameter host     host address
 * @parameter port     port
 */
void process_chatmode(const char* host, const int port)
{
	// ------------------------------------------------------------
	// GUIDE 1:
	// In order to join the chatroom, you are supposed to connect
	// to the server using host and port.
	// You may re-use the function "connect_to".
	// ------------------------------------------------------------

	// ------------------------------------------------------------
	// GUIDE 2:
	// Once the client have been connected to the server, we need
	// to get a message from the user and send it to server.
	// At the same time, the client should wait for a message from
	// the server.
	// ------------------------------------------------------------
	
    // ------------------------------------------------------------
    // IMPORTANT NOTICE:
    // 1. To get a message from a user, you should use a function
    // "void get_message(char*, int);" in the interface.h file
    // 
    // 2. To print the messages from other members, you should use
    // the function "void display_message(char*)" in the interface.h
    //
    // 3. Once a user entered to one of chatrooms, there is no way
    //    to command mode where the user  enter other commands
    //    such as CREATE,DELETE,LIST.
    //    Don't have to worry about this situation, and you can 
    //    terminate the client program by pressing CTRL-C (SIGINT)
	// ------------------------------------------------------------
	char buffer[MAX_DATA];
	
	int fd = connect_to(host, port);
	
	pthread_t read_thread;
	pthread_create(&read_thread, NULL, read_messages, (void*)&fd);
	
	pthread_t send_thread;
	pthread_create(&send_thread, NULL, send_messages, (void*)&fd);
	
	while (1) {
		if (quit_flag == 1) {
			break;
		}
		sleep(.5);
	}
	
	close(fd);

}

void *send_messages(void* s) {
	int sockfd = *(int *)s;
	
	char buffer[MAX_DATA];
	
	while (1) {
		//get_message(buffer, MAX_DATA);
		
		fflush(stdout);
		get_message(buffer, MAX_DATA);
		
		printf("sending message\n");
		send(sockfd, buffer, strlen(buffer), 0);
		
		//read(sockfd, buffer, MAX_DATA);
		
		//printf("%s\n", buffer);
		
		if (strncmp(buffer, "quit", 4) == 0) {
			quit_flag = 1;
			break;
		}
	
		bzero(buffer, MAX_DATA);
	}
}

void *read_messages(void* s) {
	int sockfd = *(int *)s;
	int r;
	
	char buffer[MAX_DATA];
	
	fd_set sockset;
	struct timeval timeout;
	int retval;

	while (1) {
	    FD_ZERO(&sockset);
	    FD_SET(sockfd, &sockset);
	
	    timeout.tv_sec = 1;
	    timeout.tv_usec = 0;
	
	    retval = select(sockfd+1, &sockset, NULL, NULL, &timeout);
	    if (retval > 0)
	    {
	        if (FD_ISSET(sockfd, &sockset))
	        {
	            printf("reading\n");
				r = read(sockfd, buffer, MAX_DATA);
				if (r > 0) {
					display_message(buffer);
		    		fflush(stdout);
				}
				else if (r == 0) {
					break;
				}
				else {
					memset(buffer, 0, MAX_DATA);	
				}
	        }
	    }
	    else if (retval < 0)
	    {
	        // check errno/WSAGetLastError(), call perror(), etc ...
	    }
	}
}

