/*
 * Copyright 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netutils/ifc.h>
#include <android/log.h>

#define LOOPBACK_ADDR	0x7f000001	/* 127.0.0.1 */
#define LOOPBACK_IFNAME	"lo"

#define NTHREADS	8		/* My device has 8 cores */
#define START_PORT	10000

static void set_linger(int fd)
{
	struct linger ln;

	ln.l_onoff = 1;
	ln.l_linger = 0;
	setsockopt(fd, SOL_SOCKET, SO_LINGER, &ln, sizeof ln);
	return;
}

int create_tcp_server(int port)
{
	struct sockaddr_in serv;
	int on = 1;
	int fd;

	fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (fd < 0)
		goto fail;

	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on);

	bzero(&serv, sizeof serv);
	serv.sin_family = AF_INET;
	serv.sin_port = htons((in_port_t)port);
	serv.sin_addr.s_addr = htonl(LOOPBACK_ADDR);

	if (bind(fd, (struct sockaddr *)&serv, sizeof serv) < 0) {
		close(fd);
		goto fail;
	}

	listen(fd, 128);
	return fd;

fail:
	__android_log_print(ANDROID_LOG_ERROR, "test_tcpnuke",
		"create tcp server fail: %s", strerror(errno));
	return -1;
}

void *thread_server(void *arg)
{
	int fd, cli_fd;
	long port;
	int i;

	port = (long) arg;

	while (1) {
		fd = create_tcp_server(port);
		if (fd < 0) {
			sleep(1);
			continue;
		}

		for (i=0; i<100*1000; i++) {
			cli_fd = accept(fd, NULL, NULL);

			if (cli_fd >= 0)
				close(cli_fd);
		}

		close(fd);
	}

	return (void *) 0;
}

int create_tcp_client(int port)
{
	int fd;

	fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (fd < 0) {
		__android_log_print(ANDROID_LOG_ERROR, "test_tcpnuke",
			"create client socket fail: %s", strerror(errno));
		return -1;
	}

	return fd;
}

void *thread_client(void *arg)
{
	struct sockaddr_in serv;
	long port;
	int fd;

	port = (long) arg;

	bzero(&serv, sizeof serv);
	serv.sin_family = AF_INET;
	serv.sin_port = htons((in_port_t)port);
	serv.sin_addr.s_addr = htonl(LOOPBACK_ADDR);

	while (1) {
		fd = create_tcp_client(port);
		if (fd < 0) {
			sleep(1);
			continue;
		}

		connect(fd, (struct sockaddr*)&serv, sizeof serv);
		set_linger(fd);
		close(fd);
	}

	return (void *) 0;
}

void *thread_killaddr(void *arg)
{
	while (1) {
		/*
		 * We want to do SIOCKILLADDR as soon as possible, but netd will
		 * race us with rtnl_lock(), sorry to kill it!
		 */
		/* system("netd=($(ps | grep '/system/bin/netd$')); \
			kill ${netd[1]} 2>&-"); */
		ifc_reset_connections(LOOPBACK_IFNAME, RESET_IPV4_ADDRESSES);
	}

	return (void *) 0;
}

/*
 * Avoid nf table full to drop too many packets.
 */
static void init_nf_param(void)
{
	system("echo 100000 > /proc/sys/net/nf_conntrack_max");
	system("for t in /proc/sys/net/netfilter/nf_conntrack_tcp_timeout* ;\
		 do echo 5 > $t; done");
	return;
}

int main(int argc, char *argv[])
{
	pthread_t tid[2][NTHREADS];
	pthread_t k_tid;
	int run_time = 0;
	long port;
	int i;

	init_nf_param();

	if (argc > 1)
		run_time = atoi(argv[1]);

	for (i=0; i<NTHREADS; i++) {
		port = START_PORT + i;
		pthread_create(&tid[0][i], NULL, thread_server, (void *)port);
		pthread_create(&tid[1][i], NULL, thread_client, (void *)port);
	}
	pthread_create(&k_tid, NULL, thread_killaddr, NULL);

	if (run_time > 0) {
		sleep(run_time);

		exit(0);
	} else {
		void *tret;

		for (i=0; i<NTHREADS; i++) {
			pthread_join(tid[0][i], &tret);
			pthread_join(tid[1][i], &tret);
		}
		pthread_join(k_tid, &tret);
	}

	return 0;
}

