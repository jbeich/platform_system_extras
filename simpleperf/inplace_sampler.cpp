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

#include "inplace_sampler.h"

#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <string>

#include <android-base/logging.h>

#include "environment.h"
#include "inplace_sampler_protocol.h"
#include "IOEventLoop.h"
#include "UnixSocket.h"
#include "utils.h"

static constexpr uint64_t EVENT_ID_FOR_INPLACE_SAMPLER = ULLONG_MAX;

std::unique_ptr<InplaceSampler> InplaceSampler::Create(const perf_event_attr& attr,
                                                       const std::set<pid_t>& processes,
                                                       const std::set<pid_t>& threads) {
  // 1. Check if targets belong to the same process.
  pid_t pid;
  std::vector<pid_t> tids;
  const char* multi_process_msg = "InplaceSampler can't monitor multiple processes";
  CHECK(!(processes.empty() && threads.empty()));
  if (threads.empty()) {
    if (processes.size() != 1) {
      LOG(ERROR) << multi_process_msg;
      return nullptr;
    }
    pid = *processes.begin();
    tids.push_back(-1);
  } else if (processes.empty()) {
    // All threads should belong to the same process.
    pid = -1;
    for (const auto& tid : threads) {
      pid_t cur_pid;
      if (!GetProcessIdForThread(tid, &cur_pid)) {
        return nullptr;
      }
      if (pid == -1) {
        pid = cur_pid;
      } else if (pid != cur_pid) {
        LOG(ERROR) << multi_process_msg;
        return nullptr;
      }
      tids.push_back(tid);
    }
  } else {
    LOG(ERROR) << multi_process_msg;
    return nullptr;
  }

  // 2. Create InplaceSampler instance and connect to server.
  std::unique_ptr<InplaceSampler> sampler(new InplaceSampler(attr, pid, tids));
  if (!sampler->ConnectServer()) {
    return nullptr;
  }
  if (!sampler->StartProfiling()) {
    return nullptr;
  }
  return sampler;
}

InplaceSampler::InplaceSampler(const perf_event_attr& attr, pid_t pid,
                               const std::vector<pid_t>& tids)
  : attr_(attr), pid_(pid), tids_(tids) {
  if (attr_.freq) {
    freq_ = attr_.sample_freq;
  } else {
    freq_ = 1000000000 / attr_.sample_period;
  }
}

uint64_t InplaceSampler::Id() const {
  return EVENT_ID_FOR_INPLACE_SAMPLER;
}

bool InplaceSampler::ConnectServer() {
  std::string server_path = "/tmp/" + INPLACE_SERVER_NAME + std::to_string(pid_);
  conn_ = UnixSocketConnection::Connect(server_path);
  if (conn_ != nullptr) {
    return true;
  }
  server_path = "/data/local/tmp/" + INPLACE_SERVER_NAME + std::to_string(pid_);
  conn_ = UnixSocketConnection::Connect(server_path);
  if (conn_ != nullptr) {
    return true;
  }
  const char* home = getenv("HOME");
  if (home != nullptr && strlen(home) > 0) {
    server_path = std::string(home) + "/" + INPLACE_SERVER_NAME + std::to_string(pid_);
    conn_ = UnixSocketConnection::Connect(server_path);
    if (conn_ != nullptr) {
      return true;
    }
  }
  server_path = "./" + INPLACE_SERVER_NAME + std::to_string(pid_);
  conn_ = UnixSocketConnection::Connect(server_path);
  if (conn_ != nullptr) {
    return true;
  }
  LOG(ERROR) << "Can't find inplace sampler server for process " << pid_;
  return false;
}

bool InplaceSampler::StartProfiling() {
  IOEventLoop loop;
  bool reply_received = false;
  auto check_reply_callback = [&](const UnixSocketMessage& msg) {
    if (msg.type == START_PROFILING_REPLY) {
      reply_received = true;
    }
    return loop.ExitLoop();
  };
  auto close_loop_callback = [&]() {
    return loop.ExitLoop();
  };
  if (!conn_->SetReceiveMessageCallback(check_reply_callback)) {
    return false;
  }

  if (!conn_->SetCloseConnectionCallback(close_loop_callback)) {
    return false;
  }
  if (!conn_->BindToIOEventLoop(loop)) {
    return false;
  }
  if (!SendStartProfilingMessage()) {
    return false;
  }
  timeval timeout;
  timeout.tv_sec = 1;
  timeout.tv_usec = 0;
  if (!loop.AddPeriodicEvent(timeout, close_loop_callback)) {
    return false;
  }
  if (!loop.RunLoop()) {
    return false;
  }
  if (!reply_received) {
    LOG(ERROR) << "can't receive START_PROFILING_REPLY from process " << pid_;
    return false;
  }
  return true;
}

bool InplaceSampler::SendStartProfilingMessage() {
  std::vector<int32_t> tids;
  if (tids_.empty()) {
    tids.push_back(-1);
  } else {
    tids.insert(tids.end(), tids_.begin(), tids_.end());
  }
  size_t msg_size = sizeof(UnixSocketMessage);
  msg_size += sizeof(uint32_t) * (3 + tids.size());
  std::vector<char> msg(msg_size);
  UnixSocketMessage* pmsg = reinterpret_cast<UnixSocketMessage*>(msg.data());
  pmsg->len = msg_size;
  pmsg->type = START_PROFILING;
  char* p = msg.data() + sizeof(UnixSocketMessage);
  MoveToBinaryFormat(SIGUSR2, p);
  MoveToBinaryFormat(freq_, p);
  uint32_t tid_nr = tids.size();
  MoveToBinaryFormat(tid_nr, p);
  for (const auto& tid : tids) {
    MoveToBinaryFormat(tid, p);
  }
  CHECK_EQ(p, msg.data() + msg_size);
  return conn_->SendUnDelayedMessage(*pmsg);
}

bool InplaceSampler::StartPolling(IOEventLoop& loop, const std::function<bool(Record*)>& callback) {
  record_callback_ = callback;
  CHECK(conn_ != nullptr);
  if (!conn_->SetReceiveMessageCallback([&](const UnixSocketMessage& msg) {
    return ProcessMessage(msg);
  })) {
    return false;
  }
  if (!conn_->SetCloseConnectionCallback([&]() {
    return loop.ExitLoop();
  })) {
    return false;
  }
  if (!conn_->BindToIOEventLoop(loop)) {
    return false;
  }
  return true;
}

static MessageMapData LoadMapData(const UnixSocketMessage& msg) {
  MessageMapData data;
  const char* p = msg.data;
  MoveFromBinaryFormat(data.time, p);
  uint64_t tid_nr;
  MoveFromBinaryFormat(tid_nr, p);
  data.tids.resize(tid_nr);
  for (uint64_t i = 0; i < tid_nr; ++i) {
    MessageMapData::TidComm& tid_info = data.tids[i];
    MoveFromBinaryFormat(tid_info.tid, p);
    size_t len = strlen(p);
    tid_info.comm.resize(len);
    strcpy(&tid_info.comm[0], p);
    p += Align(len + 1, 64);
  }
  uint64_t map_nr;
  MoveFromBinaryFormat(map_nr, p);
  data.maps.resize(map_nr);
  for (uint64_t i = 0; i < map_nr; ++i) {
    MessageMapData::Map& map_info = data.maps[i];
    MoveFromBinaryFormat(map_info.start, p);
    MoveFromBinaryFormat(map_info.len, p);
    MoveFromBinaryFormat(map_info.offset, p);
    size_t len = strlen(p);
    map_info.dso.resize(len);
    strcpy(&map_info.dso[0], p);
    p += Align(len + 1, 64);
  }
  CHECK_EQ(p, reinterpret_cast<const char*>(&msg) + msg.len);
  return data;
}

static MessageSampleData LoadSampleData(const UnixSocketMessage& msg) {
  MessageSampleData data;
  const char* p = msg.data;
  MoveFromBinaryFormat(data.tid, p);
  MoveFromBinaryFormat(data.time, p);
  MoveFromBinaryFormat(data.period, p);
  uint64_t ip_nr;
  MoveFromBinaryFormat(ip_nr, p);
  data.ip.resize(ip_nr);
  MoveFromBinaryFormat(data.ip.data(), ip_nr, p);
  return data;
}

bool InplaceSampler::ProcessMessage(const UnixSocketMessage& msg) {
  if (msg.type == MAP_DATA) {
    MessageMapData data = LoadMapData(msg);
    for (const auto& tid_info : data.tids) {
      CommRecord r(attr_, pid_, tid_info.tid, tid_info.comm, Id());
      if (!record_callback_(&r)) {
        return false;
      }
      for (const auto& map_info : data.maps) {
        MmapRecord r(attr_, false, pid_, tid_info.tid,
                     map_info.start, map_info.len, map_info.offset,
                     map_info.dso, Id(), data.time);
        if (!record_callback_(&r)) {
          return false;
        }
      }
    }
  } else if (msg.type == SAMPLE_DATA) {
    MessageSampleData data = LoadSampleData(msg);
    SampleRecord r(attr_, Id(), data.ip[0], pid_, data.tid, data.time, UINT32_MAX,
                   data.period, data.ip);
    if (!record_callback_(&r)) {
      return false;
    }
  }
  return true;
}
