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

#include <dirent.h>
#include <errno.h>
#include <inttypes.h>
#include <libunwind.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>
#include <unwind.h>

#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/strings.h>

#include "environment.h"
#include "inplace_sampler_protocol.h"
#include "UnixSocket.h"
#include "utils.h"

//namespace {

constexpr size_t MAX_CALL_STACK_LENGTH = 1024;
constexpr uint32_t DUMP_MAP_INTERVAL_IN_SEC = 3;

static std::string& g_server_path = *new std::string;

struct PerThreadData {
  // linked in lists of PerThreadData
  PerThreadData* next;
  // If true, the data is used by signal handler and can't be freed.
  bool used_by_signal_handler;
  int tid;
  timer_t timerid;
  uint32_t sample_period_in_ns;
  UnixSocketConnection* conn;
  uint64_t last_sample_time_in_ns;
  struct SampleDataMessage {
    UnixSocketMessage msg_header;
    uint64_t tid;
    uint64_t time_in_ns;
    uint64_t period;
    uint64_t ip_nr;
    uint64_t ips[MAX_CALL_STACK_LENGTH];
  } msg;
};

static pid_t GetTid() {
#if defined(__ANDROID__)
  return gettid();
#else
  return syscall(SYS_gettid);
#endif
}

// Signal handlers can't allocate or free memory, so we allocate and free
// PerThreadData for each monitored thread in sampler thread. And the access
// to PerThreadData is managed in class SignalHandlerHelper.
class SignalHandlerHelper {
 public:
  static void Init(UnixSocketConnection* conn);
  static PerThreadData* AllocateDataForThread(pid_t tid, int signo, uint32_t sample_period_in_ns);
  static void DeleteDataForThread(pid_t tid);
  static void Destroy();
  static void ReleaseLock();

  // Used in signal handler
  static PerThreadData* GetDataForCurrentThread() {
    pid_t tid = GetTid();
    std::lock_guard<decltype(mutex_)> lock(mutex_);
    for (PerThreadData* p = data_list_; p != nullptr; p = p->next) {
      if (p->tid == tid) {
        p->used_by_signal_handler = true;
        return p;
      }
    }
    return nullptr;
  }

  // Used in signal handler
  static void ReleaseData(PerThreadData* data) {
    std::lock_guard<decltype(mutex_)> lock(mutex_);
    data->used_by_signal_handler = false;
  }

  // Used in signal handler
  static void StartTimer(PerThreadData* data) {
    itimerspec ts;
    ts.it_value.tv_sec = 0;
    ts.it_value.tv_nsec = data->sample_period_in_ns;
    ts.it_interval.tv_sec = 0;
    ts.it_interval.tv_nsec = 0;
    timer_settime(data->timerid, 0, &ts, nullptr);
  }

 private:
  static bool CreateTimer(PerThreadData* data, int signo);
  static void DestroyTimer(PerThreadData* data);

  // mutex_ protects access to all other class members.
  static std::mutex& mutex_;
  static UnixSocketConnection* conn_;
  static PerThreadData* data_list_;
  static PerThreadData* free_list_;
};

// Avoid calling destructor of mutex_, because signal handler may try to
// lock the mutex even after __cxa_thread_finalize() is called.
std::mutex& SignalHandlerHelper::mutex_ = *new std::mutex();
UnixSocketConnection* SignalHandlerHelper::conn_;
PerThreadData* SignalHandlerHelper::data_list_;
PerThreadData* SignalHandlerHelper::free_list_;

void SignalHandlerHelper::Init(UnixSocketConnection* conn) {
  std::lock_guard<decltype(mutex_)> lock(mutex_);
  conn_ = conn;
  data_list_ = nullptr;
  free_list_ = nullptr;
}

PerThreadData* SignalHandlerHelper::AllocateDataForThread(pid_t tid, int signo, uint32_t sample_period_in_ns) {
  PerThreadData* data = nullptr;
  {
    std::lock_guard<decltype(mutex_)> lock(mutex_);
    PerThreadData** ap = &free_list_;
    while (*ap != nullptr && (*ap)->used_by_signal_handler) {
      ap = &((*ap)->next);
    }
    if (*ap != nullptr) {
      data = *ap;
      DestroyTimer(data);
      *ap = data->next;
    }
  }
  if (data == nullptr) {
    data = new PerThreadData;
  }
  data->used_by_signal_handler = false;
  data->tid = tid;
  if (!CreateTimer(data, signo)) {
    delete data;
    return nullptr;
  }
  data->sample_period_in_ns = sample_period_in_ns;
  data->conn = conn_;
  data->last_sample_time_in_ns = GetSystemClock();
  data->msg.msg_header.type = SAMPLE_DATA;
  data->msg.tid = tid;
  std::lock_guard<decltype(mutex_)> lock(mutex_);
  data->next = data_list_;
  data_list_ = data;
  return data;
}

void SignalHandlerHelper::DeleteDataForThread(pid_t tid) {
  std::lock_guard<decltype(mutex_)> lock(mutex_);
  PerThreadData** ap = &data_list_;
  while (*ap != nullptr && (*ap)->tid != tid) {
    ap = &((*ap)->next);
  }
  // Delete the data by moving it from data_list_ to free_list_, so
  // it can't be used by further signal handlers.
  if (*ap != nullptr) {
    PerThreadData* data = *ap;
    *ap = data->next;
    data->next = free_list_;
    free_list_ = data;
  }
}

void SignalHandlerHelper::Destroy() {
  std::lock_guard<decltype(mutex_)> lock(mutex_);
  PerThreadData* p = data_list_;
  data_list_ = nullptr;
  PerThreadData* np;
  while (p != nullptr) {
    np = p->next;
    p->next = free_list_;
    free_list_ = p;
    p = np;
  }
  while (free_list_ != nullptr) {
    while (free_list_ != nullptr && !free_list_->used_by_signal_handler) {
      p = free_list_->next;
      DestroyTimer(free_list_);
      delete free_list_;
      free_list_ = p;
    }
    if (free_list_ != nullptr) {
      mutex_.unlock();
      usleep(10);
      mutex_.lock();
    }
  }
  conn_ = nullptr;
  free_list_ = nullptr;
}

bool SignalHandlerHelper::CreateTimer(PerThreadData* data, int signo) {
  sigevent se;
  se.sigev_signo = signo;
  se.sigev_notify = SIGEV_THREAD_ID;
  se._sigev_un._tid = data->tid;
  if (timer_create(CLOCK_MONOTONIC, &se, &data->timerid) != 0) {
    PLOG(ERROR) << "timer_create() failed";
    return false;
  }
  return true;
}

void SignalHandlerHelper::DestroyTimer(PerThreadData* data) {
  timer_delete(data->timerid);
}

static _Unwind_Reason_Code find_frame(_Unwind_Context* context, void* arg) {
  auto data = reinterpret_cast<PerThreadData::SampleDataMessage*>(arg);
  if (data->ip_nr < MAX_CALL_STACK_LENGTH) {
    uintptr_t ip = _Unwind_GetIP(context);
    data->ips[data->ip_nr++] = ip;
  }
  return _URC_NO_REASON;
}

// SignalHandler will not be called on SampleThread.
static void SignalHandler(int) {
  PerThreadData* data = SignalHandlerHelper::GetDataForCurrentThread();
  if (data == nullptr) {
    return;
  }
  PerThreadData::SampleDataMessage& msg = data->msg;
  msg.ip_nr = 0;
  _Unwind_Backtrace(find_frame, &msg);
  if (msg.ip_nr > 0) {
    msg.time_in_ns = GetSystemClock();
    msg.period = msg.time_in_ns - data->last_sample_time_in_ns;
    data->last_sample_time_in_ns = msg.time_in_ns;
    // Send sample data.
    msg.msg_header.len = sizeof(UnixSocketMessage) + sizeof(uint64_t) * (4 + msg.ip_nr);
    // Don't care whether the message is send successfully.
    data->conn->SendMessage(msg.msg_header);
  }
  // Start a timer which fires only one time. If we use a timer fires
  // periodically, we take the risk of blocking the monitored thread.
  // TODO: Start the timer periodically when unwinding time is speed up.
  SignalHandlerHelper::StartTimer(data);
  SignalHandlerHelper::ReleaseData(data);
}

struct ThreadInfo {
  std::string comm;
};

// SampleManager has following responsibilities:
// 1. Handle messages sent by simpleperf.
// 2. Update thread info regularly.
// 3. Send thread info to simpleperf.
// 4. Set up timer to send signals to profiled threads regularly.
class SampleManager {
 public:
  SampleManager(UnixSocketConnection* conn) : conn_(conn), signo_(-1), sample_freq_(0),
      sample_pid_(getpid()), sample_tid_(GetTid()), monitor_all_tids_(false),
      send_map_data_failed_(false) {
  }
  ~SampleManager();

  bool SampleLoop();

 private:
  bool HandleCommand(IOEventLoop& loop, const UnixSocketMessage& msg);
  bool LoadStartProfilingMessage(const UnixSocketMessage& msg);
  bool SendStartProfilingReply();
  bool StartProfiling(IOEventLoop& loop);
  bool InstallSignalHandler();
  bool SearchThreads();
  bool CheckThreadChange(bool* has_new_thread);
  bool CheckMapChange(bool* has_new_map);
  bool SendThreadMapData();

  UnixSocketConnection* const conn_;
  int signo_;
  uint32_t sample_freq_;
  uint32_t sample_period_in_ns_;
  pid_t sample_pid_;
  pid_t sample_tid_;
  bool monitor_all_tids_;
  std::set<int> monitor_tid_filter_;
  // threads_ is the really monitored threads.
  std::map<int, ThreadInfo> threads_;
  std::map<uint64_t, MessageMapData::Map> maps_;
  bool send_map_data_failed_;
};

SampleManager::~SampleManager() {
  SignalHandlerHelper::Destroy();
}

bool SampleManager::SampleLoop() {
  IOEventLoop loop;
  if (!conn_->SetReceiveMessageCallback([&](const UnixSocketMessage& msg) {
    return HandleCommand(loop, msg);
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
  return loop.RunLoop();
}

bool SampleManager::HandleCommand(IOEventLoop& loop, const UnixSocketMessage& msg) {
  if (msg.type == START_PROFILING) {
    if (!LoadStartProfilingMessage(msg)) {
      return false;
    }
    if (!SendStartProfilingReply()) {
      return false;
    }
    if (!StartProfiling(loop)) {
      return false;
    }
    return true;
  }
  LOG(ERROR) << "Unexpected msg type: " << msg.type;
  return false;
}

bool SampleManager::LoadStartProfilingMessage(const UnixSocketMessage& msg) {
  const char* p = msg.data;
  if (msg.len < sizeof(int32_t) * 3) {
    LOG(ERROR) << "StartProfilingMessage format error";
    return false;
  }
  MoveFromBinaryFormat(signo_, p);
  MoveFromBinaryFormat(sample_freq_, p);
  if (sample_freq_ == 0 || sample_freq_ > 1000000000) {
    LOG(ERROR) << "unexpected sample_freq: " << sample_freq_;
    return false;
  }
  if (sample_freq_ == 1) {
    sample_period_in_ns_ = 999999999;
  } else {
    sample_period_in_ns_ = 1000000000 / sample_freq_;
  }
  uint32_t tid_nr;
  MoveFromBinaryFormat(tid_nr, p);
  if (msg.len != sizeof(UnixSocketMessage) + sizeof(int32_t) * (3 + tid_nr)) {
    LOG(ERROR) << "StartProfilingMessage format error";
    return false;
  }
  monitor_tid_filter_.clear();
  const uint32_t* ptid = reinterpret_cast<const uint32_t*>(p);
  monitor_tid_filter_.insert(ptid, ptid + tid_nr);
  monitor_all_tids_ = (monitor_tid_filter_.find(-1) != monitor_tid_filter_.end());
  return true;
}

bool SampleManager::SendStartProfilingReply() {
  UnixSocketMessage msg;
  msg.len = sizeof(UnixSocketMessage);
  msg.type = START_PROFILING_REPLY;
  return conn_->SendUnDelayedMessage(msg);
}

bool SampleManager::StartProfiling(IOEventLoop& loop) {
  if (!InstallSignalHandler()) {
    return false;
  }
  // Cache maps in current process in libunwind.
  unw_map_local_create();
  if (!SearchThreads()) {
    return false;
  }
  timeval tv;
  tv.tv_sec = DUMP_MAP_INTERVAL_IN_SEC;
  tv.tv_usec = 0;
  if (!loop.AddPeriodicEvent(tv, [&]() {
    return SearchThreads();
  })) {
    return false;
  }
  return true;
}

bool SampleManager::InstallSignalHandler() {
  SignalHandlerHelper::Init(conn_);
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = SignalHandler;
  sa.sa_flags |= SA_RESTART;
  sigfillset(&sa.sa_mask);
  if (sigaction(signo_, &sa, nullptr) != 0) {
    PLOG(ERROR) << "sigaction failed";
    return false;
  }
  return true;
}

bool SampleManager::SearchThreads() {
  bool has_new_map;
  if (!CheckMapChange(&has_new_map)) {
    return false;
  }
  bool has_new_thread;
  if (!CheckThreadChange(&has_new_thread)) {
    return false;
  }
  bool send_map_data = has_new_thread || has_new_map || send_map_data_failed_;
  if (send_map_data) {
    send_map_data_failed_ = !SendThreadMapData();
  }
  return true;
}

bool SampleManager::CheckThreadChange(bool* has_new_thread) {
  *has_new_thread = false;
  std::vector<ThreadComm> thread_comms;
  if (!GetThreadComm(sample_pid_, &thread_comms)) {
    return false;
  }
  std::map<pid_t, std::string> current_threads;
  for (const auto& thread : thread_comms) {
    if (thread.tid != sample_tid_ && (monitor_all_tids_ || monitor_tid_filter_.find(thread.tid) != monitor_tid_filter_.end())) {
      current_threads[thread.tid] = thread.comm;
    }
  }
  // Delete per thread data for dead threads.
  std::vector<pid_t> dead_threads;
  for (const auto& pair : threads_) {
    if (current_threads.find(pair.first) == current_threads.end()) {
      dead_threads.push_back(pair.first);
    }
  }
  for (auto dead_tid : dead_threads) {
    // Delete timer
    SignalHandlerHelper::DeleteDataForThread(dead_tid);
    threads_.erase(dead_tid);
  }
  // Create per thread data for new threads.
  for (const auto& pair : current_threads) {
    auto it = threads_.find(pair.first);
    if (it == threads_.end()) {
      PerThreadData* data = SignalHandlerHelper::AllocateDataForThread(pair.first, signo_, sample_period_in_ns_);
      if (data == nullptr) {
        continue;
      }
      SignalHandlerHelper::StartTimer(data);
      ThreadInfo& info = threads_[pair.first];
      info.comm = pair.second;
      *has_new_thread = true;
    } else if (it->second.comm != pair.second) {
      it->second.comm = pair.second;
      // We want to send thread map data when the name of a thread changes.
      *has_new_thread = true;
    }
  }
  return true;
}

bool SampleManager::CheckMapChange(bool* has_new_map) {
  *has_new_map = false;
  // 1. Get current map info.
  std::vector<ThreadMmap> thread_mmaps;
  if (!GetThreadMmapsInProcess(getpid(), &thread_mmaps)) {
    return false;
  }
  // 2. Check if map info has been changed.
  for (const auto& map : thread_mmaps) {
    if (map.executable) {
      auto it = maps_.find(map.start_addr);
      if (it == maps_.end() || it->second.len != map.len || it->second.offset != map.pgoff
          || it->second.dso != map.name) {
        *has_new_map = true;
        break;
      }
    }
  }
  if (!*has_new_map) {
    return true;
  }
  maps_.clear();
  for (const auto& map : thread_mmaps) {
    if (map.executable) {
      auto& m = maps_[map.start_addr];
      m.start = map.start_addr;
      m.len = map.len;
      m.offset = map.pgoff;
      m.dso = map.name;
    }
  }
  return true;
}

bool SampleManager::SendThreadMapData() {
  size_t msg_size = sizeof(UnixSocketMessage);
  msg_size += sizeof(uint64_t) * 2;
  for (const auto& pair : threads_) {
    msg_size += sizeof(int64_t) + Align(pair.second.comm.size() + 1, 64);
  }
  msg_size += sizeof(uint64_t);
  uint64_t map_nr = maps_.size();
  for (const auto& pair : maps_) {
    msg_size += sizeof(uint64_t) * 3 + Align(pair.second.dso.size() + 1, 64);
  }

  std::vector<char> msg(msg_size);
  UnixSocketMessage* pmsg = reinterpret_cast<UnixSocketMessage*>(msg.data());
  pmsg->len = msg_size;
  pmsg->type = MAP_DATA;
  char* p = msg.data() + sizeof(UnixSocketMessage);
  MoveToBinaryFormat(GetSystemClock(), p);
  uint64_t tid_nr = threads_.size();
  MoveToBinaryFormat(tid_nr, p);
  for (const auto& pair : threads_) {
    uint64_t tid = pair.first;
    MoveToBinaryFormat(tid, p);
    strcpy(p, pair.second.comm.c_str());
    p += Align(pair.second.comm.size() + 1, 64);
  }
  MoveToBinaryFormat(map_nr, p);
  for (const auto& pair : maps_) {
    CHECK_LE(static_cast<uint32_t>(p - msg.data() + 24), msg_size);
    MoveToBinaryFormat(pair.second.start, p);
    MoveToBinaryFormat(pair.second.len, p);
    MoveToBinaryFormat(pair.second.offset, p);
    CHECK_LE(p - msg.data() + pair.second.dso.size() + 1, msg_size);
    strcpy(p, pair.second.dso.c_str());
    p += Align(pair.second.dso.size() + 1, 64);
  }
  CHECK_EQ(p, msg.data() + msg_size);
  return conn_->SendMessage(*pmsg);
}

static bool SamplerThread(UnixSocketConnection* conn) {
  SampleManager manager(conn);
  return manager.SampleLoop();
}

static std::unique_ptr<UnixSocketServer> CreateServer() {
  pid_t pid = getpid();
  std::string server_path = "/tmp/" + INPLACE_SERVER_NAME + std::to_string(pid);
  std::unique_ptr<UnixSocketServer> server = UnixSocketServer::Create(server_path);
  if (server != nullptr) {
    return server;
  }
  server_path = "/data/local/tmp/" + INPLACE_SERVER_NAME + std::to_string(pid);
  server = UnixSocketServer::Create(server_path);
  if (server != nullptr) {
    return server;
  }
  std::string cmdline;
  if (!android::base::ReadFileToString("/proc/self/cmdline", &cmdline)) {
    PLOG(ERROR) << "failed to read cmdline";
  } else {
    std::string path = "/data/data/" + android::base::Trim(cmdline.c_str());
    if (IsDir(path)) {
      server_path = path + "/" + INPLACE_SERVER_NAME + std::to_string(pid);
      server = UnixSocketServer::Create(server_path);
      if (server != nullptr) {
        return server;
      }
    }
  }
  const char* home = getenv("HOME");
  if (home != nullptr && strlen(home) > 0) {
    server_path = std::string(home) + "/" + INPLACE_SERVER_NAME + std::to_string(pid);
    server = UnixSocketServer::Create(server_path);
    if (server != nullptr) {
      return server;
    }
  }
  LOG(ERROR) << "Can't create inplace sampler server for process " << pid;
  return nullptr;
}

static void ExitCleanup() {
  // It is not thread safe to access global variables here, but better than none.
  if (!g_server_path.empty()) {
    unlink(g_server_path.c_str());
  }
}

static void* ServerThread(void*) {
  pthread_setname_np(pthread_self(), "sample_server");
  std::unique_ptr<UnixSocketServer> server = CreateServer();
  if (server == nullptr) {
    pthread_setname_np(pthread_self(), "conn_failed");
    while (true);
    return nullptr;
  }
  LOG(INFO) << "Server is created at " << server->GetPath();
  g_server_path = server->GetPath();
  atexit(ExitCleanup);
  while (true) {
    LOG(INFO) << "Sample server is waiting for new connection.";
    std::unique_ptr<UnixSocketConnection> conn = server->AcceptConnection();
    if (conn == nullptr) {
      break;
    }
    LOG(INFO) << "Sample server gets a new connection.";
    if (!SamplerThread(conn.get())) {
    }
  }
  return nullptr;
}

__attribute__((constructor)) void InitInplaceSamplerServer() {
  pthread_t thread;
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  pthread_create(&thread, &attr, ServerThread, nullptr);
}

//}  // anonymous namespace
