#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "interface.h"

struct Room {
	char name[10];
	int sockfd;
	int members;
	int port;
};

struct Room rooms[256];
int active_rooms = 0;
int PORT = 0;


void *process_command(void *s);
void run_room(int room_num);
void create_room(int port, char *name);


int main(int argc, char** argv) 
{
	PORT = atoi(argv[1]);
	int server_fd, new_socket, valread;
	struct sockaddr_in address;
	int opt = 1;
	int addrlen = sizeof(address);
	
	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd == 0) {
		perror("socket()");
		exit(EXIT_FAILURE);
	}
	
	//if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
	//	perror("setsocketopt()");
	//	exit(EXIT_FAILURE);
//	}
	
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(PORT);
	
	if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
		perror("bind()");
		exit(EXIT_FAILURE);
	}
	
	listen(server_fd, 10);
	
	while (1) {
		new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
		if (new_socket < 0) {
			perror("accept()");
			exit(EXIT_FAILURE);
		}
		
		printf("new connection in main()\n");
		
		pthread_t thread_id;
		void *s = &new_socket;
		pthread_create(&thread_id, NULL, process_command, s);
		//pthread_join(thread_id, NULL);
	}
	
	close(server_fd);
	
    return 0;
}


void *process_command(void *s) {
	int sockfd = *(int *)s;
	char buffer[MAX_DATA], command[MAX_DATA], param[MAX_DATA];
	struct Reply reply;
	
	read(sockfd, buffer, MAX_DATA);
	
	printf("%s\n", buffer);
	
	int pos = -1;
	for (int i = 0; i < MAX_DATA; ++i) {
		if (buffer[i] == ' ') {
			pos = i+1;
		}
		else if (pos == -1) {
			command[i] = buffer[i];
		}
		else {
			param[i - pos] = buffer[i];
		}
	}
	
	
	if (strncmp(command, "CREATE", 6) == 0) {
		int exists = 0;
		printf("%d\n", active_rooms);
		for (int i = 0; i < active_rooms; ++i) {
			printf("%s\n", rooms[i].name);
			if (strncmp(rooms[i].name, param, 10) == 0) {
				reply.status = FAILURE_ALREADY_EXISTS;
				send(sockfd, (char *)&reply, sizeof(struct Reply), 0);
				exists = 1;
				break;
			}
		}
		
		if (exists == 0) {
			reply.port = active_rooms + PORT + 1;
			
			reply.num_member = 0;
			reply.status = SUCCESS;
			send(sockfd, (char *)&reply, sizeof(struct Reply), 0);
			
			printf("Closing temp socket\n");
			close(sockfd);
			
			create_room(reply.port, param);
			
		}
	}
	else if (strncmp(command, "JOIN", 4) == 0) {
		int exists = 0;
		for (int i = 0; i < active_rooms; ++i) {
			if (strncmp(rooms[i].name, param, 10) == 0) {
				rooms[i].members++;
				
				reply.status = SUCCESS;
				reply.port = rooms[i].port;
				reply.num_member = rooms[i].members;
				
				exists = 1;
				break;
			}
		}
		
		if (exists == 0) {
			reply.status = FAILURE_NOT_EXISTS;
		}
		
		send(sockfd, (char *)&reply, sizeof(struct Reply), 0);
		
		printf("Closing temp socket\n");
		close(sockfd);
	}
	else if (strncmp(command, "DELETE", 6) == 0) {
		
	}
	else {
		
	}
	
	
	pthread_detach(pthread_self());
	
}

void create_room(int port, char *name) {
	int listen_fd;
	struct sockaddr_in address;
	int opt = 1;
	int addrlen = sizeof(address);
	
	listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_fd == 0) {
		perror("socket()");
		exit(EXIT_FAILURE);
	}
	
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(port);
	
	if (bind(listen_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
		perror("bind()");
		exit(EXIT_FAILURE);
	}
	
	printf("%d\n", listen_fd);
	printf("%d\n", port);
	
	rooms[active_rooms].sockfd = listen_fd;
	rooms[active_rooms].port = port;
	rooms[active_rooms].members = 0;
	strcpy(rooms[active_rooms].name, name);
	
	active_rooms++;
	
	char buffer[MAX_DATA];

	int client_socks[10];  /* list of connected client sockets, initially empty */
	int retval, n_clients = 0;
	
	listen(listen_fd, 5);
	
	while(1) {
	    int max_sock = -1;
	    fd_set sock_set;
	    FD_ZERO(&sock_set);
	    FD_SET(listen_fd, &sock_set);  // listenSock will be ready-for-ready when it's time to accept()
	    
	    if (listen_fd > max_sock) {
	    	max_sock = listen_fd;
	    }
	
	    for (int i = 0; i < 10; ++i) {
	    	FD_SET(client_socks[i], &sock_set);
	    	if (client_socks[i] > max_sock) {
	       		max_sock = client_socks[i];
	    	}
	    }
	
	    struct timeval timeout;
	    timeout.tv_sec = 1;
    	timeout.tv_usec = 0;
    	
	    retval = select(max_sock+1, &sock_set, NULL, NULL, &timeout);
	    if(retval >= 0)
	    {
	        if(FD_ISSET(listen_fd, &sock_set))
	        {
	            printf("Client requesting to connect to room...\n");
	            
	            int new_fd = accept(listen_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
	            
	            if (new_fd >= 0) {
	            	client_socks[n_clients] = new_fd;
	            	n_clients++;
	            }
	            else {
	            	perror("accept");
	            }
	        }
	
	        // iterate backwards in case we need to remove a disconnected client socket
	        for (int i = n_clients; i >= 0; --i) {
	        	if (FD_ISSET(client_socks[i], &sock_set)) {
	        		printf("TCP data incoming from socket #%i...\n", client_socks[i]);
	               
	            	int n_bytes = recv(client_socks[i], buffer, sizeof(buffer), 0);
					if (n_bytes > 0) {
						printf("%s\n", buffer);
					}
					else {
		            	if (n_bytes == 0) {
		            		printf("Client with socket #%i disconnected.\n", client_socks[i]);
		            	}
		                else {
		                	perror("recv(TCP)");
		                }
		                
		                close(client_socks[i]);
		                client_socks[i] = -1; // remove closed socket at position i
	            	}
	        	}
	        }
	    }
	    else {
	    	perror("select");
	    }
	}
	active_rooms--;
	
	close(listen_fd);
}