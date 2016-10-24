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

#include <gtest/gtest.h>

#include <string>
#include <thread>

static void ClientToTestUndelayedMessage(const std::string& path, bool &client_success) {
  std::unique_ptr<UnixSocketConnection> client = UnixSocketConnection::Connect(path, true);
  ASSERT_TRUE(client != nullptr);
  IOEventLoop loop;
  // For each message received from the server, the client replies a msg
  // with type + 1.
  auto receive_message_callback = [&](const UnixSocketMessage& msg) {
    if (msg.len != sizeof(UnixSocketMessage)) {
      return false;
    }
    UnixSocketMessage reply_msg;
    reply_msg.len = sizeof(UnixSocketMessage);
    reply_msg.type = msg.type + 1;
    return client->SendUnDelayedMessage(reply_msg);
  };
  auto close_connection_callback = [&]() {
    return loop.ExitLoop();
  };
  ASSERT_TRUE(client->PrepareForIO(loop, receive_message_callback, close_connection_callback));
  ASSERT_TRUE(loop.RunLoop());
  client_success = true;
}

TEST(UnixSocket, undelayed_message) {
  std::string path = "unix_socket_test_" + std::to_string(getpid());
  std::unique_ptr<UnixSocketServer> server = UnixSocketServer::Create(path, true);
  ASSERT_TRUE(server != nullptr);
  bool client_success = false;
  std::thread thread([&]() {
    ClientToTestUndelayedMessage(path, client_success);
  });
  std::unique_ptr<UnixSocketConnection> conn = server->AcceptConnection();
  ASSERT_TRUE(conn != nullptr);
  IOEventLoop loop;
  uint32_t need_reply_type = 1;
  // For each message received from the client, the server replies a msg
  // with type + 1, and exits when type reaches 10.
  auto receive_message_callback = [&](const UnixSocketMessage& msg) {
    if (msg.len != sizeof(UnixSocketMessage) || msg.type != need_reply_type) {
      return false;
    }
    if (need_reply_type >= 10) {
      return conn->CloseConnection();
    }
    UnixSocketMessage new_msg;
    new_msg.len = sizeof(UnixSocketMessage);
    new_msg.type = msg.type + 1;
    need_reply_type = msg.type + 2;
    return conn->SendUnDelayedMessage(new_msg);
  };
  auto close_connection_callback = [&]() {
    return loop.ExitLoop();
  };
  ASSERT_TRUE(conn->PrepareForIO(loop, receive_message_callback, close_connection_callback));
  UnixSocketMessage msg;
  msg.len = sizeof(UnixSocketMessage);
  msg.type = 0;
  ASSERT_TRUE(conn->SendUnDelayedMessage(msg));
  ASSERT_TRUE(loop.RunLoop());
  thread.join();
  ASSERT_TRUE(client_success);
}

static void ClientToTestBufferedMessage(const std::string& path, bool &client_success) {
  std::unique_ptr<UnixSocketConnection> client = UnixSocketConnection::Connect(path, true);
  ASSERT_TRUE(client != nullptr);
  IOEventLoop loop;
  // The client exits once receiving a message from the server.
  auto receive_message_callback = [&](const UnixSocketMessage& msg) {
    if (msg.len != sizeof(UnixSocketMessage) || msg.type != 0) {
      return false;
    }
    return client->CloseConnection();
  };
  auto close_connection_callback = [&]() {
    return loop.ExitLoop();
  };
  ASSERT_TRUE(client->PrepareForIO(loop, receive_message_callback, close_connection_callback));
  // The client sends buffered messages until the send buffer is full.
  UnixSocketMessage msg;
  msg.len = sizeof(UnixSocketMessage);
  msg.type = 0;
  while (true) {
    msg.type++;
    if (!client->SendMessage(msg)) {
      break;
    }
  }
  ASSERT_TRUE(loop.RunLoop());
  client_success = true;
}

TEST(UnixSocket, buffered_message) {
  std::string path = "unix_socket_test_" + std::to_string(getpid());
  std::unique_ptr<UnixSocketServer> server = UnixSocketServer::Create(path, true);
  ASSERT_TRUE(server != nullptr);
  bool client_success = false;
  std::thread thread([&]() {
    ClientToTestBufferedMessage(path, client_success);
  });
  std::unique_ptr<UnixSocketConnection> conn = server->AcceptConnection();
  ASSERT_TRUE(conn != nullptr);
  IOEventLoop loop;
  uint32_t need_reply_type = 1;
  auto receive_message_callback = [&](const UnixSocketMessage& msg) {
    // The server checks if the type of received message is increased by one each time.
    if (msg.len != sizeof(UnixSocketMessage) || msg.type != need_reply_type) {
      return false;
    }
    if (need_reply_type == 1) {
      // Notify the client to exit.
      UnixSocketMessage new_msg;
      new_msg.len = sizeof(UnixSocketMessage);
      new_msg.type = 0;
      if (!conn->SendUnDelayedMessage(new_msg)) {
        return false;
      }
    }
    need_reply_type++;
    return true;
  };
  auto close_connection_callback = [&]() {
    return loop.ExitLoop();
  };
  ASSERT_TRUE(conn->PrepareForIO(loop, receive_message_callback, close_connection_callback));
  ASSERT_TRUE(loop.RunLoop());
  thread.join();
  ASSERT_TRUE(client_success);
}
