#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <linux/net.h>
#include <linux/socket.h>
#include <linux/in.h>
#include <sys/socket.h>

int main(void)
{
	int cli_sock;
	struct sockaddr_in serv;
	struct linger ln;
	int i = 0, rc;
	ln.l_onoff = 1;
	ln.l_linger = 0;
	memset(&serv, 0, sizeof serv);
	serv.sin_family = AF_INET;
	serv.sin_addr.s_addr = htonl(0x01010101);
	serv.sin_port = htons(9999);
	while (1) {
		if ((cli_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
			perror("tcp_client create sock error");
			continue;
		}
		setsockopt(cli_sock, SOL_SOCKET, SO_LINGER, (char *)&ln, sizeof(ln));
		connect(cli_sock, (struct sockaddr*) &serv, sizeof(serv));
		close(cli_sock);
		if (++i % 4096 == 0)
			sched_yield();
	}
	return 0;
}
