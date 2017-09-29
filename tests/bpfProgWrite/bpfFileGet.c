/* eBPF example program:
 * Try to get a eBPF map from a pinned location with write only flag and try to
 * read and write to it. used to verify selinux rules and file mode of eBPF
 * object
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

int main(void)
{
    int res, uid_counterSet_map_fd;
    const char* uid_counterSet_map_path = "/sys/fs/bpf/traffic_uid_counterSet_map";
    uint32_t uid = 0;
    uint32_t counterSet = testRand()%10;
    if (res < 0)
        printf("get cookie failed: %s\n", strerror(errno));
    //get a map write only.
    uid_counterSet_map_fd = bpf_obj_get(uid_counterSet_map_path, BPF_F_MAP_WRONLY);
    if (uid_counterSet_map_fd < 0) {
            error(1, errno, "bpf_obj_get(%s): %s(%d)\n",
            uid_counterSet_map_path, strerror(errno), errno);
    }

    //Try to update the map content, should success.
    res = bpf_update_elem(uid_counterSet_map_fd, &uid, &counterSet, BPF_ANY);
    if (res < 0)
        error(1, errno, "update counter set failed\n");

    //Try to look up the map content just updated, should fail.
    res = bpf_lookup_elem(uid_counterSet_map_fd, &uid, &counterSet);
    if (res < 0) {
        error(1, errno, "fail to get entry value of Key: %u\n",
              curN);
    } else {
        printf("uid: %u, counterSet: %u\n", uid, counterSet);
    }
    return res;
}
