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

	if (buf == 0)
        len = 0;
	else
        len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
        return (0);
	newbuf[0] = 0;
	if (buf != 0)
        strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}
// --- END COPY/PASTE ---

// --- Global variables for socket tracking ---
int max_fd = 0, next_id = 0;
int ids[65536]; // Maps fd to a user id (0, 1, 2, ...)
char *msgs[65536]; // Keeps track of partially received messages per fd
fd_set active_fds, read_fds, write_fds; // active_fds is the source of truth, read/write are copies for select()

void fatal() {
    write(2, "Fatal error\n", 12);
    exit(1);
}

// Broadcasts a message to everyone except the author
void send_all(int author, char *msg) {
    for (int fd = 0; fd <= max_fd; fd++) {
        // If the fd is in write_fds, it's ready to receive data
        if (FD_ISSET(fd, &write_fds) && fd != author)
            send(fd, msg, strlen(msg), 0); //MSG_NOSIGNAL instead of 0 for exam (only works on linux, prevents SIGPIPE crashes)
    }
}

int main(int ac, char **av) {
    if (ac != 2) { 
        write(2, "Wrong number of arguments\n", 26); 
        return 1; 
    }

    // --- Server setup ---
    struct sockaddr_in addr;
    int server = socket(AF_INET, SOCK_STREAM, 0); // Create a TCP socket
    if (server < 0) fatal();

    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(2130706433); // 127.0.0.1
    addr.sin_port = htons(atoi(av[1])); // Parse port from args

    if (bind(server, (struct sockaddr *)&addr, sizeof(addr)) != 0) fatal();
    if (listen(server, 128) != 0) fatal(); // Backlog set to 128

    // Initialize our set of active connections
    FD_ZERO(&active_fds);
    FD_SET(server, &active_fds);
    max_fd = server;

    while (1) {
        // select() modifies the fd_sets, so we have to copy them from our active_fds master list
        read_fds = write_fds = active_fds;
        
        // select monitors the largest fd + 1
        if (select(max_fd + 1, &read_fds, &write_fds, NULL, NULL) < 0) fatal();

        for (int fd = 0; fd <= max_fd; fd++) {
            // Only handle read events if fd is flagged by select
            if (!FD_ISSET(fd, &read_fds)) continue;

            if (fd == server) {
                // Event on server fd means a new client is connecting
                int client = accept(server, NULL, NULL);
                if (client < 0) continue;
                max_fd = client > max_fd ? client : max_fd; // Update max_fd if needed
                ids[client] = next_id++;
                msgs[client] = NULL;
                FD_SET(client, &active_fds); // Add new client to active list
                
                char buf[64];
                sprintf(buf, "server: client %d just arrived\n", ids[client]);
                send_all(client, buf);
            } 
            else {
                // Event on client fd means data was received or client disconnected
                char buffer[1001];
                int bytes = recv(fd, buffer, 1000, 0);
                
                if (bytes <= 0) {
                    // Client disconnected or error
                    char buf[64];
                    sprintf(buf, "server: client %d just left\n", ids[fd]);
                    send_all(fd, buf);
                    free(msgs[fd]);
                    msgs[fd] = NULL;
                    FD_CLR(fd, &active_fds); // Remove client from active list
                    close(fd);
                } 
                else {
                    // Received data
                    buffer[bytes] = '\0'; // Null-terminate received string
                    msgs[fd] = str_join(msgs[fd], buffer); // Append to existing buffer
                    if (msgs[fd] == NULL) fatal(); // Check for str_join allocation error
                    
                    char *msg;
                    // Extract full messages ending with '\n'
                    while (extract_message(&msgs[fd], &msg)) {
                        char buf[64];
                        sprintf(buf, "client %d: ", ids[fd]);
                        send_all(fd, buf); // Send prefix
                        send_all(fd, msg); // Send actual message
                        free(msg); // extract_message allocates new memory, so free it
                    }
                }
            }
        }
    }
}
