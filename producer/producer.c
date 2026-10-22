#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>

#define CONMAX 4

void handle_connection(int fd);
int tcp_listen(const char *ip, int port);
int handle_client(int fd);
void handle_stdin(int fd, struct pollfd *connections, size_t ncon);
void send_events(struct pollfd *connections, ssize_t ncon);
char *read_entire_file(FILE *fp, size_t *len);

int main(void)
{
	int ret;
	int listenfd, clientfd;
	const int port = 3939;
	const char *ip = "127.0.0.1";
	struct pollfd connections[CONMAX];
	int coni, nready;

	for (int i = 0; i < CONMAX; ++i)
		connections[i].fd = -1;

	listenfd = tcp_listen(ip, port);

	connections[0].fd = listenfd;
	connections[0].events = POLLIN;
	connections[1].fd = STDIN_FILENO;
	connections[1].events = POLLIN;
	coni = 1;

	fprintf(stderr, "Listening on port [%d]\n", port);
	fprintf(stderr, "Enter \"send\" command after kore app connects.\n");
	for (;;) {
		nready = poll(connections, coni + 1, -1);
		if (nready == -1) {
			perror("poll error");
			exit(EXIT_FAILURE);
		}
		if (connections[0].revents & POLLIN) {
			clientfd = accept(listenfd, NULL, NULL);
			if (clientfd == -1) {
				perror("accept error");
				exit(EXIT_FAILURE);
			}
			fprintf(stderr, "New connection received\n");
			int found = 0;
			for (int i = 2; i < CONMAX; ++i) {
				if (connections[i].fd == -1) {
					connections[i].fd = clientfd;
					connections[i].events = POLLIN;
					found = 1;
					fprintf(stderr, "New connection established\n");
					if (i > coni) coni = i;
					break;
				}
			}
			if (!found) {
				fprintf(stderr, "Connection full, cannot accept new connection.\n");
				close(clientfd);
			}
			if (--nready <= 0)
				continue;
		}

		// Stdin ready
		if (connections[1].revents & (POLLIN | POLLERR)) {
			handle_stdin(connections[1].fd, &connections[2], coni - 2 + 1);
		}

		// Check client ready socket
		for (int i = 2; i < CONMAX && nready; ++i) {
			if (connections[i].revents & (POLLIN | POLLERR)) {
				ret = handle_client(connections[i].fd);
				if (ret == -1) {
					close(connections[i].fd);
					fprintf(stderr, "Connection closed [%d]\n", connections[i].fd);
					connections[i].fd = -1;
				}
			}
		}

	}

	return 0;
}

int handle_client(int fd)
{
	ssize_t nbytes;
	char buf[BUFSIZ];

	nbytes = read(fd, buf, sizeof(buf));
	if (nbytes == -1 || nbytes == 0) {
		return -1;
	}

	fprintf(stderr, "We should not be recieving any data from client.\n");

	return 0;
}

void handle_connection(int fd)
{
	fprintf(stderr, "New connection [%d]\n", fd);
}

int
tcp_listen(const char *ip, int port)
{
	int ret;
	int sockfd;
	struct sockaddr_in addr;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) {
		perror("sock error");
		exit(EXIT_FAILURE);
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	ret = inet_pton(AF_INET, ip, &addr.sin_addr.s_addr);
	if (ret == 0) {
		fprintf(stderr, "inet_ntop: First arg is invalid\n");
		exit(EXIT_FAILURE);
	} else if (ret == -1) {
		perror("inet_ntop error");
		exit(EXIT_FAILURE);
	}

	ret = bind(sockfd, (struct sockaddr *) &addr, sizeof(addr));
	if (ret == -1) {
		perror("bind error");
		exit(EXIT_FAILURE);
	}

	ret = listen(sockfd, 8);
	if (ret == -1) {
		perror("listen error");
		exit(EXIT_FAILURE);
	}

	return sockfd;
}

void handle_stdin(int fd, struct pollfd *connections, size_t ncon)
{
	ssize_t nbytes;
	char buf[BUFSIZ];
	char *cmds[] = { "send", NULL };
	char **ptr;

	nbytes = read(fd, buf, sizeof(buf));

	if (nbytes > 10) {
		fprintf(stderr, "Invalid command, ignoring.\n");
		return;
	}

	// Truncate new line
	buf[nbytes - 1] = '\0';

	ptr = cmds;
	int found = 0;
	while (*ptr) {
		if (strcmp(buf, *ptr) == 0) {
			found = 1;
			break;
		}
		ptr++;
	}

	if (!found) {
		fprintf(stderr, "Command [%s] not found\n", buf);
		return;
	}

	/* We only handle one command */
	for (size_t i = 0; i < ncon; ++i)
		send_events(connections, ncon);
}

void
send_events(struct pollfd *connections, ssize_t ncon)
{
	static struct {
		char *filename;
		FILE *fp;
		int delay; // in millisecond
	} files[] = {
		{ .filename="data/event_data_1.txt", .fp=NULL, .delay=0 },
		{ .filename="data/event_data_2.txt", .fp=NULL, .delay=100 },
		{ .filename="data/event_data_3.txt", .fp=NULL, .delay=0 },
		{ .filename="data/event_data_4.txt", .fp=NULL, .delay=6*1000 },
		{ .filename="data/event_data_5.txt", .fp=NULL, .delay=0 },
		{ .filename="data/event_data_6.txt", .fp=NULL, .delay=3*1000 },
		{ .filename="data/event_data_7.txt", .fp=NULL, .delay=100 },
	};
	int fileslen = 7;
	static int fileready = 0;


	if (!fileready) {
		for (int i = 0; i < fileslen; ++i) {
			char *ptr = files[i].filename;
			files[i].fp = fopen(ptr, "r");
			if (files[i].fp == NULL) {
				perror("fopen error");
				exit(EXIT_FAILURE);
			}
		}

		fileready = 1;
	}

	for (int i = 0; i < fileslen; ++i) {
		FILE *fp = files[i].fp;
		size_t len;
		char *content = read_entire_file(fp, &len);
		if (content) {
			for (int j = 0; j < ncon; ++j) {
				if (connections[j].fd != -1) {
					// sleep for x amount of millisecond
					usleep(files[i].delay * 1000);
					fprintf(stderr, "%2d: Sending event data [%s]..\n", j, files[i].filename);
					write(connections[j].fd, content, len);
				}
			}
			free(content);
		}
	}
}

char *
read_entire_file(FILE *fp, size_t *len)
{
	char *content;
	size_t ret;

	if (fseek(fp, 0, SEEK_END) == -1)
		return NULL;

	*len = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	content = malloc(*len);
	if (content == NULL) {
		perror("malloc error");
		return NULL;
	}

	ret = fread(content, 1, *len, fp);
	if (ret != *len && ferror(fp)) {
		perror("fread error, short read");
		free(content);
		return NULL;
	}

	return content;
}
