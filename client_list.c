#include "client_list.h"

#include <stdlib.h>
#include <netinet/in.h>
#define NICK_LEN 128

int client_list_add(struct client_info **clients, int fd, const struct sockaddr_in *address) {
	struct client_info *client = malloc(sizeof(*client));			//on créer le maillon
	
	if (client == NULL) {
		return -1;
	}
	client->fd = fd;												//on le rempli
	client->address = *address;
	client->next = *clients;										// on insère le nouveau client au début 
	client->nickname[0] = '\0'; //Initialisation du nickname
	client->connected_at = time(NULL); //Initialisation du timestamp de connexion
	*clients = client;
	return 0;
}

void client_list_remove(struct client_info **clients, int fd) {
	struct client_info **cursor = clients;

	while (*cursor != NULL) {
		if ((*cursor)->fd == fd) {
			struct client_info *removed = *cursor;
			*cursor = removed->next;
			free(removed);
			return;
		}
		cursor = &(*cursor)->next;
	}
}

void client_list_destroy(struct client_info **clients) {
	while (*clients != NULL) {
		struct client_info *removed = *clients;
		*clients = removed->next;
		free(removed);
	}
}
