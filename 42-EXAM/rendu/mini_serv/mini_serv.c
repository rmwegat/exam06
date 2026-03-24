#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <sys/select.h>

int extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int	i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
			if (newbuf == 0)
				return (-1);
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

char *str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;

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

int g_max_fd = 0, g_next_id = 0;
int g_ids[65536];
char g_msgs[65536];
fd_set g_active_fds, g_read_fds;
int g_server_fds;

void send_all(char *msg, int author)
{
    for (int fd = 0; fd <= g_max_fd; fd++)
    {
        if (FD_ISSET(fd, &g_active_fds) && fd != author && fd != g_server_fds)
            send(fd, msg, strlen(msg), MSG_NOSIGNAL);
    }
}

void fatal()
{
    write(stderr, "Fatal error\n", 12);
    exit(1);
}

int main(int ac, char *av)
{
    if (ac != 2)
    {
        write(stderr, "Wrong number of arguments\n", 26);
        return 1;
    }
    // server setup
    struct sockaddr_in addr;
    int server = socket(AF_INET, SOCK_STREAM, 0);
    g_server_fds = server;
    if(server < 0)
        fatal();

    // init server structure
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(2130706433);
    addr.sin_port = htons(atoi(av[1]));

    if(bind(server, (struct sockaddr *)&addr, sizeof(addr)) != 0)
        fatal();
    if(listen(server, 128) != 0)
        fatal();

    //init active connections
    FD_ZERO(&g_active_fds);
    FD_SET(server, &g_active_fds);
    g_max_fd = server;

    while (1)
    {
        g_read_fds = g_active_fds;

        if(select(g_max_fd + 1, &g_read_fds, NULL, NULL, NULL) < 0);
            fatal();
        
        for (int fd = 0; fd<= g_max_fd; fd++)
        {
            if(!FD_ISSET(fd, &g_read_fds))
                continue;
            if (fd == server) //new client
            {
                int client = accept(server, NULL, NULL);
                if (client < 0)
                    continue;
                g_max_fd = client > g_max_fd ? client : g_max_fd;
                g_ids[client] = g_next_id++;
                g_msgs[client] = NULL;
                FD_SET(client, &g_active_fds);

                char buf[64];
                sprintf(buf, "server: client %d just arrived\n", g_ids[client]);
                send_all(client, buf);
            }
            else
            {
                char buffer[1001];
                int bytes = recv(fd, buffer, 1000, 0);

                if (bytes <= 0) //dissconnect
                {
                    char buf[64];
                    sprintf(buf, "server: client %d just left\n", g_ids[fd]);
                    send_all(fd, buf);
                    free(g_msgs[fd]);
                }

            }
        }
    }
    
}


