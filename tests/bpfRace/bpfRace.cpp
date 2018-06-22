/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef LOG_TAG
#define LOG_TAG "bpfloader"
#endif

#include <arpa/inet.h>
#include <elf.h>
#include <error.h>
#include <fcntl.h>
#include <inttypes.h>
#include <linux/bpf.h>
#include <linux/unistd.h>
#include <linux/membarrier.h>
#include <net/if.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <android-base/stringprintf.h>
#include <android-base/unique_fd.h>
#include <cutils/log.h>
#include <gtest/gtest.h>

#include <netdutils/Misc.h>
#include "bpf/BpfUtils.h"
#include "bpf/BpfMap.h"
#include "bpf_test.h"

using android::base::unique_fd;

#define BPF_PROG_PATH "/system/etc/bpf"
#define BPF_PROG_SRC BPF_PROG_PATH "/bpf_test.o"

using android::bpf::ReplacePattern;
using android::bpf::BpfMap;
using android::bpf::BpfProgInfo;
using android::bpf::createMap;

class BpfRaceTest : public ::testing::Test {
  protected:
    BpfRaceTest() {}
    BpfMap<uint64_t, stats_value> fakeCookieStatsMapA;
    BpfMap<uint64_t, stats_value> fakeCookieStatsMapB;
    BpfMap<uint32_t, uint32_t> fakeConfigurationMap;
    BpfProgInfo program;

    void SetUp() {
        fakeCookieStatsMapA.reset(createMap(BPF_MAP_TYPE_HASH, sizeof(uint64_t),
                                            sizeof(struct stats_value), 16, 0));
        fakeCookieStatsMapB. reset(createMap(BPF_MAP_TYPE_HASH, sizeof(uint64_t),
                                             sizeof(struct stats_value), 16, 0));
        fakeConfigurationMap.reset(createMap(BPF_MAP_TYPE_HASH, sizeof(uint32_t),
                                             sizeof(uint32_t), 1, 0));
        const std::vector<ReplacePattern> mapPatterns = {
            ReplacePattern(COOKIE_STATS_MAP_A, fakeCookieStatsMapA.getMap().get()),
            ReplacePattern(COOKIE_STATS_MAP_B, fakeCookieStatsMapB.getMap().get()),
            ReplacePattern(CONFIGURATION_MAP, fakeConfigurationMap.getMap().get()),
        };
        program = { .attachType = MAX_BPF_ATTACH_TYPE, .name = TEST_PROG_NAME,
                    .loadType = BPF_PROG_TYPE_SOCKET_FILTER};
        ASSERT_EQ(0, android::bpf::parseProgramsFromFile(BPF_PROG_SRC, &program, 1, mapPatterns));
    }

};


TEST_F(BpfRaceTest, testRace) {
    int prog_fd = program.fd.get();
    uint32_t configureKey = 1;
    fakeConfigurationMap.writeValue(configureKey, 0, BPF_ANY);
    pid_t pids[16];
    for (int i = 0; i < 16; i++) {
        if ((pids[i] = fork()) < 0) {
            FAIL() << "fork child process failed";
        } else if (pids[i] == 0) {
            struct sockaddr_in si_other;
            struct sockaddr_in si_me;
            uint64_t j = i;
            int s_rcv, s_send, recv_len;
            char buf[15];
            int res;
            socklen_t slen = sizeof(si_other);

            s_rcv = socket(PF_INET, SOCK_DGRAM, 0);
            ASSERT_LE(0, s_rcv);
            si_other.sin_family = AF_INET;
            si_other.sin_port = htons(8888);
            std::string address = android::base::StringPrintf("127.0.0.%d", i+1);
            ASSERT_NE(0, (inet_aton(address.c_str(), &si_other.sin_addr)));
            ASSERT_NE(-1, bind(s_rcv, (struct sockaddr *)&si_other, sizeof(si_other)));
            s_send = socket(PF_INET, SOCK_DGRAM, 0);
            ASSERT_LE(0, s_send) << "send socket creat failed!\n";
            ASSERT_LE(0, setsockopt(s_rcv, SOL_SOCKET, SO_ATTACH_BPF, &prog_fd, sizeof(prog_fd)))
                << "attach bpf program failed\n";
            while (1) {
                    std::string id = android::base::StringPrintf("%d: %" PRIu64 "\n", i, j);
                    res = sendto(s_send, &id, id.length(), 0,
                                 (struct sockaddr *)&si_other, slen);
                    ASSERT_LE(0, res);
                    recv_len = recvfrom(s_rcv, &buf, sizeof(buf), 0,
                                 (struct sockaddr *)&si_me, &slen);
                    ASSERT_LE(0, recv_len);
                    ++j;
            }
        }
    }

    const auto printStatsInfo = [](const uint64_t key, const stats_value& value,
                                   const BpfMap<uint64_t, stats_value>&) {
        printf("cookie: %" PRIu64 " stats:%" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 "\n", key,
               value.rxBytes, value.rxPackets, value.txBytes, value.txPackets);
        return android::netdutils::status::ok;
    };
    int i = 0;
    while (1) {
        if (i%2 == 0) {
            ASSERT_TRUE(isOk(fakeCookieStatsMapB.isEmpty()));
        } else if (i%2 == 1) {
            ASSERT_TRUE(isOk(fakeCookieStatsMapA.isEmpty()));
        }
        ++i;
        auto oldConfigure = fakeConfigurationMap.readValue(configureKey);
        if (isOk(oldConfigure)) {
            printf("old configure is: %u\n", oldConfigure.value());
        }
        printf("map A stats\n");
        android::netdutils::Status res = fakeCookieStatsMapA.iterateWithValue(printStatsInfo);
        if (!isOk(res)) {
            FAIL() << "print map failed: %s";
        }

        printf("map B stats\n");
        res = fakeCookieStatsMapB.iterateWithValue(printStatsInfo);
        if (!isOk(res)) {
            FAIL() << "print map failed: %s";
        }
        fakeConfigurationMap.writeValue(configureKey, i%2, BPF_ANY);

        // Comment the following syscall out if you want to generate the race
        // problem.

        if (i%2 == 0) {
            fakeCookieStatsMapB.clear();
        } else {
            fakeCookieStatsMapA.clear();
        }
    }
}
