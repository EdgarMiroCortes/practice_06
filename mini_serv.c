// 1. Anadimos stdio.h, stdlib.h
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>

// 2. Variables para todo
int max_fd = 0;
int next_id = 0;

int ids[65536];
char *buffers[65536];

fd_set readfds, writefds, activefds;
char read_buf[1001];

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
			newbuf = calloc(1, strlen(*buf + i + 1) + 1);
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
	newbuf = malloc(len + strlen(add) + 1);
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}

// 3. Fatal.
void fatal(void) {
	write(2, "Fatal error\n", 12);
	exit(1);
}

// 4. Notify_all
// Recibimos mensaje y author.
// Loop para recorrer los fds. Si el fd esta en los WriteFDS y no somos nosotros, enviamos.
void send_all(int author, char *msg) {
	for (int fd = 0; fd <= max_fd; fd++)
		if (FD_ISSET(fd, &writefds) && fd != author)
			send(fd, msg, strlen(msg), 0);
}

int main(int ac, char **av) {
  // Check args
	if (ac != 2)
		return (write(2, "Wrong number of arguments\n", 26), 1);

  // Eliminamos len y cli
	int sockfd, connfd;
	struct sockaddr_in servaddr;

  // Nos cargamos los print, exit, y else, y ponemos el fatal().
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1)
		fatal();

  	max_fd = sockfd;
	bzero(&servaddr, sizeof(servaddr));

  // Server config: Port = Atoi av1
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(2130706433);
	servaddr.sin_port = htons(atoi(av[1]));

	if (bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0)
		fatal();
	if (listen(sockfd, 200) != 0)
		fatal();

  	FD_ZERO(&activefds);
	FD_SET(sockfd, &activefds);

	bzero(ids, sizeof(ids));
	bzero(buffers, sizeof(buffers));

	while (1) {
		readfds = writefds = activefds;
		if (select(max_fd + 1, &readfds, &writefds, NULL, NULL) < 0)
			continue;

		for (int fd = 0; fd <= max_fd; fd++) {
			if (!FD_ISSET(fd, &readfds))
				continue;

			if (fd == sockfd) { 
			// NEW CONNECTION
				connfd = accept(sockfd, NULL, NULL);
				if (connfd < 0)
					continue;
				if (connfd > max_fd)
					max_fd = connfd;

				ids[connfd] = next_id++;
				buffers[connfd] = NULL;
				
				FD_SET(connfd, &activefds);

				char	msg[100];
				sprintf(msg, "server: client %d just arrived\n", ids[connfd]);
				send_all(connfd, msg);
			} else
			{
				//antiguo cliente
				int r = recv(fd, read_buf, 1000, 0);
				if (r <= 0)
				{
					//el cliente se ha ido
					char	msg[100];
					sprintf(msg, "server: client %d just left\n", ids[fd]);
					send_all(fd, msg);
					
					FD_CLR(fd, &activefds);
					free(buffers[fd]);
					close(fd);
				}
				else
				{
					read_buf[r] = '\0';

					buffers[fd] = str_join(buffers[fd], read_buf);
					if (!buffers[fd])
						fatal();

					char *msg;
					while (extract_message(&buffers[fd], &msg) == 1)
					{
						char prefix[50];
						sprintf(prefix, "client %d: ", ids[fd]);

						char *full = malloc(strlen(prefix) + strlen(msg) + 1);
						if (!full)
							fatal();
						full[0] = 0; // INICIALIZAR
						strcat(full, prefix);
						strcat(full, msg);

						send_all(fd, full);
						free(full);
						free(msg);
					}
				}
			}
		}
	}
	return 0;
}

