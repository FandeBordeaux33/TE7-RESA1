#include<stdio.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<netdb.h>
#include"common.h"
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>
#include <string.h>
#include <assert.h>
#define BACKLOG 20 
#define FD_TAB_SIZE 128 

void die(int ret, char * msg)
{
    if (ret <0 ){
        perror(msg);
        exit(EXIT_FAILURE);
    }    
}


struct header{
    int size;
    char username[128];
    int type;
    char message[MSG_LEN];
};

int write_on_socket(int fd, void * buf, int size){
    int size_sent = 0;
    int ret_value = 0;
    while(size_sent !=size){
        ret_value = write(fd, (char*)buf+size_sent, size- size_sent);    //envoie par paquets d'octets
        die(ret_value, "wwriting on socket");
        if(ret_value == 0){
            close(fd);
            exit(EXIT_FAILURE);
        }
        size_sent += ret_value;

    }
    return size_sent;

}

int read_from_socket(int fd, void * buf, int size){
    int size_read = 0;
    int ret_value = 0;
    while(size_read !=size){
        ret_value = read(fd, (char*)buf+size_read, size- size_read);
        die(ret_value, "reading from socket");
        if(ret_value == 0){
            close(fd);
            return 0;
        }   
        size_read += ret_value;

    }
    return size_read;
}       

int main (int argc, char const *argv[])
{
    if (argc < 2) {
		fprintf(stderr, "Error : Invalid arguments.\nUsage: ./server <port>\n");
		exit(EXIT_FAILURE);
	}
    int listen_fd = socket(AF_INET, SOCK_STREAM,0); // on a crÃ©Ã© une socket ici
    if (listen_fd == -1){                           // si il y a un problÃ¨me error donc la y a pas d'erreur
        perror("Socket creation:");
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[1]));
    inet_aton("127.0.0.1", &server_addr.sin_addr);

    int yes=1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);

    int ret_value;
    ret_value = bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    die(ret_value, "on binding");
    
    ret_value = listen(listen_fd, BACKLOG);
    die(ret_value, "on listening");


    //init strut pollfd
    struct pollfd fds[FD_TAB_SIZE];
    fds[0].fd = listen_fd;
    fds[0].events = POLLIN;
    fds[0].revents = 0;
    for(int i = 1; i < FD_TAB_SIZE; i++){
        fds[i].fd = -1;
        fds[i].events = POLLIN;
        fds[i].revents = 0;
    }
    //fist item -> listening fd, events = POLLON, revent = 0
    
        while(1){
        printf("Will poll...\n");
        int nbfds = poll(fds,FD_TAB_SIZE, -1);          //nombre de descripteur de fichier actif 
        printf("Number of active fd (%d)\n", nbfds);
        for(int i =0; i < FD_TAB_SIZE; i++){
            //if activity on listening socket
            if(i==0 && (fds[0].revents & POLLIN)){      // 1 & 1 avec le binaire donne 1 si POLLIN est actif et 0 sinon
                printf("New client...\n");              // && est un opÃ©rateur logique qui retourne vrai si les deux conditions sont vraies
                fds[i].revents = 0;                     // On remet la socket d'Ã©coute Ã  0 pour pas qu'elle soit considÃ©rÃ©e comme active lors du prochain poll
                int new_fd = accept(listen_fd, NULL, NULL); // Accept the new client connection
                printf("ok\n");             // New client accepted successfully
                

                for(int j = 1; j < FD_TAB_SIZE; j++){       // Find an empty slot in the pollfd array
                    if(fds[j].fd == -1){
                        fds[j].fd = new_fd;
                        fds[j].events = POLLIN;
                        fds[j].revents = 0;             //On initialise les Ã©vÃ©nements retournÃ©s Ã  0 pour ce nouveau client

                        break;                    
                    }
                }
                // can accept
                // use new fd to init struct and fill array
            }
            //if activity on client socket
            if(i !=0 && (fds[i].revents & POLLIN)){
                fds[i].revents = 0;
                struct header hdr;
                int ret = read_from_socket(fds[i].fd, &hdr, sizeof(hdr));   //Le client envoie la structure brute que le serveur doit lire pour connaÃ®tre la taille du message suivant
                if (ret <= 0) {
                    close(fds[i].fd);
                    fds[i].fd = -1;
                    continue;
                }
                                     
                fprintf(stdout, "[%s] (Taille: %d octets) a dit : %s\n", hdr.username, hdr.size, hdr.message);
                int ret2 = -1;
                ret2 = write(fds[i].fd, &hdr, sizeof(hdr));
                assert(ret2 != -1);
        
                // Reset the buffer for the next read
                //Read data from socket
                //close socket if needed
            } 
        }

    }



    // struct sockaddr_in client_addr;
    // socklen_t addrlen = sizeof(struct sockaddr_in);
    // printf("Accepting...\n");
    // int new_clientfd = accept(listen_fd, (struct sockaddr*)&client_addr, &addrlen);
    // die(new_clientfd, "Accept");
    // printf("New client on addr (%s:%hu) and fd %d\n", inet_ntoa(client_addr.sin_addr),ntohs(client_addr.sin_port), new_clientfd);


    // //First rcv next message size
    // struct header msgheader = {0};
    // int size_read = read_from_socket(new_clientfd, &msgheader, sizeof(struct header));       //& l'adresse de la ou je veux lire
    // printf("MSG HEADER size (%d) usernam (%s) type(%d)\n", msgheader.size, msgheader.username, msgheader.type);


    // int size_of_next_msg = msgheader.size;  // il rÃ©cupÃ¨re la taille du message Ã  cette ligne 
    // char * buf = malloc(size_of_next_msg * sizeof(char));  
    // //char buf[128] = {0};                               // ce qu'on lit donc la taille de size_of_next_msg
    // size_read = read_from_socket(new_clientfd, buf, size_of_next_msg);
    // printf("MSG RECU (%s) (%d)\n", buf, size_read);
    // return 0;
}