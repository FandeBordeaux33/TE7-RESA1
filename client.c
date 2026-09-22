#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <assert.h>
#include <poll.h>
#define FD_TAB_SIZE 128

#include "common.h"

struct header{
    int size;
    char username[128];
    int type;
	char message[MSG_LEN];

};

void echo_client(int sockfd) {
	char buff[MSG_LEN];
	int n;
	while (1) {
		// Cleaning memory
		memset(buff, 0, MSG_LEN);
		// Getting message from client
		printf("Message: ");
		n = 0;
		while ((buff[n++] = getchar()) != '\n') {} // trailing '\n' will be sent
		// Sending message (ECHO)
		if (send(sockfd, buff, strlen(buff), 0) <= 0) {
			break;
		}
		printf("Message sent!\n");
		// Cleaning memory
		memset(buff, 0, MSG_LEN);
		// Receiving message
		if (recv(sockfd, buff, MSG_LEN, 0) <= 0) {
			break;
		}
		printf("Received: %s", buff);
	}
}


int handle_connect() {
	struct addrinfo hints, *result, *rp;
	int sfd;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	if (getaddrinfo(SERV_ADDR, SERV_PORT, &hints, &result) != 0) {
		perror("getaddrinfo()");
		exit(EXIT_FAILURE);
	}
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype,rp->ai_protocol);
		if (sfd == -1) {
			continue;
		}
		if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1) {
			break;
		}
		close(sfd);
	}
	if (rp == NULL) {
		fprintf(stderr, "Could not connect\n");
		exit(EXIT_FAILURE);
	}
	freeaddrinfo(result);
	return sfd;
}

int main(int argc, char *argv[]) {

	if (argc < 3) {
		fprintf(stderr, "Error : Invalid arguments.\nUsage: ./client <server_address> <port>\n");
		exit(EXIT_FAILURE);
	}
	int sfd;
	sfd = socket(AF_INET, SOCK_STREAM,0);		//création de la socket client
	assert(sfd != -1);		//vérification
	struct sockaddr_in server_addr;		//structure pour l'adresse du serveur
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(atoi(argv[2]));
	inet_pton(AF_INET, argv[1], &server_addr.sin_addr);
	int ret_value;
	ret_value = connect(sfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
	assert(ret_value != -1);		//vérification de la connexion

	struct pollfd fds[FD_TAB_SIZE];
    fds[0].fd = 0;						//file descriptor d'écoute standard (stdin)
    fds[0].events = POLLIN;				
    fds[0].revents = 0;


    for(int i = 1; i < FD_TAB_SIZE; i++){
        fds[i].fd = -1;
        fds[i].events = POLLIN;
        fds[i].revents = 0;
    }


	while(1) {
		fprintf(stdout, "En attente du clavier...\n");
		int nbfds = poll(fds,FD_TAB_SIZE, -1); 
		assert(nbfds != -1);
		for(int i = 0; i < FD_TAB_SIZE; i++) {
			if (i == 0 && (fds[i].revents & POLLIN)) {
				char buff[MSG_LEN];
				read(fds[i].fd, buff, MSG_LEN);
				printf("Read from stdin: %s", buff);
				int taille_msg = strlen(buff) + 1; 

				struct header msgheader = {0};          //Initialisation de la structure header à zéro
    			msgheader.size = taille_msg;      //On renseigne la taille du message suivant
    			strcpy(msgheader.username, "Pierre");   //On renseigne le nom d'utilisateur
    			msgheader.type = 0;
				buff[taille_msg - 1] = '\0';
				strcpy(msgheader.message, buff);
			

				int size_sent = 0;
				while(size_sent !=sizeof(struct header)){
        			ret_value = write(sfd, (char *)(&msgheader)+size_sent, sizeof(struct header)-size_sent);
        			if (ret_value == 0){
            			close(sfd);
            		exit(EXIT_FAILURE);
        			}
        		size_sent += ret_value;
    			}
				memset(buff, 0, MSG_LEN); // Clear the buffer after reading from stdin

				int new_fd = accept(sfd, NULL, NULL);
				for(int j = 0; j < FD_TAB_SIZE; j++) {
					if(fds[j].fd == -1) {
						fds[j].fd = new_fd;
						fds[j].events = POLLIN;
						fds[j].revents = 0;
						break;
					}
				}
			}
			
        }

	}
		
	// sfd = handle_connect();
	// echo_client(sfd);q
	// close(sfd);
	// return EXIT_SUCCESS;_
	return EXIT_SUCCESS;
}
