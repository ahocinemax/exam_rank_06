#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/ip.h>

char *msgs[4096*42], read_buff[4096*42], write_buff[42];
int g_id, sockfd, maxfd = 0;
int ids[65536];
fd_set rfds, wfds, afds;

void    fatal()
{
    write(2, "Fatal error\n", 12);
    exit(1);
}

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
			newbuf = (char *)calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
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
	newbuf = (char *)malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}

void    notify_clients(int author, char *msg)
{
    for (int fd = 0 ; fd <= maxfd ; fd++)
        if (FD_ISSET(fd, &wfds) && fd != author)
            send(fd, msg, strlen(msg), 0);
}

void recieve_client()
{
    struct sockaddr_in clientAddr;
    socklen_t len = sizeof(clientAddr);
    int fd = accept(sockfd, (struct sockaddr *)&clientAddr, &len);
    if (fd < 0) fatal();
    maxfd = fd > maxfd ? fd : maxfd;
    FD_SET(fd, &afds);
    ids[fd] = g_id++;
    msgs[fd] = NULL;
    sprintf(write_buff, "server: client %d just arrived\n", ids[fd]);
    notify_clients(fd, write_buff);
}

void remove_client(int fd)
{
    sprintf(write_buff, "server: client %d just left\n", ids[fd]);
    notify_clients(fd, write_buff);
    FD_CLR(fd, &afds);
    free(msgs[fd]);
    close(fd);
}

void send_msg(int fd)
{
    char *msg = NULL;
    while (extract_message(&msgs[fd], &msg))
    {
        sprintf(write_buff, "client %d: ", ids[fd]);
        notify_clients(fd, write_buff);
        notify_clients(fd, msg);
        free(msg);
    }
    if (!msgs[fd] || !*(msgs[fd]))
    {
        free(msgs[fd]);
        msgs[fd] = NULL;
    }
}

void create_socket()
{
    maxfd = socket(AF_INET, SOCK_STREAM, 0);
    if (maxfd < 0) fatal();
    FD_SET(maxfd, &afds);
    sockfd = maxfd;
}

int main(int ac, char *av[])
{
    if (ac != 2)
    {
        write(2, "Wrong number of arguments\n", 26);
        exit(1);
    }
    struct sockaddr_in serverAddr;
    socklen_t len = sizeof(serverAddr);
    bzero(&serverAddr, len);
    create_socket();
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(atoi(av[1])),
    serverAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(sockfd, (const struct sockaddr *)&serverAddr, len) < 0) fatal();
    if (listen(sockfd, SOMAXCONN) < 0) fatal();
    while (1)
    {
        wfds = rfds = afds;
        if (select(maxfd + 1, &rfds, &wfds, NULL, NULL) < 0) fatal();
        for (int fd = 0 ; fd <= maxfd ; fd++)
        {
            if (!FD_ISSET(fd, &rfds)) continue ;
            if (fd == sockfd) recieve_client();
            else
            {
                int read = recv(fd, read_buff, 1000, 0);
                if (read < 0)
                {
                    remove_client(fd);
                    break ;
                }
                read_buff[read] = '\0';
                msgs[fd] = str_join(msgs[fd], read_buff);
                send_msg(fd);
            }
        }
    }
    return (0);
}
