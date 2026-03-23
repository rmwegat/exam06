#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/select.h>

// --- EXAM PROVIDED COPY/PASTES ---
int extract_message(char **buf, char **msg) {
	char *newbuf;
	int i;

	*msg = 0;
	if (*buf == 0) return (0);
	i = 0;
	while ((*buf)[i]) {
		if ((*buf)[i] == '\n') {
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
			if (newbuf == 0) return (-1);
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		} i++;
	}
	return (0);
}

char *str_join(char *buf, char *add) {
	char *newbuf;
	int len;

	if (buf == 0) len = 0;
	else len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0) return (0);
	newbuf[0] = 0;
	if (buf != 0) strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}
// --- END COPY/PASTE ---

int max_fd = 0, next_id = 0;
int ids[65536];
char *msgs[65536];
fd_set active_fds, read_fds, write_fds;

void fatal() {
    write(2, "Fatal error\n", 12);
    exit(1);
}

void send_all(int author, char *msg) {
    for (int fd = 0; fd <= max_fd; fd++) {
        if (FD_ISSET(fd, &write_fds) && fd != author)
            send(fd, msg, strlen(msg), 0); //MSG_NOSIGNAL instead of 0 for exam (only works on linux, prevents SIGPIPE crashes)
    }
}

int main(int ac, char **av) {
    if (ac != 2) { 
        write(2, "Wrong number of arguments\n", 26); 
        return 1; 
    }

    struct sockaddr_in addr;
    int server = socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0) fatal();

    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(2130706433);
    addr.sin_port = htons(atoi(av[1]));

    if (bind(server, (struct sockaddr *)&addr, sizeof(addr)) != 0) fatal();
    if (listen(server, 128) != 0) fatal();

    FD_ZERO(&active_fds);
    FD_SET(server, &active_fds);
    max_fd = server;

    while (1) {
        read_fds = write_fds = active_fds;
        if (select(max_fd + 1, &read_fds, &write_fds, NULL, NULL) < 0) fatal();

        for (int fd = 0; fd <= max_fd; fd++) {
            if (!FD_ISSET(fd, &read_fds)) continue;

            if (fd == server) {
                int client = accept(server, NULL, NULL);
                if (client < 0) continue;
                max_fd = client > max_fd ? client : max_fd;
                ids[client] = next_id++;
                msgs[client] = NULL;
                FD_SET(client, &active_fds);
                
                char buf[64];
                sprintf(buf, "server: client %d just arrived\n", ids[client]);
                send_all(client, buf);
            } 
            else {
                char buffer[1001];
                int bytes = recv(fd, buffer, 1000, 0);
                if (bytes <= 0) {
                    char buf[64];
                    sprintf(buf, "server: client %d just left\n", ids[fd]);
                    send_all(fd, buf);
                    free(msgs[fd]);
                    FD_CLR(fd, &active_fds);
                    close(fd);
                } 
                else {
                    buffer[bytes] = '\0';
                    msgs[fd] = str_join(msgs[fd], buffer);
                    if (msgs[fd] == NULL) fatal();
                    char *msg;
                    while (extract_message(&msgs[fd], &msg)) {
                        char buf[64];
                        sprintf(buf, "client %d: ", ids[fd]);
                        send_all(fd, buf);
                        send_all(fd, msg);
                        free(msg);
                    }
                }
            }
        }
    }
}
