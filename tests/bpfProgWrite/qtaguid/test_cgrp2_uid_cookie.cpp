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
 * - Attaches the new program to a cgroup using BPF_PROG_ATTACH
 *
 * - Every second, reads map[0] and map[1] to see how many bytes and
 *   packets were seen on any socket of tasks in the given cgroup.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <error.h>
#include <fcntl.h>
#include <stdint.h>
#include <arpa/inet.h>

#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/unistd.h>
#include <linux/in.h>

#include <linux/bpf.h>
#include <sys/types.h>
#include "BpfUtils.h"
#include <sys/time.h>
#include <sys/resource.h>

#define ptr_to_u64(x) ((uint64_t)(uintptr_t)x)
#define LOG_BUF_SIZE 65536

const int IPV6_TRANSPORT_PROTOCOL_OFFSET = 6;
const int IPV4_TRANSPORT_PROTOCOL_OFFSET = 9;

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

struct Stats {
  uint64_t  rxTcpPackets;
  uint64_t  rxTcpBytes;
  uint64_t  txTcpPackets;
  uint64_t  txTcpBytes;
  uint64_t  rxUdpPackets;
  uint64_t  rxUdpBytes;
  uint64_t  txUdpPackets;
  uint64_t  txUdpBytes;
  uint64_t  rxOtherPackets;
  uint64_t  rxOtherBytes;
  uint64_t  txOtherPackets;
  uint64_t  txOtherBytes;
};

int cookieTagMap;
int uidCounterSetMap;
int uidStatsMap;
int tagStatsMap;

char bpf_log_buf[LOG_BUF_SIZE];
const char* cookie_uid_map_path = "/sys/fs/bpf/traffic_cookie_uid_map";
const char* uid_counterSet_map_path = "/sys/fs/bpf/traffic_uid_counterSet_map";
const char* uid_stats_map_path = "/sys/fs/bpf/traffic_uid_stats_map";
const char* tag_stats_map_path = "/sys/fs/bpf/traffic_tag_stats_map";

int createMap(enum bpf_map_type map_type, int key_size, int value_size,
              int max_entries, int map_flags) {
        union bpf_attr attr;

        memset(&attr, 0, sizeof(attr));
        attr.map_type = map_type;
        attr.key_size = key_size;
        attr.value_size = value_size;
        attr.max_entries = max_entries;
        attr.map_flags = map_flags;
        return syscall(__NR_bpf, BPF_MAP_CREATE, &attr, sizeof(attr));
}

int writeToMapEntry(int fd, void *key, void *value, unsigned long long flags) {
        union bpf_attr attr;

        memset(&attr, 0, sizeof(attr));
        attr.map_fd = fd;
        attr.key = ptr_to_u64(key);
        attr.value = ptr_to_u64(value);
        attr.flags = flags;

        return syscall(__NR_bpf, BPF_MAP_UPDATE_ELEM, &attr, sizeof(attr));
}

int findMapEntry(int fd, void *key, void *value) {
        union bpf_attr attr;

        memset(&attr, 0, sizeof(attr));
        attr.map_fd = fd;
        attr.key = ptr_to_u64(key);
        attr.value = ptr_to_u64(value);

        return syscall(__NR_bpf, BPF_MAP_LOOKUP_ELEM, &attr, sizeof(attr));
}

int deleteMapEntry(int fd, void *key) {
        union bpf_attr attr;

        memset(&attr, 0, sizeof(attr));
        attr.map_fd = fd;
        attr.key = ptr_to_u64(key);

        return syscall(__NR_bpf, BPF_MAP_DELETE_ELEM, &attr, sizeof(attr));
}

int getNextMapKey(int fd, void *key, void *next_key) {
        union bpf_attr attr;

        memset(&attr, 0, sizeof(attr));
        attr.map_fd = fd;
        attr.key = ptr_to_u64(key);
        attr.next_key = ptr_to_u64(next_key);

        return syscall(__NR_bpf, BPF_MAP_GET_NEXT_KEY, &attr, sizeof(attr));
}

int bpfProgLoad(enum bpf_prog_type prog_type,
                const struct bpf_insn *insns, int prog_len,
                const char *license, int kern_version, char* bpf_log_buf,
                int buf_size) {
        union bpf_attr attr;

        memset(&attr, 0, sizeof(attr));
        attr.prog_type = prog_type;
        attr.insns = ptr_to_u64((void *) insns);
        attr.insn_cnt = prog_len;
        attr.license = ptr_to_u64((void *) license);
        attr.log_buf = ptr_to_u64(bpf_log_buf);
        attr.log_size = buf_size;
        attr.log_level = 1;
        attr.kern_version = kern_version;

        int ret =  syscall(__NR_bpf, BPF_PROG_LOAD, &attr, sizeof(attr));
        return ret;
}

int mapPin(int fd, const char *pathname) {
        union bpf_attr attr;

        memset(&attr, 0, sizeof(attr));
        attr.pathname = ptr_to_u64((void *)pathname);
        attr.bpf_fd = fd;

        return syscall(__NR_bpf, BPF_OBJ_PIN, &attr, sizeof(attr));
}

int mapRetrieve(const char *pathname, int) {
        union bpf_attr attr;

        memset(&attr, 0, sizeof(attr));
        attr.pathname = ptr_to_u64((void *)pathname);
        return syscall(__NR_bpf, BPF_OBJ_GET, &attr, sizeof(attr));
}

int egressProgramLoad() {
    printf("cookieTagMap: %p\n", &cookieTagMap);
    struct bpf_insn prog[] = {
        /*
         * Save sk_buff for future usage. value stored in R6 to R10 will
         * not be reset after a bpf helper function call.
         */
        BPF_INS_BLK(REG_MOV64, BPF_REG_6, BPF_REG_1, 0, 0),
        /*
         * pc1: BPF_FUNC_get_socket_cookie takes one parameter,
         * R1: sk_buff
         */
        BPF_INS_BLK(BPF_JMP | BPF_CALL, 0, 0, 0, BPF_FUNC_get_socket_cookie),
        /* pc2-4: save &socketCookie to r7 for future usage*/
        BPF_INS_BLK(MEM_SET_BY_REG(BPF_DW), BPF_REG_10, BPF_REG_0, -8, 0),
        BPF_INS_BLK(REG_MOV64, BPF_REG_7, BPF_REG_10, 0, 0),
        BPF_INS_BLK(VAL_ALU64(BPF_ADD), BPF_REG_7, 0, 0, -8),
        /*
         * pc5-8: set up the registers for BPF_FUNC_map_lookup_elem,
         * it takes two parameters (R1: map_fd,  R2: &socket_cookie)
         */
        LOAD_MAP_FD(BPF_REG_1, (uint32_t)cookieTagMap),
        BPF_INS_BLK(REG_MOV64, BPF_REG_2, BPF_REG_7, 0, 0),
        BPF_INS_BLK(BPF_JMP | BPF_CALL, 0, 0, 0, BPF_FUNC_map_lookup_elem),
        /*
         * pc9. if r0 != 0x0, go to pc+14, since we have the cookie
         * stored already
         * Otherwise do pc10-22 to setup a new data entry.
         */
        BPF_INS_BLK(VAL_ALU_JMP(BPF_JNE), BPF_REG_0, 0, 1, 0), //10
        BPF_INS_BLK(VAL_ALU_JMP(BPF_JA), 0, 0, 81, 0),
        BPF_INS_BLK(REG_MOV64, BPF_REG_8, BPF_REG_0, 0, 0),
        LOAD_MAP_FD(BPF_REG_7, tagStatsMap),
        BPF_INS_BLK(MEM_LD(BPF_W), BPF_REG_2, BPF_REG_8,
                    static_cast<__s16>(offsetof(struct UidTag, uid)), 0),
        BPF_INS_BLK(MEM_SET_BY_REG(BPF_W), BPF_REG_10, BPF_REG_2, -132, 0),
        BPF_INS_BLK(REG_MOV64, BPF_REG_2, BPF_REG_10, 0, 0),
        BPF_INS_BLK(VAL_ALU64(BPF_ADD), BPF_REG_2, 0, 0, -132),
        LOAD_MAP_FD(BPF_REG_1, uidCounterSetMap), //20
        BPF_INS_BLK(BPF_JMP | BPF_CALL, 0, 0, 0,
                        BPF_FUNC_map_lookup_elem),
        BPF_INS_BLK(VAL_ALU_JMP(BPF_JNE), BPF_REG_0, 0, 2, 0),
        BPF_INS_BLK(MEM_SET_BY_VAL(BPF_W), BPF_REG_10, 0, -32
                   + static_cast<__s16>(offsetof(struct StatsKey, counterSet)), 0),
        BPF_INS_BLK(VAL_ALU_JMP(BPF_JA), 0, 0, 2, 0),
        BPF_INS_BLK(MEM_LD(BPF_B), BPF_REG_1, BPF_REG_0, 0, 0),
        BPF_INS_BLK(MEM_SET_BY_REG(BPF_W), BPF_REG_10, BPF_REG_1, -32
                    + static_cast<__s16>(offsetof(struct StatsKey, counterSet)), 0),
        BPF_INS_BLK(MEM_LD(BPF_W), BPF_REG_2, BPF_REG_6,
                      static_cast<__s16>(offsetof(struct __sk_buff, ifindex)), 0),
        BPF_INS_BLK(MEM_LD(BPF_W), BPF_REG_3, BPF_REG_8,
                      static_cast<__s16>(offsetof(struct UidTag, uid)), 0),
        BPF_INS_BLK(MEM_LD(BPF_W), BPF_REG_4, BPF_REG_8,
                      static_cast<__s16>(offsetof(struct UidTag, tag)), 0),
        BPF_INS_BLK(MEM_SET_BY_REG(BPF_W), BPF_REG_10, BPF_REG_2, -32
                    + static_cast<__s16>(offsetof(struct StatsKey, ifaceIndex)), 0), //30
        BPF_INS_BLK(MEM_SET_BY_REG(BPF_W), BPF_REG_10, BPF_REG_3, -32
                    + static_cast<__s16>(offsetof(struct StatsKey, uid)), 0),
        BPF_INS_BLK(MEM_SET_BY_REG(BPF_W), BPF_REG_10, BPF_REG_4, -32
                    + static_cast<__s16>(offsetof(struct StatsKey, tag)), 0),
        BPF_INS_BLK(REG_MOV64, BPF_REG_9, BPF_REG_10, 0, 0),
        BPF_INS_BLK(VAL_ALU64(BPF_ADD), BPF_REG_9, 0, 0, -32),
        BPF_INS_BLK(REG_MOV64, BPF_REG_1, BPF_REG_7, 0, 0),
        BPF_INS_BLK(REG_MOV64, BPF_REG_2, BPF_REG_9, 0, 0),
        BPF_INS_BLK(BPF_JMP | BPF_CALL, 0, 0, 0, BPF_FUNC_map_lookup_elem),
        BPF_INS_BLK(VAL_ALU_JMP(BPF_JNE), BPF_REG_0, 0, 23, 0),
        BPF_INS_BLK(MEM_SET_BY_VAL(BPF_DW), BPF_REG_10, 0,
                    -128 + static_cast<__s16>(offsetof(struct Stats, rxTcpBytes)), 0),
        BPF_INS_BLK(MEM_SET_BY_VAL(BPF_DW), BPF_REG_10, 0,
                    -128 + static_cast<__s16>(offsetof(struct Stats, rxTcpPackets)), 0), //40
        BPF_INS_BLK(MEM_SET_BY_VAL(BPF_DW), BPF_REG_10, 0,
                    -128 + static_cast<__s16>(offsetof(struct Stats, rxUdpBytes)), 0),
        BPF_INS_BLK(MEM_SET_BY_VAL(BPF_DW), BPF_REG_10, 0,
                    -128 + static_cast<__s16>(offsetof(struct Stats, rxUdpPackets)), 0),
        BPF_INS_BLK(MEM_SET_BY_VAL(BPF_DW), BPF_REG_10, 0,
                    -128 + static_cast<__s16>(offsetof(struct Stats, txTcpPackets)), 0),
        BPF_INS_BLK(MEM_SET_BY_VAL(BPF_DW), BPF_REG_10, 0,
                    -128 + static_cast<__s16>(offsetof(struct Stats, txTcpBytes)), 0),
        BPF_INS_BLK(MEM_SET_BY_VAL(BPF_DW), BPF_REG_10, 0,
                    -128 + static_cast<__s16>(offsetof(struct Stats, txUdpPackets)), 0),
        BPF_INS_BLK(MEM_SET_BY_VAL(BPF_DW), BPF_REG_10, 0,
                    -128 + static_cast<__s16>(offsetof(struct Stats, txUdpBytes)), 0),
        BPF_INS_BLK(MEM_SET_BY_VAL(BPF_DW), BPF_REG_10, 0,
                    -128 + static_cast<__s16>(offsetof(struct Stats, rxOtherPackets)), 0),
        BPF_INS_BLK(MEM_SET_BY_VAL(BPF_DW), BPF_REG_10, 0,
                    -128 + static_cast<__s16>(offsetof(struct Stats, rxOtherBytes)), 0),
        BPF_INS_BLK(MEM_SET_BY_VAL(BPF_DW), BPF_REG_10, 0,
                    -128 + static_cast<__s16>(offsetof(struct Stats, txOtherBytes)), 0),
        BPF_INS_BLK(MEM_SET_BY_VAL(BPF_DW), BPF_REG_10, 0,
                    -128 + static_cast<__s16>(offsetof(struct Stats, txOtherPackets)), 0), //50
        /*
         * add new map entry using BPF_FUNC_map_update_elem, it takes
         * 4 parameters (R1: map_fd, R2: &socket_cookie, R3: &stats,
         * R4: flags)
         */
        BPF_INS_BLK(REG_MOV64, BPF_REG_1, BPF_REG_7, 0, 0),
        BPF_INS_BLK(REG_MOV64, BPF_REG_2, BPF_REG_9, 0, 0),
        BPF_INS_BLK(REG_MOV64, BPF_REG_3, BPF_REG_10, 0, 0),
        BPF_INS_BLK(VAL_ALU64(BPF_ADD), BPF_REG_3, 0, 0, -128),
        BPF_INS_BLK(VAL_MOV64, BPF_REG_4, 0, 0, 0),
        BPF_INS_BLK(BPF_JMP | BPF_CALL, 0, 0, 0, BPF_FUNC_map_update_elem),
        BPF_INS_BLK(REG_MOV64, BPF_REG_1, BPF_REG_7, 0, 0),
        BPF_INS_BLK(REG_MOV64, BPF_REG_2, BPF_REG_9, 0, 0),
        BPF_INS_BLK(BPF_JMP | BPF_CALL, 0, 0, 0, BPF_FUNC_map_lookup_elem),
        BPF_INS_BLK(VAL_ALU_JMP(BPF_JNE), BPF_REG_0, 0, 1, 0), //60
        BPF_INS_BLK(VAL_ALU_JMP(BPF_JA), 0, 0, 56, 0),
        /*
         * pc24-30 update the packet info to a exist data entry, it can
         * be done by directly write to pointers instead of using
         * BPF_FUNC_map_update_elem helper function
         */
        BPF_INS_BLK(REG_MOV64, BPF_REG_9, BPF_REG_0, 0, 0),
        BPF_INS_BLK(MEM_LD(BPF_W), BPF_REG_7, BPF_REG_6,
                    static_cast<__s16>(offsetof(struct __sk_buff, len)), 0),
        BPF_INS_BLK(MEM_LD(BPF_W), BPF_REG_1, BPF_REG_6,
                    static_cast<__s16>(offsetof(struct __sk_buff, protocol)), 0),
        BPF_INS_BLK(VAL_ALU_JMP(BPF_JNE), BPF_REG_1, 0, 7, htons(ETH_P_IP)),
        BPF_INS_BLK(REG_MOV64, BPF_REG_1, BPF_REG_6, 0, 0),
        BPF_INS_BLK(VAL_MOV64, BPF_REG_2, 0, 0, IPV4_TRANSPORT_PROTOCOL_OFFSET),
        BPF_INS_BLK(REG_MOV64, BPF_REG_3, BPF_REG_10, 0, 0),
        BPF_INS_BLK(VAL_ALU64(BPF_ADD), BPF_REG_3, 0, 0, -133),
        BPF_INS_BLK(VAL_MOV64, BPF_REG_4, 0, 0, 1), //70
        BPF_INS_BLK(BPF_JMP | BPF_CALL, 0, 0, 0, BPF_FUNC_skb_load_bytes),
        BPF_INS_BLK(VAL_ALU_JMP(BPF_JA), 0, 0, 7, 0),
        BPF_INS_BLK(VAL_ALU_JMP(BPF_JNE), BPF_REG_1, 0, 15, htons(ETH_P_IPV6)),
        BPF_INS_BLK(REG_MOV64, BPF_REG_1, BPF_REG_6, 0, 0),
        BPF_INS_BLK(VAL_MOV64, BPF_REG_2, 0, 0, IPV6_TRANSPORT_PROTOCOL_OFFSET),
        BPF_INS_BLK(REG_MOV64, BPF_REG_3, BPF_REG_10, 0, 0),
        BPF_INS_BLK(VAL_ALU64(BPF_ADD), BPF_REG_3, 0, 0, -133),
        BPF_INS_BLK(VAL_MOV64, BPF_REG_4, 0, 0, 1),
        BPF_INS_BLK(BPF_JMP | BPF_CALL, 0, 0, 0, BPF_FUNC_skb_load_bytes),
        BPF_INS_BLK(VAL_MOV64, BPF_REG_5, 0, 0, 1), //80
        BPF_INS_BLK(MEM_LD(BPF_B), BPF_REG_0, BPF_REG_10, -133, 0),
        BPF_INS_BLK(VAL_ALU_JMP(BPF_JNE), BPF_REG_0, 0, 3, IPPROTO_TCP),
        BPF_INS_BLK(REG_ATOMIC_ADD(BPF_DW), BPF_REG_9, BPF_REG_5,
                     static_cast<__s16>(offsetof(struct Stats, rxTcpPackets)), 0),
        BPF_INS_BLK(REG_ATOMIC_ADD(BPF_DW), BPF_REG_9, BPF_REG_8,
                     static_cast<__s16>(offsetof(struct Stats, rxTcpBytes)), 0),
        BPF_INS_BLK(VAL_ALU_JMP(BPF_JA), 0, 0, 6, 0),
        BPF_INS_BLK(VAL_ALU_JMP(BPF_JNE), BPF_REG_0, 0, 3, IPPROTO_UDP),
        BPF_INS_BLK(REG_ATOMIC_ADD(BPF_DW), BPF_REG_9, BPF_REG_5,
                     static_cast<__s16>(offsetof(struct Stats, rxUdpPackets)), 0),
        BPF_INS_BLK(REG_ATOMIC_ADD(BPF_DW), BPF_REG_9, BPF_REG_8,
                     static_cast<__s16>(offsetof(struct Stats, rxUdpBytes)), 0),
        BPF_INS_BLK(VAL_ALU_JMP(BPF_JA), 0, 0, 2, 0),
        BPF_INS_BLK(REG_ATOMIC_ADD(BPF_DW), BPF_REG_9, BPF_REG_5,
                     static_cast<__s16>(offsetof(struct Stats, rxOtherPackets)), 0),
        BPF_INS_BLK(REG_ATOMIC_ADD(BPF_DW), BPF_REG_9, BPF_REG_8,
                     static_cast<__s16>(offsetof(struct Stats, rxOtherBytes)), 0), //90
        BPF_INS_BLK(VAL_ALU_JMP(BPF_JA), 0, 0, 25, 0),

        BPF_INS_BLK(REG_MOV64, BPF_REG_1, BPF_REG_6, 0, 0),
        BPF_INS_BLK(BPF_JMP | BPF_CALL, 0, 0, 0, BPF_FUNC_get_socket_uid),
        BPF_INS_BLK(MEM_SET_BY_REG(BPF_W), BPF_REG_10, BPF_REG_0,
                    -16 + static_cast<__s16>(offsetof(struct UidTag, uid)), 0),
        BPF_INS_BLK(MEM_SET_BY_VAL(BPF_W), BPF_REG_10, 0,
                    -16 + static_cast<__s16>(offsetof(struct UidTag, tag)), 0),
        BPF_INS_BLK(REG_MOV64, BPF_REG_8, BPF_REG_10, 0, 0),
        BPF_INS_BLK(VAL_ALU64(BPF_ADD), BPF_REG_8, 0, 0, -16),
        BPF_INS_BLK(MEM_LD(BPF_W), BPF_REG_2, BPF_REG_8,
                    static_cast<__s16>(offsetof(struct UidTag, uid)), 0),
        BPF_INS_BLK(MEM_SET_BY_REG(BPF_W), BPF_REG_10, BPF_REG_2, -132, 0),
        BPF_INS_BLK(REG_MOV64, BPF_REG_2, BPF_REG_10, 0, 0), //100
        BPF_INS_BLK(VAL_ALU64(BPF_ADD), BPF_REG_2, 0, 0, -132),
        LOAD_MAP_FD(BPF_REG_1, uidCounterSetMap),
        BPF_INS_BLK(BPF_JMP | BPF_CALL, 0, 0, 0, BPF_FUNC_map_lookup_elem),
        BPF_INS_BLK(VAL_ALU_JMP(BPF_JNE), BPF_REG_0, 0, 2, 0),
        BPF_INS_BLK(MEM_SET_BY_VAL(BPF_W), BPF_REG_10, 0,
                    -32 + static_cast<__s16>(offsetof(struct StatsKey, counterSet)), 0),
        BPF_INS_BLK(VAL_ALU_JMP(BPF_JA), 0, 0, 2, 0),
        BPF_INS_BLK(MEM_LD(BPF_B), BPF_REG_1, BPF_REG_0, 0, 0),
        BPF_INS_BLK(MEM_SET_BY_REG(BPF_W), BPF_REG_10, BPF_REG_1,
                    -32 + static_cast<__s16>(offsetof(struct StatsKey, counterSet)), 0),
        BPF_INS_BLK(MEM_LD(BPF_W), BPF_REG_2, BPF_REG_6,
                    static_cast<__s16>(offsetof(struct __sk_buff, ifindex)), 0),
        BPF_INS_BLK(MEM_LD(BPF_W), BPF_REG_3, BPF_REG_8,
                    static_cast<__s16>(offsetof(struct UidTag, uid)), 0), // 110
        BPF_INS_BLK(MEM_LD(BPF_W), BPF_REG_4, BPF_REG_8,
                    static_cast<__s16>(offsetof(struct UidTag, tag)), 0),
        BPF_INS_BLK(MEM_SET_BY_REG(BPF_W), BPF_REG_10, BPF_REG_2,
                    -32 + static_cast<__s16>(offsetof(struct StatsKey, ifaceIndex)), 0),
        BPF_INS_BLK(MEM_SET_BY_REG(BPF_W), BPF_REG_10, BPF_REG_3,
                    -32 + static_cast<__s16>(offsetof(struct StatsKey, uid)), 0),
        BPF_INS_BLK(MEM_SET_BY_REG(BPF_W), BPF_REG_10, BPF_REG_4,
                    -32 + static_cast<__s16>(offsetof(struct StatsKey, tag)), 0),
        BPF_INS_BLK(VAL_ALU_JMP(BPF_JA), 0, 0, 1, 0),
        BPF_INS_BLK(MEM_SET_BY_VAL(BPF_W), BPF_REG_10, 0,
                    -32 + static_cast<__s16>(offsetof(struct StatsKey, tag)), 0),
        LOAD_MAP_FD(BPF_REG_7, uidStatsMap),
        BPF_INS_BLK(REG_MOV64, BPF_REG_9, BPF_REG_10, 0, 0),
        BPF_INS_BLK(VAL_ALU64(BPF_ADD), BPF_REG_9, 0, 0, -32), //120
        BPF_INS_BLK(REG_MOV64, BPF_REG_1, BPF_REG_7, 0, 0),
        BPF_INS_BLK(REG_MOV64, BPF_REG_2, BPF_REG_9, 0, 0),
        BPF_INS_BLK(BPF_JMP | BPF_CALL, 0, 0, 0, BPF_FUNC_map_lookup_elem),
        BPF_INS_BLK(VAL_ALU_JMP(BPF_JNE), BPF_REG_0, 0, 24, 0),
        BPF_INS_BLK(MEM_SET_BY_VAL(BPF_DW), BPF_REG_10, 0,
                    -128 + static_cast<__s16>(offsetof(struct Stats, rxTcpBytes)), 0),
        BPF_INS_BLK(MEM_SET_BY_VAL(BPF_DW), BPF_REG_10, 0,
                    -128 + static_cast<__s16>(offsetof(struct Stats, rxTcpPackets)), 0),
        BPF_INS_BLK(MEM_SET_BY_VAL(BPF_DW), BPF_REG_10, 0,
                    -128 + static_cast<__s16>(offsetof(struct Stats, rxUdpBytes)), 0),
        BPF_INS_BLK(MEM_SET_BY_VAL(BPF_DW), BPF_REG_10, 0,
                    -128 + static_cast<__s16>(offsetof(struct Stats, rxUdpPackets)), 0),
        BPF_INS_BLK(MEM_SET_BY_VAL(BPF_DW), BPF_REG_10, 0,
                    -128 + static_cast<__s16>(offsetof(struct Stats, txTcpPackets)), 0),
        BPF_INS_BLK(MEM_SET_BY_VAL(BPF_DW), BPF_REG_10, 0,
                    -128 + static_cast<__s16>(offsetof(struct Stats, txTcpBytes)), 0), //130
        BPF_INS_BLK(MEM_SET_BY_VAL(BPF_DW), BPF_REG_10, 0,
                    -128 + static_cast<__s16>(offsetof(struct Stats, txUdpPackets)), 0),
        BPF_INS_BLK(MEM_SET_BY_VAL(BPF_DW), BPF_REG_10, 0,
                    -128 + static_cast<__s16>(offsetof(struct Stats, txUdpBytes)), 0),
        BPF_INS_BLK(MEM_SET_BY_VAL(BPF_DW), BPF_REG_10, 0,
                    -128 + static_cast<__s16>(offsetof(struct Stats, rxOtherPackets)), 0),
        BPF_INS_BLK(MEM_SET_BY_VAL(BPF_DW), BPF_REG_10, 0,
                    -128 + static_cast<__s16>(offsetof(struct Stats, rxOtherBytes)), 0),
        BPF_INS_BLK(MEM_SET_BY_VAL(BPF_DW), BPF_REG_10, 0,
                    -128 + static_cast<__s16>(offsetof(struct Stats, txOtherBytes)), 0),
        BPF_INS_BLK(MEM_SET_BY_VAL(BPF_DW), BPF_REG_10, 0,
                    -128 + static_cast<__s16>(offsetof(struct Stats, txOtherPackets)), 0),
        /*
         * add new map entry using BPF_FUNC_map_update_elem, it takes
         * 4 parameters (R1: map_fd, R2: &socket_cookie, R3: &stats,
         * R4: flags)
         */
        BPF_INS_BLK(REG_MOV64, BPF_REG_1, BPF_REG_7, 0, 0),
        BPF_INS_BLK(REG_MOV64, BPF_REG_2, BPF_REG_9, 0, 0),
        BPF_INS_BLK(REG_MOV64, BPF_REG_3, BPF_REG_10, 0, 0),
        BPF_INS_BLK(VAL_ALU64(BPF_ADD), BPF_REG_3, 0, 0, -128), //140
        BPF_INS_BLK(VAL_MOV64, BPF_REG_4, 0, 0, 0),
        BPF_INS_BLK(BPF_JMP | BPF_CALL, 0, 0, 0, BPF_FUNC_map_update_elem),
        BPF_INS_BLK(REG_MOV64, BPF_REG_1, BPF_REG_7, 0, 0),
        BPF_INS_BLK(REG_MOV64, BPF_REG_2, BPF_REG_9, 0, 0),
        BPF_INS_BLK(BPF_JMP | BPF_CALL, 0, 0, 0, BPF_FUNC_map_lookup_elem),
        BPF_INS_BLK(VAL_ALU_JMP(BPF_JNE), BPF_REG_0, 0, 2, 0),
        BPF_INS_BLK(VAL_MOV64, BPF_REG_0, 0, 0, 1), BPF_INS_BLK(PROG_EXIT, 0, 0, 0, 0),
        /*
         * pc24-30 update the packet info to a exist data entry, it can
         * be done by directly write to pointers instead of using
         * BPF_FUNC_map_update_elem helper function
         */
        BPF_INS_BLK(REG_MOV64, BPF_REG_9, BPF_REG_0, 0, 0),
        BPF_INS_BLK(VAL_MOV64, BPF_REG_7, 0, 0, 1),
        BPF_INS_BLK(MEM_LD(BPF_W), BPF_REG_8, BPF_REG_6,
                    static_cast<__s16>(offsetof(struct __sk_buff, len)), 0), //150
        BPF_INS_BLK(MEM_LD(BPF_W), BPF_REG_1, BPF_REG_6,
                    static_cast<__s16>(offsetof(struct __sk_buff, protocol)), 0),
        BPF_INS_BLK(VAL_ALU_JMP(BPF_JNE), BPF_REG_1, 0, 7, htons(ETH_P_IP)),
        BPF_INS_BLK(REG_MOV64, BPF_REG_1, BPF_REG_6, 0, 0),
        BPF_INS_BLK(VAL_MOV64, BPF_REG_2, 0, 0, IPV4_TRANSPORT_PROTOCOL_OFFSET),
        BPF_INS_BLK(REG_MOV64, BPF_REG_3, BPF_REG_10, 0, 0),
        BPF_INS_BLK(VAL_ALU64(BPF_ADD), BPF_REG_3, 0, 0, -133),
        BPF_INS_BLK(VAL_MOV64, BPF_REG_4, 0, 0, 1),
        BPF_INS_BLK(BPF_JMP | BPF_CALL, 0, 0, 0, BPF_FUNC_skb_load_bytes),
        BPF_INS_BLK(VAL_ALU_JMP(BPF_JA), 0, 0, 7, 0),
        BPF_INS_BLK(VAL_ALU_JMP(BPF_JNE), BPF_REG_1, 0, 15, htons(ETH_P_IPV6)), //160
        BPF_INS_BLK(REG_MOV64, BPF_REG_1, BPF_REG_6, 0, 0),
        BPF_INS_BLK(VAL_MOV64, BPF_REG_2, 0, 0, IPV6_TRANSPORT_PROTOCOL_OFFSET),
        BPF_INS_BLK(REG_MOV64, BPF_REG_3, BPF_REG_10, 0, 0),
        BPF_INS_BLK(VAL_ALU64(BPF_ADD), BPF_REG_3, 0, 0, -133),
        BPF_INS_BLK(VAL_MOV64, BPF_REG_4, 0, 0, 1),
        BPF_INS_BLK(BPF_JMP | BPF_CALL, 0, 0, 0, BPF_FUNC_skb_load_bytes),
        BPF_INS_BLK(MEM_LD(BPF_B), BPF_REG_0, BPF_REG_10, -133, 0),
        BPF_INS_BLK(VAL_ALU_JMP(BPF_JNE), BPF_REG_0, 0, 3, IPPROTO_TCP),
        BPF_INS_BLK(REG_ATOMIC_ADD(BPF_DW), BPF_REG_9, BPF_REG_7,
                    static_cast<__s16>(offsetof(struct Stats, rxTcpPackets)), 0),
        BPF_INS_BLK(REG_ATOMIC_ADD(BPF_DW), BPF_REG_9, BPF_REG_8,
                    static_cast<__s16>(offsetof(struct Stats, rxTcpBytes)), 0), //170
        BPF_INS_BLK(VAL_ALU_JMP(BPF_JA), 0, 0, 6, 0),
        BPF_INS_BLK(VAL_ALU_JMP(BPF_JNE), BPF_REG_0, 0, 3, IPPROTO_UDP),
        BPF_INS_BLK(REG_ATOMIC_ADD(BPF_DW), BPF_REG_9, BPF_REG_7,
                    static_cast<__s16>(offsetof(struct Stats, rxUdpPackets)), 0),
        BPF_INS_BLK(REG_ATOMIC_ADD(BPF_DW), BPF_REG_9, BPF_REG_8,
                    static_cast<__s16>(offsetof(struct Stats, rxUdpBytes)), 0),
        BPF_INS_BLK(VAL_ALU_JMP(BPF_JA), 0, 0, 2, 0),
        BPF_INS_BLK(REG_ATOMIC_ADD(BPF_DW), BPF_REG_9, BPF_REG_7,
                    static_cast<__s16>(offsetof(struct Stats, rxOtherPackets)), 0),
        BPF_INS_BLK(REG_ATOMIC_ADD(BPF_DW), BPF_REG_9, BPF_REG_8,
                    static_cast<__s16>(offsetof(struct Stats, rxOtherBytes)), 0),
        BPF_INS_BLK(VAL_MOV64, BPF_REG_0, 0, 0, 1),
        BPF_INS_BLK(PROG_EXIT, 0, 0, 0, 0), //179
    };
    size_t insns_cnt = sizeof(prog) / sizeof(struct bpf_insn);
    return bpfProgLoad(BPF_PROG_TYPE_CGROUP_SKB, prog, insns_cnt, "Apache", 0,
                         bpf_log_buf, sizeof(bpf_log_buf));
}

int setUpBPFMap(int key_size, int value_size, int map_size, const char* path) {
    int ret;
    int map_fd = -1;
    ret = access(path, R_OK);
    /* Check the pinned location first to check if the map is already there.
     * otherwise create a new one.
     */
    if (ret == 0) {
        map_fd = mapRetrieve(path, O_CLOEXEC | O_RDWR);
        if (map_fd < 0)
            printf("pinned map not accessable or not exist: %s(%s)\n", strerror(errno), path);
    } else if (ret < 0 && errno == ENOENT) {
        map_fd = createMap(BPF_MAP_TYPE_HASH, key_size, value_size,
                                map_size, 0);
        if (map_fd < 0) {
            ret = -errno;
            printf("map create failed!: %s(%s)\n", strerror(errno), path);
            return ret;
        }
        ret = mapPin(map_fd, path);
        if (ret) {
            ret = -errno;
            printf("bpf map pin(%d, %s): %s\n", map_fd, path, strerror(errno));
            return ret;
        }
    } else {
        ret = -errno;
        printf("pinned map not accessable: %s(%s)\n", strerror(errno), path);
        return ret;
    }
    return map_fd;
}

int main()
{
    struct rlimit rl = {RLIM_INFINITY, RLIM_INFINITY};
    setrlimit(RLIMIT_MEMLOCK, &rl);
    printf("START to load TrafficController\n");
    cookieTagMap = setUpBPFMap(sizeof(uint64_t), sizeof(struct UidTag), 100,
                                cookie_uid_map_path);
    uidCounterSetMap = setUpBPFMap(sizeof(uint32_t), sizeof(uint32_t), 100,
                            uid_counterSet_map_path);
    uidStatsMap = setUpBPFMap(sizeof(struct StatsKey), sizeof(struct Stats),
                               100, uid_stats_map_path);
    tagStatsMap = setUpBPFMap(sizeof(struct StatsKey), sizeof(struct Stats),
                               100, tag_stats_map_path);

    int mOutProgFd;
    mOutProgFd = egressProgramLoad();
    if (mOutProgFd < 0) {
        printf("load egress program failed: %s\n%s\n", strerror(errno),
              bpf_log_buf);
    } else {
        printf("load success!: \n%s\n", bpf_log_buf);
    }
}
