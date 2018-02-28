/* eBPF example program:
 * - creates arraymap in kernel with key 4 bytes and value 8 bytes
 *
 * - loads eBPF program:
 *   r0 = skb->data[ETH_HLEN + offsetof(struct iphdr, protocol)];
 *   *(u32*)(fp - 4) = r0;
 *   // assuming packet is IPv4, lookup ip->proto in a map
 *   value = bpf_map_lookup_elem(map_fd, fp - 4);
 *   if (value)
 *        (*(u64*)value) += 1;
 *
 * - attaches this program to eth0 raw socket
 *
 * - every second user space reads map[tcp], map[udp], map[icmp] to see
 *   how many packets of given protocol were seen on eth0
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
#include "libbpf.h"
#include <pthread.h>
#include <net/if.h>
#include <netdb.h>
#include <sys/resource.h>
#include <linux/if_packet.h>
#include <testUtil.h>
/* the struct used to save the 64 bit tagUid pairs as well as a counter set
currently the counter set is always 0*/
struct UidTag {
  uint32_t uid;
  uint32_t tag;
};

int tp;
#define SO_COOKIE 57
char bpf_log_buf[LOG_BUF_SIZE];

const char *uid_counterSet_map_path = "/sys/fs/bpf/traffic_uid_counterSet_map";

#define handle_error(msg) do { perror(msg); exit(EXIT_FAILURE); } while(0)

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

        sfd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (sfd == -1)
          error(1, errno, "Failed to create socket");

        memset(&addr, 0, sizeof(struct sockaddr_un));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, "/data/local/tmp/fd-pass.socket", sizeof(addr.sun_path) - 1);

	uid_counterSet_map_fd = bpf_obj_get(uid_counterSet_map_path, BPF_F_MAP_WRONLY);
	if (uid_counterSet_map_fd < 0) {
		error(1, errno, "bpf_obj_get(%s): %s(%d)\n",
		uid_counterSet_map_path, strerror(errno), errno);
	}
	if (connect(sfd, (struct sockaddr *) &addr, sizeof(struct sockaddr_un)) == -1)
                error(1, errno, "Failed to connect to socket");
    	size = sock_fd_write(sfd, "1", 1, uid_counterSet_map_fd);
	printf ("wrote %d\n", size);
        return 0;
}
