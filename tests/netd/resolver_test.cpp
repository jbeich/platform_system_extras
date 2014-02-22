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
#include <stdarg.h>
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
    int nwritten = snprintf(buffer, sizeof(buffer), "0 %s", command);
    if (write(sock, buffer, nwritten + 1) < 0) {
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


bool expectNetdResult(int code, const char* sockname, const char* format, ...) {
    char command[256];
    va_list args;
    va_start(args, format);
    vsnprintf(command, sizeof(command), format, args);
    va_end(args);
    int result = netdCommand(sockname, command);
    EXPECT_EQ(code, result) << command;
    return (200 <= code && code < 300);
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
        expectNetdResult(ResponseCode::CommandOkay, "netd",
                         "resolver clearifaceforpid %d", pid);
        expectNetdResult(ResponseCode::CommandOkay, "netd",
                         "resolver clearifaceforuidrange %d %d", uid, uid + 1);
    }

    bool SetResolverForPid(const char* address) const {
        return
            expectNetdResult(ResponseCode::CommandOkay, "netd",
                             "resolver setifaceforpid fake100 %d", pid) &&
            expectNetdResult(ResponseCode::CommandOkay, "netd",
                             "resolver setifdns fake100 \"empty.com\" %s",
                             address) &&
            expectNetdResult(ResponseCode::CommandOkay, "netd",
                             "resolver flushif fake100");
    }

    bool FlushCache() const {
        return expectNetdResult(ResponseCode::CommandOkay, "netd",
                                "resolver flushif fake100");
    }

    const char* ToString(const addrinfo* result) const {
        if (!result)
            return "<null>";
        sockaddr_in* addr = reinterpret_cast<sockaddr_in*>(result->ai_addr);
        return inet_ntoa(addr->sin_addr);
    }

    const char* ToString(const hostent* result) const {
        in_addr addr;
        memcpy(reinterpret_cast<char*>(&addr), result->h_addr_list[0],
               sizeof(addr));
        return inet_ntoa(addr);
    }

    int pid;
    int uid;
};


TEST_F(ResolverTest, GetHostByName) {
    Responder resp("127.0.0.3", "1.2.3.3");
    ASSERT_TRUE(SetResolverForPid(resp.address()));
    hostent* result = gethostbyname("hello");
    EXPECT_STREQ("hello.empty.com", resp.query());
    ASSERT_FALSE(result == NULL);
    ASSERT_EQ(4, result->h_length);
    ASSERT_FALSE(result->h_addr_list[0] == NULL);
    EXPECT_STREQ("1.2.3.3", ToString(result));
    EXPECT_TRUE(result->h_addr_list[1] == NULL);
    // endhostent(); -- undefined reference!
}


TEST_F(ResolverTest, GetAddrInfo) {
    addrinfo* result = NULL;

    Responder resp1("127.0.0.4", "1.2.3.4");
    ASSERT_TRUE(SetResolverForPid(resp1.address()));

    EXPECT_EQ(0, getaddrinfo("howdie", NULL, NULL, &result));
    EXPECT_STREQ("howdie.empty.com", resp1.query());
    EXPECT_STREQ("1.2.3.4", ToString(result));
    if (result) freeaddrinfo(result);
    result = NULL;

    // Verify that it's cached.
    EXPECT_EQ(0, getaddrinfo("howdie", NULL, NULL, &result));
    EXPECT_STREQ("1.2.3.4", ToString(result));
    if (result) freeaddrinfo(result);
    result = NULL;

    // Verify that cache can be flushed.
    ASSERT_TRUE(FlushCache());
    Responder resp2("127.0.0.4", "1.2.3.4");

    EXPECT_EQ(0, getaddrinfo("howdie", NULL, NULL, &result));
    EXPECT_STREQ("howdie.empty.com", resp2.query());
    EXPECT_STREQ("1.2.3.4", ToString(result));
    if (result) freeaddrinfo(result);
}


TEST_F(ResolverTest, GetAddrInfoV4) {
    Responder resp("127.0.0.5", "1.2.3.5");
    ASSERT_TRUE(SetResolverForPid(resp.address()));
    addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    addrinfo* result = NULL;
    EXPECT_EQ(0, getaddrinfo("hola", NULL, &hints, &result));
    EXPECT_STREQ("hola.empty.com", resp.query());
    EXPECT_STREQ("1.2.3.5", ToString(result));
    if (result) freeaddrinfo(result);
}


// TODO(szym): test android_getaddrinfofornet
