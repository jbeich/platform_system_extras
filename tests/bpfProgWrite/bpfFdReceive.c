/* eBPF example program:
 * Receive a eBPF map fd and try to read/write to it. Use to verify selinux
 * rules and file mode of eBPF object.
 */
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <linux/bpf.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <error.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stddef.h>
#include <inttypes.h>
#include "cutils/libbpf.h"
#include <pthread.h>
#include <net/if.h>
#include <netdb.h>
#include <sys/resource.h>
#include <linux/if_packet.h>
#include <testUtil.h>

const char *uid_counterSet_map_path = "/sys/fs/bpf/traffic_uid_counterSet_map";

ssize_t
sock_fd_read(int sock, void *buf, ssize_t bufsize, int *fd)
{
    ssize_t size;

    if (fd) {
        struct msghdr msg;
        struct iovec iov;
        union {
            struct cmsghdr  cmsghdr;
            char control[CMSG_SPACE(sizeof (int))];
        } cmsgu;
        struct cmsghdr *cmsg;

        iov.iov_base = buf;
        iov.iov_len = bufsize;

        msg.msg_name = NULL;
        msg.msg_namelen = 0;
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_control = cmsgu.control;
        msg.msg_controllen = sizeof(cmsgu.control);
        size = recvmsg (sock, &msg, 0);
        if (size < 0) {
            perror ("recvmsg");
            exit(1);
        }
        cmsg = CMSG_FIRSTHDR(&msg);
        if (cmsg && cmsg->cmsg_len == CMSG_LEN(sizeof(int))) {
            if (cmsg->cmsg_level != SOL_SOCKET) {
                fprintf (stderr, "invalid cmsg_level %d\n",
                     cmsg->cmsg_level);
                exit(1);
            }
            if (cmsg->cmsg_type != SCM_RIGHTS) {
                fprintf (stderr, "invalid cmsg_type %d\n",
                     cmsg->cmsg_type);
                exit(1);
            }

            *fd = *((int *) CMSG_DATA(cmsg));
            printf ("received fd %d\n", *fd);
        } else
            *fd = -1;
    } else {
        size = read (sock, buf, bufsize);
        if (size < 0) {
            perror("read");
            exit(1);
        }
    }
    return size;
}

int
main(int argc, char **argv)
{
    ssize_t nbytes;
    char buffer[256];
    int sfd, cfd, *fds;
    struct sockaddr_un addr;

    int uid_counterSet_map_fd;
    ssize_t size;
    int res;
    char buf[16];
    uint32_t uid = 0;
    uint32_t counterSet = testRand()%10;

    sfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sfd == -1)
            error(1, errno, "Failed to create socket");

    if (unlink ("/data/local/tmp/fd-pass.socket") == -1 && errno != ENOENT)
            error(1, errno, "Removing socket file failed");

    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, "/data/local/tmp/fd-pass.socket", sizeof(addr.sun_path) -1);

    if (bind(sfd, (struct sockaddr *) &addr, sizeof(struct sockaddr_un)) == -1)
            error(1, errno, "Failed to bind to socket");

    if (listen(sfd, 5) == -1)
            error(1, errno, "Failed to listen on socket");

    cfd = accept(sfd, NULL, NULL);
    if (cfd == -1)
            error(1, errno, "Failed to accept incoming connection");

    // Try to receive the fd and update the map, if the map passed is read only,
    // Should fail. The it try to lookup the entry just updated, if the map is
    // write only, should fail.
    sleep(1);
    for (;;) {
        size = sock_fd_read(cfd, buf, sizeof(buf), &uid_counterSet_map_fd);
        if (size <= 0)
            break;
        printf ("read %d\n", size);
        if (uid_counterSet_map_fd != -1) {
            res = bpf_update_elem(uid_counterSet_map_fd, &uid, &counterSet, BPF_ANY);
            if (res < 0)
                error(1, errno, "update counter set failed\n");
            res = bpf_lookup_elem(uid_counterSet_map_fd, &uid, &counterSet);
            if (res < 0) {
                error(1, errno, "fail to get entry value of Key: %u\n",
                      uid);
            } else {
                printf("uid: %u, counterSet: %u\n", uid, counterSet);
            }
            close(uid_counterSet_map_fd);
        }
    }

    if (close(cfd) == -1)
            error(1, errno, "Failed to close client socket");

    return 0;
}
