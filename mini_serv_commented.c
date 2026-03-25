#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>

// --- EXAM PROVIDED COPY/PASTES ---
int extract_message(char **buf, char **msg) {
    char *newbuf;
	int i;
    
	*msg = 0;
	if (*buf == 0) return (0);
	i = 0;
	while ((*buf)[i]) {
        if ((*buf)[i] == '\n') {
            newbuf = calloc(1, sizeof(*newbuf) * \
            (strlen(*buf + i + 1) + 1));
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
	newbuf = malloc(sizeof(*newbuf) * \
    (len + strlen(add) + 1));
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

#include <sys/select.h>

// --- Global variables for socket tracking ---
int g_max_fd = 0, g_next_id = 0;
int g_ids[65536]; // Maps fd to a user id (0, 1, 2, ...)
char *g_msgs[65536]; // Keeps track of partially received messages per fd
fd_set g_active_fds, g_read_fds; // g_active_fds is the source of truth, read are copies for select()
int g_server_fds; // To keep track of the server listening socket

void fatal() {
    write(2, "Fatal error\n", 12);
    exit(1);
}

// Broadcasts a message to everyone except the author
void send_all(int author, char *msg) {
    for (int fd = 0; fd <= g_max_fd; fd++) {
        if (FD_ISSET(fd, &g_active_fds) && \
        fd != author && fd != g_server_fds)
            send(fd, msg, strlen(msg), MSG_NOSIGNAL); //MSG_NOSIGNAL for linux in exam, 0 on MacOS prevents SIGPIPE crashes)
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
    g_server_fds = server;
    if (server < 0)
        fatal();

    // Set up server address structure
    bzero(&addr, sizeof(addr)); //allocate and zero
    addr.sin_family = AF_INET; //set adrees family
    addr.sin_addr.s_addr = htonl(2130706433); // bind to 127.0.0.1
    addr.sin_port = htons(atoi(av[1])); // Parse port from args


    if (bind(server, (struct sockaddr *)&addr, sizeof(addr)) != 0)
        fatal();
    if (listen(server, 128) != 0) // Backlog set to 128
        fatal();

    // Initialize our set of active connections
    FD_ZERO(&g_active_fds);
    FD_SET(server, &g_active_fds);
    g_max_fd = server;

    while (1) {
        // select() modifies the fd_sets, so we have to copy them from our g_active_fds master list
        g_read_fds = g_active_fds;
        
        // select monitors the largest fd + 1
        if (select(g_max_fd + 1, &g_read_fds, NULL, NULL, NULL) < 0)
            fatal();

        for (int fd = 0; fd <= g_max_fd; fd++)
        {
            // skip not ready fds
            if (!FD_ISSET(fd, &g_read_fds))
                continue;

            if (fd == server) // Event on server fd means a new client is connecting
            {
                int client = accept(server, NULL, NULL);
                if (client < 0)
                    continue;
                g_max_fd = client > g_max_fd ? client : g_max_fd; // Update g_max_fd if needed
                g_ids[client] = g_next_id++;
                g_msgs[client] = NULL;
                FD_SET(client, &g_active_fds); // Add new client to active list
                
                char buf[64];
                sprintf(buf, "server: client %d just arrived\n", g_ids[client]);
                send_all(client, buf);

            }
            else // Event on client fd means data was received or client disconnected
            {
                char buffer[1001];
                int bytes = recv(fd, buffer, 1000, 0);

                if (bytes <= 0)  // Client disconnected (or error)
                {
                    char buf[64];
                    sprintf(buf, "server: client %d just left\n", g_ids[fd]);
                    send_all(fd, buf);
                    free(g_msgs[fd]);
                    FD_CLR(fd, &g_active_fds); // Remove client from active list
                    close(fd);
                } 
                else // process incoming client data
                {
                    // Received data
                    buffer[bytes] = '\0';
                    g_msgs[fd] = str_join(g_msgs[fd], buffer);
                    if (g_msgs[fd] == NULL)
                        fatal();
                    
                    char *msg;
                    // Extract full messages ending with '\n'
                    while (extract_message(&g_msgs[fd], &msg)) {
                        char buf[64];
                        sprintf(buf, "client %d: ", g_ids[fd]);
                        send_all(fd, buf); // Send prefix
                        send_all(fd, msg); // Send actual message
                        free(msg); // extract_message allocates new memory, so free it
                    }
                }
            }
        }
    }
}
