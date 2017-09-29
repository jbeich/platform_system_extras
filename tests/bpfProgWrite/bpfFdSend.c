/* eBPF example program:
 * Get a eBPF map and send out through unix socket. Use to verify selinux rules
 * and eBPF file mode on eBPF object.
 */
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <linux/bpf.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <error.h>

#include <sys/un.h>
#include <sys/socket.h>
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
#include <testUtil.h>;

char bpf_log_buf[LOG_BUF_SIZE];

const char *uid_counterSet_map_path = "/sys/fs/bpf/traffic_uid_counterSet_map";

ssize_t
sock_fd_write(int sock, void *buf, ssize_t buflen, int fd)
{
    ssize_t     size;
    struct msghdr   msg;
    struct iovec    iov;
    union {
        struct cmsghdr  cmsghdr;
        char        control[CMSG_SPACE(sizeof (int))];
    } cmsgu;
    struct cmsghdr  *cmsg;

    iov.iov_base = buf;
    iov.iov_len = buflen;

    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    if (fd != -1) {
        msg.msg_control = cmsgu.control;
        msg.msg_controllen = sizeof(cmsgu.control);

        cmsg = CMSG_FIRSTHDR(&msg);
        cmsg->cmsg_len = CMSG_LEN(sizeof (int));
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;

        printf ("passing fd %d\n", fd);
        *((int *) CMSG_DATA(cmsg)) = fd;
    } else {
        msg.msg_control = NULL;
        msg.msg_controllen = 0;
        printf ("not passing fd\n");
    }

    size = sendmsg(sock, &msg, 0);

    if (size < 0)
        perror ("sendmsg");
    return size;
}

int
main(int argc, char *argv[]) {
    int sfd, size;
    struct sockaddr_un addr;
    int uid_counterSet_map_fd;

    //set up the unix socket
    sfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sfd == -1)
      error(1, errno, "Failed to create socket");

    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, "/data/local/tmp/fd-pass.socket", sizeof(addr.sun_path) - 1);

    //get map from a pinned location.
    uid_counterSet_map_fd = bpf_obj_get(uid_counterSet_map_path, BPF_F_MAP_RDONLY);
    if (uid_counterSet_map_fd < 0) {
        //map not exist, create one
        uid_counterSet_map_fd = bpf_create_map(BPF_MAP_TYPE_HASH, sizeof(int), sizeof(int),
                                10, BPF_F_MAP_RDONLY);
        if (map_fd < 0)
            error (1, errno, "map create failed!\n");
    }
    //connect to the unix socket and send the fd.
    if (connect(sfd, (struct sockaddr *) &addr, sizeof(struct sockaddr_un)) == -1)
            error(1, errno, "Failed to connect to socket");
    size = sock_fd_write(sfd, "1", 1, uid_counterSet_map_fd);
    printf ("wrote %d\n", size);
    return 0;
}
