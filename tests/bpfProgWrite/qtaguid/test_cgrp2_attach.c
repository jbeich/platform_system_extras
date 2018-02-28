/* eBPF example program:
 *
 * - Creates arraymap in kernel with 4 bytes keys and 8 byte values
 *
 * - Loads eBPF program
 *
 *   The eBPF program accesses the map passed in to store two pieces of
 *   information. The number of invocations of the program, which maps
 *   to the number of packets received, is stored to key 0. Key 1 is
 *   incremented on each iteration by the number of bytes stored in
 *   the skb.
 *
 * - Detaches any eBPF program previously attached to the cgroup
 *
 * - Attaches the new program to a cgroup using BPF_PROG_ATTACH
 *
 * - Every second, reads map[0] and map[1] to see how many bytes and
 *   packets were seen on any socket of tasks in the given cgroup.
 */

#define _GNU_SOURCE

#include <inttypes.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <error.h>
#include <linux/bpf.h>
#include <linux/in.h>
#include <linux/if_ether.h>
#include <net/if.h>
#include "libbpf.h"

enum {
	MAP_KEY_PACKETS,
	MAP_KEY_BYTES,
};

struct UidTag {
    uint32_t uid;
    uint32_t tag;
};

struct StatsKey {
    uint32_t uid;
    uint32_t tag;
    uint32_t counterSet;
    uint32_t ifaceIndex;
};

// TODO: verify if framework side still need the detail number about TCP and UDP
// traffic. If not, remove the related tx/rx bytes and packets field to save
// space and simplify the eBPF program.
struct StatsValue {
    uint64_t rxPackets;
    uint64_t rxBytes;
    uint64_t txPackets;
    uint64_t txBytes;
};

const char* cookie_uid_map_path = "/sys/fs/bpf/traffic_cookie_uid_map";
const char* uid_counterSet_map_path = "/sys/fs/bpf/traffic_uid_counterSet_map";
const char* uid_stats_map_path = "/sys/fs/bpf/traffic_uid_stats_map";
const char* tag_stats_map_path = "/sys/fs/bpf/traffic_tag_stats_map";
const char* iface_index_name_map_path = "/sys/fs/bpf/traffic_iface_index_name_map";

int IPV6_PROTOCOL_OFFSET = 6;
int IPV4_PROTOCOL_OFFSET = 9;

int cookie_uid_map_fd, uid_counterSet_map_fd;
int uid_stats_map_fd, tag_stats_map_fd;
int iface_index_name_map_fd;

static void print_cookie_uid_map()
{
        struct UidTag curEntry;
        uint64_t curN = UINT64_MAX;
        uint64_t nextN;
        int res;
	printf("Cookie to tag map:\n");
        while (bpf_get_next_key(cookie_uid_map_fd, &curN, &nextN) > -1) {
                curN = nextN;
                res = bpf_lookup_elem(cookie_uid_map_fd, &curN, &curEntry);
                if (res < 0) {
                        error(1, errno, "fail to get entry value of Key\n");
                } else {
                        printf("cookie: %" PRIu64 ", uid: 0x%u, tag: %u\n", curN, curEntry.uid,
                                curEntry.tag);
                }
        }
        printf("\n");
}

static void print_uid_counterSet_map()
{
        uint32_t curEntry;
        uint32_t curN = UINT32_MAX;
        uint32_t nextN;
        int res;

	printf("uid to CounterSet map:\n");
        while (bpf_get_next_key(uid_counterSet_map_fd, &curN, &nextN) > -1) {
                curN = nextN;
                res = bpf_lookup_elem(uid_counterSet_map_fd, &curN, &curEntry);
                if (res < 0) {
                        error(1, errno, "fail to get entry value of Key: %u\n",
                                curN);
                } else {
                        printf("uid: %u, counterSet: %u\n", curN, curEntry);
                }
        }
        printf("\n");
}

static void print_uid_stats_map()
{
        struct StatsValue curEntry;
        struct StatsKey curN, nextN;
        int res;
        char ifname[100];
        char *if_ptr = ifname;
	memset(&curN, 0, sizeof(struct StatsKey));
	memset(&nextN, 0, sizeof(struct StatsKey));
	printf("uid to stats map:\n");
        while (bpf_get_next_key(uid_stats_map_fd, &curN, &nextN) > -1) {
                curN = nextN;
                res = bpf_lookup_elem(uid_stats_map_fd, &curN, &curEntry);
                if (res < 0) {
                        printf("Key: uid: %u, tag: %u, ifaceIndex: %u, counterSet: %d\n",
                               curN.uid, curN.tag, curN.ifaceIndex, curN.counterSet);
                        error(1, errno, "fail to get entry value of Key\n");
                } else {
                        if_ptr = if_indextoname(curN.ifaceIndex, ifname);
                        printf("Key: uid: %u, tag: %x, iface: %s, counterSet: %d\n",
                               curN.uid, curN.tag, ifname, curN.counterSet);
                        printf("Value: rxPackets: %" SCNu64 ", rxBytes: %" SCNu64 ", txPackets: %" SCNu64 ", txBytes: %" SCNu64 "\n", curEntry.rxPackets, curEntry.rxBytes, curEntry.txPackets, curEntry.txBytes);
                }
        }
        printf("\n");
}

static void print_tag_stats_map()
{
        struct StatsValue curEntry;
        struct StatsKey curN, nextN;
        int res;
        char ifname[100];
        char *if_ptr = ifname;
        memset(&curN, 0, sizeof(struct StatsKey));
        memset(&nextN, 0, sizeof(struct StatsKey));
        printf("tag to stats map:\n");
        while (bpf_get_next_key(tag_stats_map_fd, &curN, &nextN) > -1) {
                curN = nextN;
                res = bpf_lookup_elem(tag_stats_map_fd, &curN, &curEntry);
                if (res < 0) {
                        printf("Key: uid: %u, tag: %x, ifaceIndex: %s, counterSet: %d\n",
                               curN.uid, curN.tag, ifname, curN.counterSet);
                        error(1, errno, "fail to get entry value of Key\n");
                } else {
                        if_ptr = if_indextoname(curN.ifaceIndex, ifname);
                        printf("Key: uid: %u, tag: %x, ifaceIndex: %s, counterSet: %d\n",
                               curN.uid, curN.tag, ifname, curN.counterSet);
                        printf("Value: rxPackets: %" SCNu64 ", rxBytes: %" SCNu64 ", txPackets: %" SCNu64 ", txBytes: %" SCNu64 "\n",
                               curEntry.rxPackets, curEntry.rxBytes, curEntry.txPackets, curEntry.txBytes);
                }
        }
        printf("\n");
}

static void print_iface_index_name_map()
{
        int res;
        uint32_t ifaceIndex = UINT32_MAX;
        uint32_t curIndex = UINT32_MAX;
        char ifname[IF_NAMESIZE];
        printf("iface index to name map:\n");
        while (bpf_get_next_key(iface_index_name_map_fd, &curIndex, &ifaceIndex) > -1) {
                curIndex = ifaceIndex;
                res = bpf_lookup_elem(tag_stats_map_fd, &curIndex, &ifname);
                if (res < 0) {
                        printf("ifaceIndex: %d\n", curIndex);
                        error(1, errno, "fail to get entry value of Key\n");
                } else {
                        printf("ifaceIndex: %u, ifaceName %s\n",
                               curIndex, ifname);
                }
        }
        printf("\n");
}

//static void print_uid_owner_map()
//{
//        struct uidOwnerKey curKey, nextKey;
//        uint32_t value;
//        int res;
//        char ifname[100];
//        char *if_ptr = ifname;
//        memset(&curKey, 0, sizeof(struct uidOwnerKey));
//        memset(&nextKey, 0, sizeof(struct uidOwnerKey));
//        printf("uid owner map:\n");
//        while (bpf_get_next_key(uid_owner_egress_map_fd, &curKey, &nextKey) > -1) {
//                curKey = nextKey;
//                res = bpf_lookup_elem(uid_owner_egress_map_fd, &curKey, &value);
//                if (res < 0) {
//                        printf("Key: uid: %u, ifaceIndex: %u\n",
//                               curKey.uid, curKey.ifaceIndex);
//                        error(1, errno, "fail to get entry value of Key\n");
//                } else {
//                        if_ptr = if_indextoname(curKey.ifaceIndex, ifname);
//                        printf("Key: uid: %u, iface: %s\n",
//                               curKey.uid, ifname);
//                        printf("Value: %u\n", value);
//                }
//        }
//        printf("\n");
//        memset(&curKey, 0, sizeof(struct uidOwnerKey));
//        memset(&nextKey, 0, sizeof(struct uidOwnerKey));
//        printf("uid owner ingress map:\n");
//        while (bpf_get_next_key(uid_owner_ingress_map_fd, &curKey, &nextKey) > -1) {
//                curKey = nextKey;
//                res = bpf_lookup_elem(uid_owner_ingress_map_fd, &curKey, &value);
//                if (res < 0) {
//                        printf("Key: uid: %u, ifaceIndex: %u\n",
//                               curKey.uid, curKey.ifaceIndex);
//                        error(1, errno, "fail to get entry value of Key\n");
//                } else {
//                        if_ptr = if_indextoname(curKey.ifaceIndex, ifname);
//                        printf("Key: uid: %u, iface: %s\n",
//                               curKey.uid, ifname);
//                        printf("Value: %u\n", value);
//                }
//        }
//        printf("\n");
//}

int main()
{
    cookie_uid_map_fd = bpf_obj_get(cookie_uid_map_path);
    if (cookie_uid_map_fd < 0) {
            error(1, errno, "bpf_obj_get(%s): %s(%d)\n",
            cookie_uid_map_path, strerror(errno), errno);
    }
    uid_counterSet_map_fd = bpf_obj_get(uid_counterSet_map_path);
    if (uid_counterSet_map_fd < 0) {
            error(1, errno, "bpf_obj_get(%s): %s(%d)\n",
            uid_counterSet_map_path, strerror(errno), errno);
    }
  uid_stats_map_fd = bpf_obj_get(uid_stats_map_path);
  if (uid_stats_map_fd < 0) {
          error(1, errno, "bpf_obj_get(%s): %s(%d)\n",
          uid_stats_map_path, strerror(errno), errno);
  }
    tag_stats_map_fd = bpf_obj_get(tag_stats_map_path);
    if (tag_stats_map_fd < 0) {
            error(1, errno, "bpf_obj_get(%s): %s(%d)\n",
            tag_stats_map_path, strerror(errno), errno);
    }
    iface_index_name_map_fd = bpf_obj_get(iface_index_name_map_path);
    if (iface_index_name_map_fd < 0) {
            error(1, errno, "bpf_obj_get(%s): %s(%d)\n",
            iface_index_name_map_path, strerror(errno), errno);
    }

//    uid_owner_egress_map_fd = bpf_obj_get(uid_owner_egress_map_path);
//    if (uid_owner_egress_map_fd < 0) {
//            error(1, errno, "bpf_obj_get(%s): %s(%d)\n",
//            uid_owner_egress_map_path, strerror(errno), errno);
//    }

//    uid_owner_ingress_map_fd = bpf_obj_get(uid_owner_ingress_map_path);
//    if (uid_owner_ingress_map_fd < 0) {
//            error(1, errno, "bpf_obj_get(%s): %s(%d)\n",
//            uid_owner_ingress_map_path, strerror(errno), errno);
//    }
  for(;;) {
      print_cookie_uid_map();
      print_uid_counterSet_map();
      print_uid_stats_map();
      print_tag_stats_map();
      print_iface_index_name_map();
      sleep(1);
    }
}
