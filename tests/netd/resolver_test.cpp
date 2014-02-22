/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless requied by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>

#include <cutils/sockets.h>
#include <private/android_filesystem_config.h>

#include <gtest/gtest.h>
#define LOG_TAG "resolverTest"
#include <utils/Log.h>
#include <testUtil.h>

#include "dns_responder.h"


class ResponseCode {
public:
    // Keep in sync with
    // frameworks/base/services/java/com/android/server/NetworkManagementService.java
    static const int CommandOkay               = 200;
    static const int DnsProxyQueryResult       = 222;

    static const int DnsProxyOperationFailed   = 401;

    static const int CommandSyntaxError        = 500;
    static const int CommandParameterError     = 501;
};


// Returns ResponseCode.
int netdCommand(const char* sockname, const char* command) {
    int sock = socket_local_client(sockname,
                                   ANDROID_SOCKET_NAMESPACE_RESERVED,
                                   SOCK_STREAM);
    if (sock < 0) {
        perror("Error connecting");
        return -1;
    }

    // FrameworkListener expects the whole command in one read.
    char buffer[256];
    int written = snprintf(buffer, sizeof(buffer), "0 %s", command);
    if (write(sock, buffer, written + 1) < 0) {
        perror("Error sending netd command");
        close(sock);
        return -1;
    }

    int nread = read(sock, buffer, sizeof(buffer));
    if (nread < 0) {
        perror("Error reading response");
        close(sock);
        return -1;
    }
    close(sock);
    return atoi(buffer);
}


class ResolverTest : public ::testing::Test {
protected:
    virtual void SetUp() {
        // Ensure resolutions go via proxy.
        setenv("ANDROID_DNS_MODE", "", 1);
        uid = getuid();
        pid = getpid();
        ClearResolver();
    }

    virtual void TearDown() {
        netdCommand("netd", "resolver clearifacemapping");
    }

    void ClearResolver() const {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "resolver clearifaceforpid %d", pid);
        EXPECT_EQ((int)ResponseCode::CommandOkay, netdCommand("netd", cmd)) <<
            cmd;
        snprintf(cmd, sizeof(cmd), "resolver clearifaceforuidrange %d %d",
                 uid, uid + 1);
        EXPECT_EQ((int)ResponseCode::CommandOkay, netdCommand("netd", cmd)) <<
            cmd;
    }

    bool SetResolverForPid(const char* address) const {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "resolver setifaceforpid fake100 %d",
                 pid);
        int code = netdCommand("netd", cmd);
        EXPECT_EQ((int)ResponseCode::CommandOkay, code) << cmd;
        if (!(200 <= code && code < 300))
            return false;
        snprintf(cmd, sizeof(cmd), "resolver setifdns fake100 \"empty.com\" %s",
                 address);
        code = netdCommand("netd", cmd);
        EXPECT_EQ((int)ResponseCode::CommandOkay, code) << cmd;
        if (!(200 <= code && code < 300))
            return false;
        snprintf(cmd, sizeof(cmd), "resolver flushif fake100");
        code = netdCommand("netd", cmd);
        EXPECT_EQ((int)ResponseCode::CommandOkay, code) << cmd;
        if (!(200 <= code && code < 300))
            return false;
        return true;
    }

    int pid;
    int uid;
};


TEST_F(ResolverTest, GetHostByName) {
    Responder resp("127.0.0.3", "1.2.3.3");
    ASSERT_TRUE(SetResolverForPid(resp.address()));
    hostent* he = gethostbyname("hello");
    EXPECT_STREQ("hello.empty.com", resp.query());
    ASSERT_EQ(4, he->h_length);
    ASSERT_FALSE(he->h_addr_list[0] == NULL);
    in_addr addr;
    memcpy(reinterpret_cast<char*>(&addr), he->h_addr_list[0], sizeof(addr));
    EXPECT_STREQ("1.2.3.3", inet_ntoa(addr));
    EXPECT_TRUE(he->h_addr_list[1] == NULL);
    // endhostent(); -- undefined reference!
}

TEST_F(ResolverTest, GetAddrInfo) {
    Responder resp("127.0.0.4", "1.2.3.4");
    ASSERT_TRUE(SetResolverForPid(resp.address()));
    addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    addrinfo* result = NULL;
    getaddrinfo("howdie", NULL, &hints, &result);
    EXPECT_STREQ("howdie.empty.com", resp.query());
    ASSERT_FALSE(result == NULL);
    sockaddr_in* addr = reinterpret_cast<sockaddr_in*>(result->ai_addr);
    EXPECT_STREQ("1.2.3.4", inet_ntoa(addr->sin_addr));
    if (result)
        freeaddrinfo(result);
}

