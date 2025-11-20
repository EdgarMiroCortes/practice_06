// Includes de main.c
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>

// 1. Anadir includes necesarios
#include <sys/select.h>
#include <stdlib.h>
#include <stdio.h>

// 2. Stuct t_client y variables globales
typedef struct s_client
{
	int	fd;
	int	id;
	char	*buf;
}	t_client;


t_client	clients[1024];
int	max_fd = 0;
int	next_id = 0;
fd_set	readfds, writefds, activefds; // tipo: fd_set

// 3. Funcion fatal
void	fatal(void)
{
	write(2, "Fatal error\n", 12);
	exit (1);
}

// 4. Funcion send_all
void	send_all(int sender_fd, char *msg)
{
	for (int i = 3; i <= max_fd; i++) // 3 pq el socket y el listen ya estan set.
	{
		if (clients[i].fd > 0 && clients[i].fd != sender_fd)
			send(clients[i].fd, msg, strlen(msg), 0);
	}
}

// 5. Funcion extract_message: copiar de main.c
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

// 6. Funcion str_join: copiar de main.c
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

void handle_new_connection(int sockfd, int *max_fd)
{
	int connfd = accept(sockfd, NULL, NULL);
	if (connfd < 0)
		return;

	if (connfd > *max_fd)
		*max_fd = connfd;

	clients[connfd].fd = connfd;
	clients[connfd].id = next_id++;
	clients[connfd].buf = NULL;

	FD_SET(connfd, &activefds);

	char msg[100];
	sprintf(msg, "server: client %d just arrived\n", clients[connfd].id);
	send_all(connfd, msg);
}

void handle_client_message(int i)
{
	char buffer[4096];
	int r = recv(i, buffer, sizeof(buffer) - 1, 0);

	if (r <= 0)
	{
		char msg[100];
		sprintf(msg, "server: client %d just left\n", clients[i].id);
		send_all(i, msg);

		FD_CLR(i, &activefds);
		free(clients[i].buf);
		close(i);
		clients[i].fd = 0;
		return;
	}

	buffer[r] = '\0';

	clients[i].buf = str_join(clients[i].buf, buffer);
	if (!clients[i].buf)
		fatal();

	char *msg;
	while (extract_message(&clients[i].buf, &msg) == 1)
	{
		char prefix[50];
		sprintf(prefix, "client %d: ", clients[i].id);

		char *full = malloc(strlen(prefix) + strlen(msg) + 1);
		if (!full)
			fatal();

		full[0] = 0;
		strcat(full, prefix);
		strcat(full, msg);

		send_all(i, full);
		free(full);
		free(msg);
	}
}

int main(int argc, char **argv) 
{
	// 1. Anadir check num args
	if (argc != 2)
		return (write(2, "Wrong number of arguments\n", 26), 1);

	// 2. Mismas variables que en main.c, eliminamos len
	int sockfd;
	struct sockaddr_in servaddr; 

	// 3. socket create and verification 
	sockfd = socket(AF_INET, SOCK_STREAM, 0); 
	if (sockfd == -1)
		fatal(); // cambir el printf y el exit por fatal(). eliminar el else.
	
	// 4. Igualamos max_fd a sockfd.
	max_fd = sockfd;

	// 5. Nos quedamos con el bzero y asignamos IP, PORT 
	bzero(&servaddr, sizeof(servaddr)); 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(atoi(argv[1])); 
  
	// 6. Mimas comprobaciones sin los mensajes de success. Si hay error, fatal() 
	if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
		fatal();
	if (listen(sockfd, 10) != 0)
		fatal();

	// A partir de aqui, nada es igual a main.c
	FD_ZERO(&activefds);
	FD_SET(sockfd, &activefds);

	memset(clients, 0, sizeof(clients));

	while (1)
	{
		readfds = writefds = activefds;

		if (select(max_fd + 1, &readfds, &writefds, NULL, NULL) < 0)
			continue;

        for (int i = 0; i <= max_fd; i++)
        {
            if (!FD_ISSET(i, &readfds))
                continue;
        
            if (i == sockfd)
                handle_new_connection(sockfd, &max_fd);
            else
                handle_client_message(i);
        }
	}
}
