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

#define MAX_CLIENTS 10
#define MAX_ROOMS 30

struct Room {
	char name[10];
	int sockfd;
	int members;
	int port;
};

struct Room rooms[MAX_ROOMS];
int active_rooms = 0;
int PORT = 0;


void *process_command(void *s);
void run_room(int room_num);
void create_room(int port, char *name);


int main(int argc, char** argv) 
{
	
	PORT = atoi(argv[1]);
	
	for (int i = 0; i < MAX_ROOMS; ++i) {
		rooms[i].members = -1;
	}
	
	int server_fd, new_socket, valread;
	struct sockaddr_in address;
	int opt = 1;
	int addrlen = sizeof(address);
	
	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd == 0) {
		perror("socket()");
		exit(EXIT_FAILURE);
	}
	
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
		
		pthread_t thread_id;
		void *s = &new_socket;
		pthread_create(&thread_id, NULL, process_command, s);
	}
	
	close(server_fd);
	
    return 0;
}


void *process_command(void *s) {
	int sockfd = *(int *)s;
	char buffer[MAX_DATA], command[MAX_DATA], param[MAX_DATA];
	struct Reply reply;
	
	int make_room = 0;
	
	memset(buffer, '\0', MAX_DATA);
	memset(command, '\0', MAX_DATA);
	memset(param, '\0', MAX_DATA);
	
	read(sockfd, buffer, MAX_DATA);
	
	int pos = -1;
	for (int i = 0; i < strlen(buffer); ++i) {
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
	
	touppercase(command, strlen(command));
	
	if (strcmp(command, "CREATE") == 0 && strlen(param) > 0) {
		int exists = 0;
		printf("Active rooms: %d\n", active_rooms);
		for (int i = 0; i < MAX_ROOMS; ++i) {
			if (strcmp(rooms[i].name, param) == 0 && rooms[i].members != -1) {
				reply.status = FAILURE_ALREADY_EXISTS;
				exists = 1;
				break;
			}
		}
		
		if (exists == 0) {
			reply.port = PORT + 1;
			PORT++;
			
			reply.num_member = 0;
			reply.status = SUCCESS;
			
			make_room = 1;
		}
	}
	else if (strcmp(command, "JOIN") == 0 && strlen(param) > 0) {
		int exists = 0;
		for (int i = 0; i < MAX_ROOMS; ++i) {
			if (strcmp(rooms[i].name, param) == 0 && rooms[i].members != -1) {
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
	}
	else if (strcmp(command, "DELETE") == 0 && strlen(param) > 0) {
		int exists = 0;
		for (int i = 0; i < MAX_ROOMS; ++i) {
			if (strcmp(rooms[i].name, param) == 0 && rooms[i].members != -1) {
				shutdown(rooms[i].sockfd, SHUT_RD);
				
				reply.status = SUCCESS;
				
				exists = 1;
				break;
			}
		}
		
		if (exists == 0) {
			reply.status = FAILURE_NOT_EXISTS;
		}
		
	}
	else if (strcmp(command, "LIST") == 0) {
		int length = 0;
		
		memset(buffer, '\0', MAX_DATA);
		
		if (active_rooms > 0) {
			for (int i = MAX_ROOMS - 1; i >= 0; --i) {
				if (rooms[i].members != -1) {
					if (strlen(buffer) == 0) {
						strcpy(buffer, rooms[i].name);
					}
					else if (length + strlen(rooms[i].name) + 1 < MAX_DATA) {
						strcpy(buffer+length, rooms[i].name);
					}
					length = length + strlen(rooms[i].name);
					buffer[length] = ',';
					length++;
				}
			}
		}
		else {
			strcpy(buffer, "empty");
		}
		
		reply.status = SUCCESS;
		strcpy(reply.list_room, buffer);
	}
	else {
		reply.status = FAILURE_INVALID;
	}
	
	send(sockfd, (char *)&reply, sizeof(struct Reply), 0);
	close(sockfd);
	
	if (make_room == 1) {
		create_room(reply.port, param);
	}
	
	pthread_detach(pthread_self());
}

void create_room(int port, char *name) {
	int listen_fd, room_id;
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
	
	for (int i = 0; i < MAX_ROOMS; ++i) {
		if (rooms[i].members == -1) {
			room_id = i;
			break;
		}
	}
	
	rooms[room_id].port = port;
	rooms[room_id].sockfd = listen_fd;
	rooms[room_id].members = 0;
	strcpy(rooms[room_id].name, name);
	
	active_rooms++;
	
	char buffer[MAX_DATA];

	int client_socks[MAX_CLIENTS] = {-1};
	int retval, n_clients = 0;
	
	listen(listen_fd, 5);
	
	while(1) {
		memset(buffer, '\0', MAX_DATA);
		
	    int max_sock = -1;
	    fd_set sock_set;
	    FD_ZERO(&sock_set);
	    FD_SET(listen_fd, &sock_set);
	    
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
	            int new_fd = accept(listen_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
	            
	            if (new_fd >= 0) {
	            	client_socks[n_clients] = new_fd;
	            	n_clients++;
	            }
	            else {
	            	strcpy(buffer, "Warning: the chat room is closing...\n");
	            	for (int i = 0; i < MAX_CLIENTS; ++i) {
	            		if (client_socks[i] > 0) {
	            			send(client_socks[i], buffer, sizeof(buffer), 0);
	            			close(client_socks[i]);
	            		}
	            	}
	            	break;
	            }
	        }
	
	        // iterate backwards in case we need to remove a disconnected client socket
	        for (int i = 0; i < MAX_CLIENTS; ++i) {
	        	if (client_socks[i] > 0) {
		        	if (FD_ISSET(client_socks[i], &sock_set)) {
		            	int n_bytes = recv(client_socks[i], buffer, sizeof(buffer), 0);
						if (n_bytes > 0) {
							for (int j = 0; j < MAX_CLIENTS; ++j) {
								if (j != i) {
									send(client_socks[j], buffer, sizeof(buffer), 0);
								}
							}
						}
						else {
			            	if (n_bytes == 0) {
			            	}
			                else {
			                	perror("recv(TCP)");
			                }
			                
			                rooms[room_id].members--;
			                close(client_socks[i]);
			                client_socks[i] = -1;
		            	}
		        	}
	        	}
	        }
	    }
	    else {
	    	perror("select");
	    }
	}
	
	rooms[room_id].members = -1;
	active_rooms--;
}