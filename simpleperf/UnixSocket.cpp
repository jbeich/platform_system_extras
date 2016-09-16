/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include "UnixSocket.h"

#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <algorithm>

#include <android-base/logging.h>

#include "IOEventLoop.h"

std::unique_ptr<UnixSocketServer> UnixSocketServer::Create(const std::string& server_path) {
  int sockfd = socket(PF_UNIX, SOCK_STREAM, 0);
  if (sockfd < 0) {
    PLOG(ERROR) << "socket() failed";
    return nullptr;
  }
  sockaddr_un serv_addr;
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sun_family = AF_UNIX;
  if (server_path.size() + 1 > sizeof(serv_addr.sun_path)) {
    LOG(ERROR) << "can't create unix domain socket as server path is too long: " << server_path;
    return nullptr;
  }
  strcpy(serv_addr.sun_path, server_path.c_str());
  if (bind(sockfd, reinterpret_cast<sockaddr*>(&serv_addr), sizeof(serv_addr)) < 0) {
    PLOG(ERROR) << "bind() failed for " << server_path;
    return nullptr;
  }
  if (listen(sockfd, 1) < 0) {
    PLOG(ERROR) << "listen() failed";
    return nullptr;
  }
  return std::unique_ptr<UnixSocketServer>(new UnixSocketServer(sockfd, server_path));
}

UnixSocketServer::~UnixSocketServer() {
  close(server_fd_);
}

std::unique_ptr<UnixSocketConnection> UnixSocketServer::AcceptConnection() {
  int sockfd = accept(server_fd_, nullptr, nullptr);
  if (sockfd < 0) {
    PLOG(ERROR) << "accept() failed";
    return nullptr;
  }
  return std::unique_ptr<UnixSocketConnection>(new UnixSocketConnection(sockfd));
}

std::unique_ptr<UnixSocketConnection> UnixSocketConnection::Connect(const std::string& server_path) {
  int sockfd = socket(PF_UNIX, SOCK_STREAM, 0);
  if (sockfd < 0) {
    PLOG(DEBUG) << "socket() failed";
    return nullptr;
  }
  sockaddr_un serv_addr;
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sun_family = AF_UNIX;
  if (server_path.size() + 1 > sizeof(serv_addr.sun_path)) {
    LOG(DEBUG) << "can't create unix domain socket as server path is too long: " << server_path;
    return nullptr;
  }
  strcpy(serv_addr.sun_path, server_path.c_str());
  if (connect(sockfd, reinterpret_cast<sockaddr*>(&serv_addr), sizeof(serv_addr)) < 0) {
    PLOG(DEBUG) << "connect() failed, server_path = " << server_path;
    return nullptr;
  }
  return std::unique_ptr<UnixSocketConnection>(new UnixSocketConnection(sockfd));
}

bool UnixSocketConnection::SetReceiveMessageCallback(const std::function<bool (const UnixSocketMessage&)>& callback) {
  read_callback_ = callback;
  return true;
}

bool UnixSocketConnection::SetCloseConnectionCallback(const std::function<bool ()>& callback) {
  close_callback_ = callback;
  return true;
}

bool UnixSocketConnection::BindToIOEventLoop(IOEventLoop& loop) {
  write_event_ = loop.AddWriteEvent(fd_, [&]() {
    return WriteMessage();
  });
  if (write_event_ == nullptr) {
    return false;
  }
  if (!IOEventLoop::DisableEvent(write_event_)) {
    return false;
  }
  write_event_enabled_ = false;
  return nullptr != loop.AddReadEvent(fd_, [&]() {
    return ReadMessage();
  });
}

bool UnixSocketConnection::WriteMessage() {
  if (write_message_head_ == write_message_.size()) {
    std::lock_guard<std::mutex> lock(send_buffer_mtx_);
    if (!send_buffer_.LoadMessage(write_message_)) {
      if (IOEventLoop::DisableEvent(write_event_)) {
        write_event_enabled_ = false;
        return true;
      }
      return false;
    }
    write_message_head_ = 0;
  }
  ssize_t result = TEMP_FAILURE_RETRY(write(fd_,
      write_message_.data() + write_message_head_,
      write_message_.size() - write_message_head_));
  if (result >= 0) {
    write_message_head_ += result;
  } else if (errno != EAGAIN) {
    PLOG(ERROR) << "write() failed";
    return false;
  }
  return true;
}

bool UnixSocketConnection::ReadMessage() {
  bool connection_closed = false;
  if (read_message_head_ < sizeof(uint32_t)) {
    read_message_.resize(sizeof(uint32_t));
    ssize_t result = TEMP_FAILURE_RETRY(read(fd_,
        read_message_.data() + read_message_head_,
        sizeof(uint32_t) - read_message_head_));
    if (result > 0) {
      read_message_head_ += result;
    } else if (result == 0) {
      connection_closed = true;
    } else if (errno != EAGAIN) {
      PLOG(ERROR) << "read() failed";
      return false;
    }
  }
  if (read_message_head_ >= sizeof(uint32_t)) {
    uint32_t len = *reinterpret_cast<uint32_t*>(read_message_.data());
    read_message_.resize(len);
    ssize_t result = TEMP_FAILURE_RETRY(read(fd_,
        read_message_.data() + read_message_head_,
        len - read_message_head_));
    if (result > 0) {
      read_message_head_ += result;
    } else if (result == 0) {
      connection_closed = true;
    } else if (errno != EAGAIN) {
      PLOG(ERROR) << "read() failed";
      return false;
    }
    if (read_message_head_ == len) {
      if (!read_callback_(*reinterpret_cast<UnixSocketMessage*>(read_message_.data()))) {
        return false;
      }
      read_message_head_ = 0;
    }
  }
  if (connection_closed) {
    return close_callback_();
  }
  return true;
}
