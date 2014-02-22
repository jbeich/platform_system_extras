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

#ifndef DNS_RESPONDER_H
#define DNS_RESPONDER_H

#include <pthread.h>

// A trivial DNS responder which records the query name and responds with the
// desired single IPv4 address and TTL = 1 day.
struct Responder {
    Responder(const char* address, const char* response)
        : address_(address), response_(response), query_(), sock_(-1),
          thread_() {
        start();
    }
    ~Responder();

    const char* address() const { return address_; }
    const char* response() const { return response_; }
    const char* query() {
        join();
        return query_;
    }

private:
    void start();
    void join();
    bool parseDomainName(const char* buffer, size_t length);
    int makeErrorResponse(char* buffer, int querysize);
    int makeAddressResponse(char* buffer, int querysize);
    // Returns false when thread should quit.
    bool run();

    static void* runThread(void* data);

    // IP address to listen on, e.g., "127.0.0.3".
    const char* address_;
    // IP address to respond with, e.g., "1.2.3.4".
    const char* response_;

    // Recorded DNS query name.
    char query_[512];

    int sock_;
    pthread_t thread_;
};

#endif  // DNS_RESPONDER_H