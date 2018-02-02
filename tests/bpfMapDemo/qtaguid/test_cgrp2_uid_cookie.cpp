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
#include "android-base/strings.h"
#include "android-base/stringprintf.h"

#include <string>
#include <iostream>

#define ptr_to_u64(x) ((uint64_t)(uintptr_t)x)
#define LOG_BUF_SIZE 65536

int egressMap;
int ingressMap;

char bpf_log_buf[LOG_BUF_SIZE];

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


#define MAP_FD_A(x) x[0], x[1], x[2], x[3]
#define MAP_FD_B(x) x[4], x[5], x[6], x[7]
#define INGRESS_MAP 0x12345678ffffffff
#define EGRESS_MAP 0x87654321ffffffff

std::string replaceMapLdCmd(std::string original, std::string pattern, std::string cmd) {
    size_t i = original.find(pattern, 0);
    while(i != std::string::npos) {
        original.replace(i, pattern.length(), cmd);
        i += pattern.length();
        i = original.find(pattern, i);
    }
    return original;
}

void parseByteFromNumber(uint64_t src, char* byteArray) {
    for (int i = 0; i < 8; i++) {
        byteArray[i] = src >> i*8 & 0xFF;
    }
}

int ingressProgramLoad() {
    std::string code =
      "bf 16 00 00 00 00 00 00 " // r6 = r1
      "bf a3 00 00 00 00 00 00 " // r3 = r10
      "07 03 00 00 fc ff ff ff " // r3 += -4
      "b7 02 00 00 17 00 00 00 " // r2 = 23
      "b7 04 00 00 04 00 00 00 " // r4 = 4
      "85 00 00 00 1a 00 00 00 " // call 26
      "61 61 04 00 00 00 00 00 " // r1 = *(u32 *)(r6 + 4)
      "15 01 07 00 04 00 00 00 " // if r1 == 4 goto +7 <LBB0_3>
      "bf a2 00 00 00 00 00 00 " // r2 = r10
      "07 02 00 00 fc ff ff ff " // r2 += -4
      "18 01 00 00 ff ff ff ff 00 00 00 00 78 56 34 12 " // r1 = 1311768469162688511 ll
      "85 00 00 00 01 00 00 00 " // call 1
      "15 00 09 00 00 00 00 00 " // if r0 == 0 goto +9 <LBB0_5>
      "05 00 06 00 00 00 00 00 " // goto +6 <LBB0_4>
      "bf a2 00 00 00 00 00 00 " // r2 = r10
      "07 02 00 00 fc ff ff ff " // r2 += -4
      "18 01 00 00 ff ff ff ff 00 00 00 00 21 43 65 87 " // r1 = -8690466092633554945 ll
      "85 00 00 00 01 00 00 00 " // call 1
      "15 00 02 00 00 00 00 00 " // if r0 == 0 goto +2 <LBB0_5>
      "61 61 00 00 00 00 00 00 " // r1 = *(u32 *)(r6 + 0)
      "db 10 00 00 00 00 00 00 " // lock *(u64 *)(r0 + 0) += r1
      "b7 00 00 00 00 00 00 00 " // r0 = 0
      "95 00 00 00 00 00 00 00"; // exit

    char ingressMapCode[8];
    char egressMapCode[8];
    char ingressMapFd[8];
    char egressMapFd[8];
    parseByteFromNumber(INGRESS_MAP, ingressMapCode);
    parseByteFromNumber(EGRESS_MAP, egressMapCode);
    parseByteFromNumber(ingressMap, ingressMapFd);
    parseByteFromNumber(egressMap, egressMapFd);
    std::string ingressMapFdPattern = android::base::StringPrintf("18 01 00 00 %02x %02x %02x %02x "
                                                                  "00 00 00 00 %02x %02x %02x %02x ",
                                                                  MAP_FD_A(ingressMapCode),
                                                                  MAP_FD_B(ingressMapCode));
    std::string ingressMapFdLoadByte = android::base::StringPrintf("18 11 00 00 %02x %02x %02x %02x "
                                                                  "00 00 00 00 %02x %02x %02x %02x ",
                                                                  MAP_FD_A(ingressMapFd),
                                                                  MAP_FD_B(ingressMapFd));
    std::string egressMapFdPattern = android::base::StringPrintf("18 01 00 00 %02x %02x %02x %02x "
                                                                  "00 00 00 00 %02x %02x %02x %02x ",
                                                                  MAP_FD_A(egressMapCode),
                                                                  MAP_FD_B(egressMapCode));
    std::string egressMapFdLoadByte = android::base::StringPrintf("18 11 00 00 %02x %02x %02x %02x "
                                                                  "00 00 00 00 %02x %02x %02x %02x ",
                                                                  MAP_FD_A(egressMapFd),
                                                                  MAP_FD_B(egressMapFd));
    std::cout << ingressMapFdPattern << std::endl;
    std::cout << ingressMapFdLoadByte << std::endl;
    std::cout << egressMapFdPattern << std::endl;
    std::cout << egressMapFdLoadByte << std::endl;
    std::string codeWithIngressMap = replaceMapLdCmd(code, ingressMapFdPattern, ingressMapFdLoadByte);
    printf("replace one map:\n%s\n", codeWithIngressMap.c_str());
    std::string codeResult = replaceMapLdCmd(codeWithIngressMap, egressMapFdPattern, egressMapFdLoadByte);
    printf("final result:\n%s\n", codeResult.c_str());
    std::vector<std::string> codeArray = android::base::Split(codeResult, " ");
    char *result = (char *)malloc(codeArray.size()*sizeof(char));
    char *currentPtr = result;
    for (std::vector<std::string>::iterator byteCode = codeArray.begin(); byteCode != codeArray.end(); ++byteCode) {
        printf("%s ", byteCode->c_str());
        *currentPtr = strtoul(byteCode->c_str(), 0, 16);
        currentPtr++;
    }
    printf("\nfirst line of byte: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n", MAP_FD_A(result), MAP_FD_B(result));
    void *prog = result;
    printf("byte size: %zu, array size: %zu, program size: %zu\n", sizeof(char), codeArray.size(), codeArray.size()*sizeof(char));
    size_t insns_cnt = codeArray.size() * sizeof(char) / sizeof(struct bpf_insn);
    return bpfProgLoad(BPF_PROG_TYPE_CGROUP_SKB, (bpf_insn *)prog, insns_cnt, "Apache", 0,
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
    ingressMap = createMap(BPF_MAP_TYPE_HASH, sizeof(uint32_t), sizeof(uint64_t), 10, 0);
    printf("ingress fd: %d\n", ingressMap);
    egressMap = createMap(BPF_MAP_TYPE_HASH, sizeof(uint32_t), sizeof(uint64_t), 10, 0);
    printf("egress fd: %d\n", egressMap);

    int mOutProgFd;
    mOutProgFd = ingressProgramLoad();
    if (mOutProgFd < 0) {
        printf("load egress program failed: %s\n%s\n", strerror(errno),
              bpf_log_buf);
    } else {
        printf("load success!: \n%s\n", bpf_log_buf);
    }
}
