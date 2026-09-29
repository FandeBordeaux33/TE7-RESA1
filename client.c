#include "common.h"
#include "msg_struct.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include "client_list.h"
#include <unistd.h>

#define MAX_MESSAGE_SIZE 4096
static char pseudoactuel[NICK_LEN] = "personne";

int setup_connection(const char *server_ip, const char *server_port) {
	int socket_fd;
	int result;
	struct sockaddr_in server_address;

	printf("Using server IPv4 address %s.\n", server_ip);
	memset(&server_address, 0, sizeof(server_address));
	server_address.sin_family = AF_INET;
	result = inet_aton(server_ip, &server_address.sin_addr);
	if (result == 0) {
		fprintf(stderr, "Invalid IPv4 address: %s\n", server_ip);
		return -1;
	}

	socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	die(socket_fd, "socket");
	printf("TCP socket created.\n");

	server_address.sin_port = htons((unsigned short)atoi(server_port));
	result = connect(socket_fd, (struct sockaddr *)&server_address, sizeof(server_address));
	die(result, "connect");
	printf("Connected to %s:%s.\n", inet_ntoa(server_address.sin_addr), server_port);
	return socket_fd;
}

/* Return 1 to keep running, or 0 if the server disconnects or sends an invalid message. */
int read_server_message(int socket_fd) {
	struct message msg;
	char payload[MAX_MESSAGE_SIZE];

	if (read_from_socket(socket_fd, &msg, sizeof(msg)) == 0) {
		return 0;
	}
	if (msg.pld_len <= 0 || msg.pld_len > MAX_MESSAGE_SIZE) {
		fprintf(stderr, "Invalid message size from server: %d\n", msg.pld_len);
		return 0;
	}
	if (read_from_socket(socket_fd, payload, (size_t)msg.pld_len) == 0) {
		return 0;
	}

	write(STDOUT_FILENO, payload, (size_t)msg.pld_len);
	return 1;
}

// Return 1 to keep running, or 0 when stdin closes or the user quits. 
int get_and_send_user_message(int socket_fd) {
	char message[MAX_MESSAGE_SIZE + 1];
	ssize_t bytes_read;
	int message_size;
	struct message msg;

	bytes_read = read(STDIN_FILENO, message, MAX_MESSAGE_SIZE);
	die(bytes_read, "read stdin");
	if (bytes_read == 0) {
		return 0;
	}

	message_size = bytes_read;
	message[message_size] = '\0';

	

	
	if (strcmp(message, "/quit") == 0 || strcmp(message, "/quit\n") == 0) {
		int quit_size = 5;
		write_in_socket(socket_fd, &quit_size, sizeof(quit_size));
		write_in_socket(socket_fd, "/quit", quit_size);
		return 0;
	}

	if (strncmp(message, "/nick ",6) == 0) {
		char *pseudo = message + 6; // On passe le /nick
		size_t pseudo_len = strlen(pseudo);
		if (pseudo_len > 0 && pseudo[pseudo_len - 1] == '\n') {
    		pseudo[pseudo_len - 1] = '\0';
		}
		pseudo[pseudo_len] = '\0';

		memset(&msg, 0, sizeof(msg));
		strncpy(msg.nick_sender, pseudoactuel, NICK_LEN - 1);
		msg.type = NICKNAME_NEW;
		strncpy(msg.infos, pseudo, INFOS_LEN - 1);

		if(write_in_socket(socket_fd, &msg, sizeof(msg)) == 0) {
			return 0;
		}
		strncpy(pseudoactuel, pseudo, NICK_LEN - 1);
		fprintf(stdout, "Votre pseudo est: %s\n", pseudo);
		return 1;
	}

	memset(&msg, 0, sizeof(msg));
	msg.type = ECHO_SEND;
	msg.pld_len = message_size;

	if (write_in_socket(socket_fd, &msg, sizeof(msg)) == 0) {
    	return 0;
	}

	if (msg.pld_len > 0 && write_in_socket(socket_fd, message, (size_t)msg.pld_len) == 0) {
    	return 0;
	}
	return 1;
}

void client_poll_loop(int socket_fd) {
	struct pollfd watched[2];
	int running = 1;

	/* Initialize once; poll() fills revents after each call. */
	watched[0].fd = STDIN_FILENO;
	watched[0].events = POLLIN;
	watched[1].fd = socket_fd;
	watched[1].events = POLLIN;

	while (running) {
		int ready = poll(watched, 2, -1);
		die(ready, "poll");

		if ((watched[1].revents & POLLIN) != 0) {
			running = read_server_message(socket_fd);
		}

		if (running && (watched[0].revents & POLLIN) != 0) {
			running = get_and_send_user_message(socket_fd);
		}

		if ((watched[0].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0 || (watched[1].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
			running = 0;
		}
	}
}

int main(int argc, char **argv) {
	int socket_fd;

	if (argc != 3) {
		fprintf(stderr, "Usage: ./client <server_ipv4> <server_port>\n");
		return EXIT_FAILURE;
	}
	socket_fd = setup_connection(argv[1], argv[2]);
	if (socket_fd < 0) {
		return EXIT_FAILURE;
	}
	client_poll_loop(socket_fd);
	close(socket_fd);
	return EXIT_SUCCESS;
}
