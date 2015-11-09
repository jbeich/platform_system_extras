#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

int main(int argc, char **argv)
{
	struct sockaddr_in serv;
	int fd, clifd;

	memset(&serv, 0, sizeof(serv));
	serv.sin_family = AF_INET;
	inet_aton(argv[1], &serv.sin_addr);
	serv.sin_port = htons(9999);

	if (argc < 2) {
		fprintf(stderr, "Usage: %s <listenaddr>", argv[0]);
		exit(-1);
	}

	if ((fd=socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		perror("socket");
		exit(-1);
	}
	int on = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
		perror("setsockopt");
		exit(-1);
	}
	if (bind(fd, (struct sockaddr *)&serv, sizeof serv) < 0) {
		perror("bind");
		exit(-1);
	}
	listen(fd, 64);
	while (1) {
		clifd = accept(fd, NULL, NULL);
		usleep(2000);
		close(clifd);
	}
	close(fd);
	return 0;
}

