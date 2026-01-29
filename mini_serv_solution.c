// 1. Anadimos stdio.h, stdlib.h
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>

// 2. Variables para todo
int count = 0, max_fd = 0;
int ids[65536];
char *messages[65536];
fd_set rfds, wfds, afds;
char read_buf[1001], write_buf[100];

// 3. Funcion exactamente igual del main
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

// 4. Funcion exactamente igual del main
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

// 5. Implementamos fatal_error.
void fatal_error(void) {
	write(2, "Fatal error\n", 12);
	exit(1);
}

// 6. Implementamos notify_all
void notify_all(int author, char *msg) {
	for (int fd = 0; fd <= max_fd; fd++)
		if (FD_ISSET(fd, &wfds) && fd != author)
			send(fd, msg, strlen(msg), 0);
}

// 7. Register, remove, y send; Implementamos para usarlas abajo
void register_cli(int fd) {
	max_fd = fd > max_fd ? fd : max_fd;
	ids[fd] = count++;
	messages[fd] = NULL;
	FD_SET(fd, &afds);
	sprintf(write_buf, "server: client %d just arrived\n", ids[fd]);
	notify_all(fd, write_buf);
}

void remove_cli(int fd) {
	sprintf(write_buf, "server: client %d just left\n", ids[fd]);
	notify_all(fd, write_buf);
	free(messages[fd]);
	FD_CLR(fd, &afds);
	close(fd);
}

void send_message(int fd) {
	char *msg;
	while (extract_message(&messages[fd], &msg)) {
		sprintf(write_buf, "client %d: ", ids[fd]);
		notify_all(fd, write_buf);
		notify_all(fd, msg);
		free(msg);
	}
}

int main(int ac, char **av) {
  // 8. Revisamos args
	if (ac != 2)
		return (write(2, "Wrong number of arguments\n", 26), 1);

  // 9. Cambiamos formato de len a socklen_t
	int sockfd, connfd;
	socklen_t len;
	struct sockaddr_in servaddr, cli;
  
  // 10. Anadimos el fdzero
	FD_ZERO(&afds);

  // 11. Cambiamos el == -1 por < 0, nos cargamos los print, exit, y else, y ponemos el fatal().
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
		fatal_error();

  // 12. Dejamos el bzero tal como esta
	bzero(&servaddr, sizeof(servaddr));

  // 13. Cambiamos el 8081 por el av1
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(2130706433);
	servaddr.sin_port = htons(atoi(av[1]));

  // 14. Cambiamos los print, exit, y else por fatal
	if (bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0)
		fatal_error();
	if (listen(sockfd, 200) != 0)
		fatal_error();

  // 15. FD_SET y igualamos maxfd a sockfd.
	FD_SET(sockfd, &afds);
	max_fd = sockfd;

	while (1) {
		rfds = wfds = afds;
		if (select(max_fd + 1, &rfds, &wfds, NULL, NULL) < 0)
			fatal_error();

		for (int fd = 0; fd <= max_fd; fd++) {
			if (!FD_ISSET(fd, &rfds))
				continue;

			if (fd == sockfd) {
				len = sizeof(cli);
				connfd = accept(sockfd, (struct sockaddr *)&cli, &len);
				if (connfd >= 0)
					register_cli(connfd);
			} else {
				int n = recv(fd, read_buf, sizeof(read_buf) - 1, 0);
				if (n <= 0) {
					remove_cli(fd);
					continue;
				}
				read_buf[n] = '\0';
				messages[fd] = str_join(messages[fd], read_buf);
				send_message(fd);
			}
		}
	}
	return 0;
}
