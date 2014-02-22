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

#include "dns_responder.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace {

#pragma pack(push)
#pragma pack(1)

struct Header {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
};

struct RRecord {
    uint16_t qtype;
    uint16_t qclass;
    uint32_t ttl;
    uint16_t rdatasize;
};

#pragma pack(pop)

}


Responder::~Responder() {
    join();
}


void Responder::start() {
    // Bind the socket before spawning a thread so that we are ready early.
    sock_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock_ < 0) {
        perror("Failed to open UDP socket");
        exit(-1);
    }
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(53);
    addr.sin_addr.s_addr = inet_addr(address_);
    if (bind(sock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        perror("Failed to bind UDP socket");
        exit(-1);
    }
    int rc = pthread_create(&thread_, NULL, Responder::runThread,
                            (void*)this);
    if (rc != 0) {
        perror("Failed to start Responder thread");
        exit(-1);
    }
}


void Responder::join() {
    if (thread_) {
        // Interrupt blocked I/O calls.
        if (sock_ > 0)
            shutdown(sock_, SHUT_RDWR);
        pthread_join(thread_, NULL);
    }
    if (sock_ > 0)
        close(sock_);
    sock_ = -1;
}


bool Responder::parseDomainName(const char* buffer, size_t length) {
    char* out = query_;
    const char* p = buffer;
    const char* end = buffer + length;
    for (;;) {
        if ((*p & 0xc0) != 0) {
            fprintf(stderr, "Unexpected non-direct label in DNS query!");
            break;
        }
        int labellen = *p;
        ++p;
        if (labellen == 0) {
            *out = '\0';
            return true;
        }
        if (p + labellen > end) {
            fprintf(stderr, "DNS label longer than packet!");
            break;
        }
        if (out != query_) {
            *out = '.';
            ++out;
        }
        memcpy(out, p, labellen);
        p += labellen;
        out += labellen;
    }
    return false;
}


int Responder::makeErrorResponse(char* buffer, int querysize) {
    Header* header = reinterpret_cast<Header*>(buffer);
    header->flags |= htons(0x8002);  // Response bit, SERVFAIL.
    return querysize;
}


int Responder::makeAddressResponse(char* buffer, int querysize) {
    Header* header = reinterpret_cast<Header*>(buffer);
    header->flags |= htons(0x8000);  // Response bit.
    header->ancount = htons(1);      // One answer.
    uint16_t qname_ptr = htons(sizeof(Header) | 0xc000);
    RRecord record;
    record.qtype = htons(1);         // type A
    record.qclass = htons(1);        // class IN
    record.ttl = htonl(86400);       // 1 day
    in_addr_t rdata = inet_addr(response_);
    record.rdatasize = htons(sizeof(rdata));

    char* out = buffer + querysize;
    memcpy(out, reinterpret_cast<const char*>(&qname_ptr), sizeof(qname_ptr));
    out += sizeof(qname_ptr);
    memcpy(out, reinterpret_cast<const char*>(&record), sizeof(record));
    out += sizeof(record);
    memcpy(out, reinterpret_cast<const char*>(&rdata), sizeof(rdata));
    out += sizeof(rdata);
    return out - buffer;
}


bool Responder::run() {
    char buffer[512];
    sockaddr_in recvaddr;
    socklen_t addrlen = sizeof(recvaddr);
    int nread = recvfrom(sock_, buffer, sizeof(buffer), 0,
                         reinterpret_cast<sockaddr*>(&recvaddr), &addrlen);
    if (nread < 0) {
        perror("Failed to read from UDP socket");
        return false;
    } else if (nread == 0) {
        // Socket closed, exit thread.
        return false;
    }

    int resplen = 0;
    if (memcmp(buffer + nread - 4, "\0\1\0\1", 4)) {
        // Unhandled qtype or qclass. Only A/IN expected.
        resplen = makeErrorResponse(buffer, nread);
    } else {
        if (!parseDomainName(buffer + sizeof(Header), nread - sizeof(Header))) {
            fprintf(stderr, "Failed to parse domain name");
            return true;
        }
        resplen = makeAddressResponse(buffer, nread);
    }

    if (sendto(sock_, buffer, resplen, 0,
               reinterpret_cast<sockaddr*>(&recvaddr), addrlen) < resplen) {
        perror("Failed to send response");
    }
    return true;
}


// static
void* Responder::runThread(void* data) {
    Responder* responder = reinterpret_cast<Responder*>(data);
    while (responder->run());
    return NULL;
}
