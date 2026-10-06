#ifndef JALON1_CLIENT_LIST_H
#define JALON1_CLIENT_LIST_H

#include <netinet/in.h>
#define NICK_LEN 128

struct client_info {
	int fd;								// le descripteur qui est sa socket connecté	
	struct sockaddr_in address;			// son adresse réseau ( IP + Port )
	char nickname[NICK_LEN];			//nickname est son pseudo
	struct client_info *next;			// pointeur vers le prochain client dans la liste chaînée
};

int client_list_add(struct client_info **clients, int fd, const struct sockaddr_in *address);
void client_list_remove(struct client_info **clients, int fd);
void client_list_destroy(struct client_info **clients);

#endif
