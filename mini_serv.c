#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>

fd_set a, r, w;
int max = 0, next = 0, ids[65536];
char *buffer[65536], read_buf[1001];

void fatal(void)
{
	write(2, "Fatal error\n",12);
	exit(1);
}

void notify(int author, char *msg)
{
	for(int fd = 0; fd <= max; fd++)
		if(FD_ISSET(fd, &w) && fd != author)
			send(fd, msg, strlen(msg), 0);
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


int main(int ac, char** av) {
	if(ac != 2)
		return(write(2, "Wrong number of arguments\n", 26), 1);

	int sockfd, cfd;
	struct sockaddr_in servaddr; 

	sockfd = socket(AF_INET, SOCK_STREAM, 0); 
	if (sockfd == -1)
		fatal();
	
	max = sockfd;
	bzero(&servaddr, sizeof(servaddr)); 

	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433);
	servaddr.sin_port = htons(atoi(av[1])); 

	if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
		fatal(); 
	if (listen(sockfd, 10) != 0)
		fatal();
	
	FD_ZERO(&a);
	FD_SET(sockfd, &a);
	bzero(buffer, sizeof(buffer));
	bzero(ids, sizeof(ids));
	while(1){
		r = w = a;
		if(select(max + 1,  &r, &w, NULL, NULL) < 0)
			continue;
			
		for(int fd = 0; fd <= max; fd++)
		{
			if(!FD_ISSET(fd, &r))
				continue;
				
			if(fd == sockfd) // NEW CONNECTION
			{
				cfd = accept(sockfd, NULL, NULL);
				if(cfd < 0)
					continue;
				if(cfd > max)
					max = cfd;
				ids[cfd] = next++;
				buffer[cfd] = NULL;
				FD_SET(cfd, &a);
				char msg[256];
				sprintf(msg, "server: client %d just arrived\n", ids[cfd]);
				notify(cfd, msg);
			}
			else // OLD CLIENT :)
			{
				int recived = recv(fd, read_buf, 1000, 0);
				if(recived <= 0) // CLIENT LEAVE
				{
					char msg[256];
					sprintf(msg, "server: client %d just left\n", ids[fd]);
					notify(fd, msg);
					FD_CLR(fd, &a);
					free(buffer[fd]);
					close(fd);
				}
				else // SEND MSG!
				{
					read_buf[recived] = '\0';
					buffer[fd] = str_join(buffer[fd], read_buf);
					if(!buffer[fd])
						fatal();
					char *msg;
					while(extract_message(&buffer[fd], &msg))
					{
						char prefix[50];
						sprintf(prefix, "client %d: ", ids[fd]);

						char *full = malloc(strlen(prefix) + strlen(msg) + 1);
						if (!full)
							fatal();
						full[0] = 0; // INICIALIZAR
						strcat(full, prefix);
						strcat(full, msg);

						notify(fd, full);
						free(full);
						free(msg);
					}
				}
							
			}
		}		
	}
	return(1);
}
