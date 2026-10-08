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
	int listen_fd;										//lancement socket d'écoute 
	int result;
	struct sockaddr_in server_address;					//adresse du serveur 

	listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	die(listen_fd, "socket");
	printf("TCP listening socket created.\n");

	memset(&server_address, 0, sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = htonl(INADDR_ANY); 					// To listen on all interfaces --- Equivalent to 0.0.0.0
	server_address.sin_port = htons((unsigned short)port);
	result = bind(listen_fd, (struct sockaddr *)&server_address, sizeof(server_address));	//lie la socket avec le serveur
	die(result, "bind");
	printf("Socket bound to port %d.\n", port);

	result = listen(listen_fd, 20);								//mise en écoute de la socket	
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
    	struct client_info *current = clients;			//premier client
    	while (current != NULL) {						//parcours de la liste des clients jusq'à la fin 
        	if (current->fd == client_fd) {
				int premier_pseudo = (current->nickname[0] == '\0');  //vaut 1 si il n'y a pas de pseudo, 0 sinon 

            	strncpy(current->nickname, msg.infos, sizeof(current->nickname) - 1); //copie le pseudo de msg.infos dans current->nickname
            	current->nickname[sizeof(current->nickname) - 1] = '\0';	//on met un \0 pour le %s
				if (premier_pseudo) {
					printf(
						"\n========== CLIENT ==========\n"
						"[nickname] %s\n"
						"[socket]   %d\n"
						"[IP]       %s\n"
						"[port]     %u\n"
						"[next]     %p\n"
						"============================\n",
						current->nickname,
						current->fd,
						inet_ntoa(current->address.sin_addr),
						(unsigned int)ntohs(current->address.sin_port),
						(void *)current->next
					);
				} 
				else {
					printf("Client %d : pseudo changé en %s\n", client_fd, current->nickname);
				}
            	return 0;
        	}
        	current = current->next;
    	}
    	return 1;  // Client introuvable
    }

	if (msg.type == NICKNAME_LIST) {
		struct client_info *current = clients;
		char liste_noms[MAX_MESSAGE_SIZE];		//stocke le texte
		size_t cpt = 0;							//nombre d'octets ajouté
		struct message response;

		while (current != NULL) {
			if (current->nickname[0] != '\0') {          // on ignore les clients sans pseudo
				int n = snprintf(liste_noms + cpt, 
								sizeof(liste_noms) - cpt,
								"	- %s\n", current->nickname);
								
				if (n < 0 || (size_t)n >= sizeof(liste_noms) - cpt) {
					break;                               // plus de place : on s'arrête
				}
				cpt += (size_t)n;                        // on avance du nombre d'octets écrits
			}
			current = current->next;
		}

		if (cpt == 0) {                                  // le client refuse un pld_len de 0
			cpt = (size_t)snprintf(liste_noms, sizeof(liste_noms), "Aucun utilisateur\n");
		}

		memset(&response, 0, sizeof(response));
		response.type = NICKNAME_LIST;
		response.pld_len = (int)cpt;

		if (write_in_socket(client_fd, &response, sizeof(response)) == 0 ||
			write_in_socket(client_fd, liste_noms, cpt) == 0) {
			return 1;                                    // échec d'envoi : on déconnecte
		}
		return 0;                                        // succès : on garde le client
	}

	if (msg.type == NICKNAME_INFOS) {
		struct client_info *current = clients;
		struct message response;
		char texte[MAX_MESSAGE_SIZE];
		int n;

		msg.infos[sizeof(msg.infos) - 1] = '\0';

		n = snprintf(texte, sizeof(texte),
					"[Server] : User not found\n");

		while (current != NULL) {
			if (current->nickname[0] != '\0' &&
				strcmp(current->nickname, msg.infos) == 0) {

				char date[32];
				struct tm *t = localtime(&current->connected_at);

				if (t == NULL ||
					strftime(date, sizeof(date), "%Y/%m/%d@%H:%M", t) == 0) {
					return 1;
				}

				n = snprintf(
					texte, sizeof(texte),
					"[Server] : %s connected since %s "
					"with IP address %s and port number %u\n",
					current->nickname,
					date,
					inet_ntoa(current->address.sin_addr),
					(unsigned int)ntohs(current->address.sin_port)
				);

				break;  // Client trouvé : on arrête la recherche.
			}

			current = current->next;
		}

		if (n < 0 || (size_t)n >= sizeof(texte)) {
			return 1;
		}

		memset(&response, 0, sizeof(response));
		response.type = NICKNAME_INFOS;
		response.pld_len = n;

		if (write_in_socket(client_fd, &response, sizeof(response)) == 0 ||
			write_in_socket(client_fd, texte, (size_t)n) == 0) {
			return 1;
		}

		return 0;
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

void server_poll_loop(int listen_fd, struct pollfd poll_fds[MAX_CLIENTS], struct client_info **clients) {	//argument : socket d'écoute, tableau poll, liste chainée des clients
	int running = 1;

	/* Slot 0 is the listener. The other slots contain client sockets. */

	for (int i = 0; i < MAX_CLIENTS; i++) {			//initialisation des slots de poll_fds
		poll_fds[i].fd = -1;			
		poll_fds[i].events = 0;						
		poll_fds[i].revents = 0;
	}

	poll_fds[0].fd = listen_fd;						//initialisation du slot de la socket d'écoute
	poll_fds[0].events = POLLIN;					//elle est active pour la lecture (nouveaux clients) je veux du POLLIN

	// execute server logic
	while (running) {
		int ready = poll(poll_fds, MAX_CLIENTS, -1);								// attente d'événements sur les sockets
		die(ready, "poll");

		if ((poll_fds[0].revents & POLLIN) != 0) {									//un nouveau client arrive on regrade la socket d'écoute si POLLIN & POLLIN
			accept_and_insert_client(listen_fd, poll_fds, clients);					
		}

		for (int slot = 1; slot < MAX_CLIENTS; slot++) {							//on regarde si il y a du mouvement sur les autres sockets (les clients)
			short returned_events = poll_fds[slot].revents;							//création d'une variable étant le revents du slot qu'on surveille 
			int close_connection = 0;
			if (poll_fds[slot].fd < 0) {
				continue;
			}

			if ((returned_events & POLLIN) != 0) {									
				close_connection = handle_client_message(poll_fds[slot].fd, *clients);			//renvoie 0 si tout se passe bien, 1 si le client doit être déconnecté
			}
			if ((returned_events & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
				close_connection = 1;
			}
			if (close_connection != 0) {														//gestion de close_connection si ça retourne 1
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
	struct pollfd poll_fds[MAX_CLIENTS];			//tableau de structure pour les sockets qu'on surveille
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

	listen_fd = setup_listening_socket(port);					//initialisation de la socket d'écoute (listen_fd) sur le port en argument 
	server_poll_loop(listen_fd, poll_fds, &clients);			//boucle principal
	close(listen_fd);											//fermeture de la socket d'écoute
	return EXIT_SUCCESS;
}
