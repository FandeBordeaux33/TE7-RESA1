#include "common.h"
#include "client_list.h"
#include "msg_struct.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_MESSAGE_SIZE 4096
#define MAX_CLIENTS 128


int setup_listening_socket(int port) {					//initialisation connexion 
	int listen_fd;
	int result;
	struct sockaddr_in server_address;

	listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	die(listen_fd, "socket");
	printf("TCP listening socket created.\n");

	memset(&server_address, 0, sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = htonl(INADDR_ANY); // To listen on all interfaces --- Equivalent to 0.0.0.0
	server_address.sin_port = htons((unsigned short)port);
	result = bind(listen_fd, (struct sockaddr *)&server_address, sizeof(server_address));
	die(result, "bind");
	printf("Socket bound to port %d.\n", port);

	result = listen(listen_fd, 20);
	die(result, "listen");
	printf("Listening for client connections.\n");
	return listen_fd;
}

void accept_and_insert_client(int listen_fd, struct pollfd poll_fds[MAX_CLIENTS], struct client_info **clients) {
	struct sockaddr_in client_address;
	socklen_t client_address_length = sizeof(client_address);
	int client_fd = accept(listen_fd, (struct sockaddr *)&client_address, &client_address_length);
	int slot;
	die(client_fd, "accept");

	for (slot = 1; slot < MAX_CLIENTS; slot++) {								//recherche d'un slot libre
		if (poll_fds[slot].fd < 0) {											// slot libre trouvé car fd < 0
			if (client_list_add(clients, client_fd, &client_address) < 0) {
    			perror("malloc client information");
    			close(client_fd);
    			return;
			}
			poll_fds[slot].fd = client_fd;
			poll_fds[slot].events = POLLIN;
			poll_fds[slot].revents = 0;
			printf("Accepted client %s:%u on slot %d.\n",
				inet_ntoa(client_address.sin_addr),
				(unsigned int)ntohs(client_address.sin_port), slot);
			break;
		}
	}

	if (slot == MAX_CLIENTS) {
		fprintf(stderr, "Client limit reached. Closing the new connection.\n");
		close(client_fd);
	}
}

/* Return 1 when the client should be disconnected, 0 after a successful echo. */
int handle_client_message(int client_fd, struct client_info *clients) {
	struct message msg;
	char payload[MAX_MESSAGE_SIZE + 1];

	// first read next message size
	if (read_from_socket(client_fd, &msg, sizeof(msg)) == 0) {
		fprintf(stderr, "Client %d : Socket close\n", client_fd);
		return 1;
	}
	if (msg.pld_len < 0 || msg.pld_len > MAX_MESSAGE_SIZE) {
		fprintf(stderr, "Client %d : Error on message size (%d) \n", client_fd, msg.pld_len);
		return 1;
	}
	// then read the message payload
	if (msg.pld_len > 0 && read_from_socket(client_fd, payload, msg.pld_len) == 0) {
		fprintf(stderr, "Client %d : Socket close\n", client_fd);
		return 1;
	}
	payload[msg.pld_len] = '\0';

	if (msg.type == NICKNAME_NEW) {		
    	struct client_info *current = clients;

    	while (current != NULL) {						//parcours de la liste des clients jusq'à la fin 
        	if (current->fd == client_fd) {
            	strncpy(current->nickname, msg.infos, sizeof(current->nickname) - 1);
            	current->nickname[sizeof(current->nickname) - 1] = '\0';

            	printf("Client %d : pseudo %s\n",
                   client_fd, current->nickname);
            	return 0;
        	}
        	current = current->next;
    	}
    	return 1;  // Client introuvable
    }

	if (strcmp(payload, "/quit") == 0) {
		printf("Client %d requested to quit.\n", client_fd);
		return 1;
	}

	if (write_in_socket(client_fd, &msg, sizeof(msg)) == 0 ||
		write_in_socket(client_fd, payload, msg.pld_len) == 0) {
		return 1;
	}

	return 0;
}

void server_poll_loop(int listen_fd, struct pollfd poll_fds[MAX_CLIENTS], struct client_info **clients) {
	int running = 1;

	/* Slot 0 is the listener. The other slots contain client sockets. */

	for (int i = 0; i < MAX_CLIENTS; i++) {			//initialisation des slots de poll_fds
		poll_fds[i].fd = -1;
		poll_fds[i].events = 0;
		poll_fds[i].revents = 0;
	}

	poll_fds[0].fd = listen_fd;						//initialisation du slot de la socket d'écoute
	poll_fds[0].events = POLLIN;					//elle est active pour la lecture (nouveaux clients)

	// execute server logic
	while (running) {
		int ready = poll(poll_fds, MAX_CLIENTS, -1);								// attente d'événements sur les sockets
		die(ready, "poll");

		if ((poll_fds[0].revents & POLLIN) != 0) {									//un nouveau client arrive on regrade la socket d'écoute
			accept_and_insert_client(listen_fd, poll_fds, clients);					
		}

		for (int slot = 1; slot < MAX_CLIENTS; slot++) {							//on regarde si il y a du mouvement sur les autres sockets
			short returned_events = poll_fds[slot].revents;
			int close_connection = 0;
			if (poll_fds[slot].fd < 0) {
				continue;
			}

			if ((returned_events & POLLIN) != 0) {									
				close_connection = handle_client_message(poll_fds[slot].fd, *clients);
			}
			if ((returned_events & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
				close_connection = 1;
			}
			if (close_connection) {
				int client_fd = poll_fds[slot].fd;
				close(client_fd);
				client_list_remove(clients, client_fd);
				poll_fds[slot].fd = -1;
				poll_fds[slot].events = 0;
				poll_fds[slot].revents = 0;
			}
		}
		if ((poll_fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
			running = 0;
		}
	}

	// Cleaning up: close all client sockets and free the client list
	for (int slot = 1; slot < MAX_CLIENTS; slot++) {
		if (poll_fds[slot].fd >= 0) {
			close(poll_fds[slot].fd);
			poll_fds[slot].fd = -1;
		}
	}
	client_list_destroy(clients);
}

int main(int argc, char **argv) {
	struct pollfd poll_fds[MAX_CLIENTS];
	struct client_info *clients = NULL;				//pointeur vers la liste chaînée des clients connectés. C'est le premier maillon de la liste.
	int port;
	int listen_fd;

	if (argc != 2) {
		fprintf(stderr, "Usage: ./server <server_port>\n");
		return EXIT_FAILURE;
	}
	port = atoi(argv[1]);
	if (port < 1 || port > 65535) {
		fprintf(stderr, "Invalid port\n");
		return EXIT_FAILURE;
	}

	listen_fd = setup_listening_socket(port);
	server_poll_loop(listen_fd, poll_fds, &clients);
	close(listen_fd);
	return EXIT_SUCCESS;
}
