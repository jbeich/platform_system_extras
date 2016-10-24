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

#ifndef SIMPLE_PERF_UNIX_SOCKET_H_
#define SIMPLE_PERF_UNIX_SOCKET_H_

#include <unistd.h>

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "IOEventLoop.h"

// Class wrappers for unix socket communication operations.

class UnixSocketConnection;

// UnixSocketMessage is the message structure used for communication.
struct UnixSocketMessage {
  uint32_t len;
  uint32_t type;
  char data[0];
};

// UnixSocketMessageBuffer is a circular buffer used to store UnixSocketMessages.
class UnixSocketMessageBuffer {
 public:
  explicit UnixSocketMessageBuffer(size_t capacity)
      : data_(capacity), write_head_(0), read_head_(0), valid_bytes_(0) {
  }

  bool Empty() const {
    return valid_bytes_ == 0;
  }

  bool HalfFull() const {
    return valid_bytes_ * 2 >= data_.size();
  }

  bool StoreMessage(const UnixSocketMessage& message) {
    if (data_.size() - valid_bytes_ < message.len) {
      return false;
    }
    if (message.len <= data_.size() - write_head_) {
      memcpy(data_.data() + write_head_, &message, message.len);
    } else {
      uint32_t len1 = data_.size() - write_head_;
      memcpy(data_.data() + write_head_, &message, len1);
      memcpy(data_.data(), reinterpret_cast<const char*>(&message) + len1, message.len - len1);
    }
    write_head_ = (write_head_ + message.len) % data_.size();
    valid_bytes_ += message.len;
    return true;
  }

  bool LoadMessage(std::vector<char>& buffer) {
    if (Empty()) {
      return false;
    }
    uint32_t len;
    ReadBuffer(reinterpret_cast<char*>(&len), 4);
    buffer.resize(len);
    memcpy(&buffer[0], &len, 4);
    ReadBuffer(&buffer[4], len - 4);
    return true;
  }

 private:
  void ReadBuffer(char* p, uint32_t size) {
    if (size <= data_.size() - read_head_) {
      memcpy(p, data_.data() + read_head_, size);
    } else {
      uint32_t len1 = data_.size() - read_head_;
      memcpy(p, data_.data() + read_head_, len1);
      memcpy(p + len1, data_.data(), size - len1);
    }
    read_head_ = (read_head_ + size) % data_.size();
    valid_bytes_ -= size;
  }

  std::vector<char> data_;
  uint32_t write_head_;
  uint32_t read_head_;
  uint32_t valid_bytes_;
};

// UnixSocketServer creates a unix socket server listening on a unix file path.
class UnixSocketServer {
 public:
  static std::unique_ptr<UnixSocketServer> Create(const std::string& server_path, bool is_abstract);

  ~UnixSocketServer();
  const std::string& GetPath() const {
    return path_;
  }
  std::unique_ptr<UnixSocketConnection> AcceptConnection();

 private:
  UnixSocketServer(int server_fd, const std::string& path) : server_fd_(server_fd), path_(path) {}
  const int server_fd_;
  const std::string path_;
};

// UnixSocketConnection is used to communicate between server and client.
// It is either created by accepting a connection in UnixSocketServer, or by
// connecting to a UnixSocketServer.
// UnixSocketConnection binds to a IOEventLoop, so it writes messages to fd
// when it is writable, and read messages from fd when it is readable. To send
// messages, UnixSocketConnection uses a buffer to store to-be-sent messages.
// And whenever it receives a complete message from fd, it calls the callback
// function.
class UnixSocketConnection {
 private:
  static constexpr size_t SEND_BUFFER_SIZE = 512 * 1024;

 public:
  explicit UnixSocketConnection(int fd)
      : fd_(fd),
        write_message_head_(0), read_message_head_(0),
        send_buffer_(SEND_BUFFER_SIZE),
        write_event_enabled_(false), write_event_(nullptr),
        no_more_message_(false) {
  }

  static std::unique_ptr<UnixSocketConnection> Connect(const std::string& server_path, bool is_abstract);

  ~UnixSocketConnection() {
    if (fd_ != -1) {
      close(fd_);
    }
  }

  bool PrepareForIO(IOEventLoop& loop,
                    const std::function<bool (const UnixSocketMessage&)>& receive_message_callback,
                    const std::function<bool ()>& close_connection_callback);

  // Thread-safe function, can be called from signal handler.
  bool SendMessage(const UnixSocketMessage& message) {
    std::lock_guard<std::mutex> lock(send_buffer_and_write_event_mtx_);
    if (no_more_message_ || !send_buffer_.StoreMessage(message)) {
      return false;
    }
    // By buffering messages, we can effectively decrease context-switch times.
    if (!write_event_enabled_ && send_buffer_.HalfFull()) {
      if (IOEventLoop::EnableEvent(write_event_)) {
        write_event_enabled_ = true;
      } else {
        return false;
      }
    }
    return true;
  }

  bool SendUnDelayedMessage(const UnixSocketMessage& message) {
    std::lock_guard<std::mutex> lock(send_buffer_and_write_event_mtx_);
    if (no_more_message_ || !send_buffer_.StoreMessage(message)) {
      return false;
    }
    if (!write_event_enabled_) {
      if (IOEventLoop::EnableEvent(write_event_)) {
        write_event_enabled_ = true;
      } else {
        return false;
      }
    }
    return true;
  }

  // Close connection after sending messages in send buffer.
  bool CloseConnection() {
    std::lock_guard<std::mutex> lock(send_buffer_and_write_event_mtx_);
    no_more_message_ = true;
    if (!write_event_enabled_) {
      if (IOEventLoop::EnableEvent(write_event_)) {
        write_event_enabled_ = true;
      } else {
        return false;
      }
    }
    return true;
  }

 private:
  bool WriteMessage();
  bool ReadMessage();

  int fd_;
  std::function<bool (const UnixSocketMessage&)> read_callback_;
  std::function<bool ()> close_callback_;
  std::vector<char> write_message_;
  size_t write_message_head_;
  std::vector<char> read_message_;
  size_t read_message_head_;

  // send_buffer_and_write_event_mtx_ protects following members, which
  // can be accessed in multiple threads.
  std::mutex send_buffer_and_write_event_mtx_;
  UnixSocketMessageBuffer send_buffer_;
  bool write_event_enabled_;
  IOEventRef write_event_;
  bool no_more_message_;
};

#endif  // SIMPLE_PERF_UNIX_SOCKET_H_
