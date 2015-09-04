// Copyright (C) 2015 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <linux/types.h>
#include <netlink/socket.h>
#include <netlink/genl/ctrl.h>
#include <netlink/genl/genl.h>

#include <linux/taskstats.h>

#include <algorithm>
#include <memory>

#include "log.h"
#include "taskstats.h"

TaskstatsSocket::TaskstatsSocket() :
    nl_(nullptr, nl_socket_free), family_id_(0) {
}

bool TaskstatsSocket::Open() {
  std::unique_ptr<struct nl_sock, decltype(&nl_socket_free)> nl(
      nl_socket_alloc(), nl_socket_free);
  if (!nl.get()) {
    ERROR("Failed to allocate netlink socket");
    return false;
  }

  int ret = genl_connect(nl.get());
  if (ret < 0) {
    nl_perror(ret, "Unable to open netlink socket (are you root?)");
    return false;
  }

  int family_id = genl_ctrl_resolve(nl.get(), TASKSTATS_GENL_NAME);
  if (family_id < 0) {
    nl_perror(family_id, "Unable to determine taskstats family id (does your kernel support taskstats?)");
    return false;
  }

  nl_ = std::move(nl);
  family_id_ = family_id;

  return true;
}

void TaskstatsSocket::Close() {
}

struct TaskStatsRequest {
  pid_t requested_pid;
  struct taskstats taskstats;
};

static pid_t ParseAggregateTaskStats(struct nlattr* attr, int attr_size,
    struct taskstats *taskstats) {
  pid_t received_pid = -1;
  nla_for_each_attr(attr, attr, attr_size, attr_size)
  {
    switch (nla_type(attr)) {
    case TASKSTATS_TYPE_PID:
    case TASKSTATS_TYPE_TGID:
      received_pid = nla_get_u32(attr);
      break;
    case TASKSTATS_TYPE_STATS:
    {
      int len = static_cast<int>(sizeof(*taskstats));
      len = std::min(len, nla_len(attr));
      nla_memcpy(taskstats, attr, len);
      return received_pid;
    }
    default:
      ERROR("unexpected attribute inside AGGR");
      return -1;
    }
  }

  return -1;
}

static int ParseTaskStats(struct nl_msg* msg, void* arg) {
  struct TaskStatsRequest* taskstats_request =
      static_cast<struct TaskStatsRequest*>(arg);
  struct genlmsghdr* gnlh =
      static_cast<struct genlmsghdr*>(nlmsg_data(nlmsg_hdr(msg)));
  struct nlattr* attr = genlmsg_attrdata(gnlh, 0);

  switch (nla_type(attr)) {
  case TASKSTATS_TYPE_AGGR_PID:
  case TASKSTATS_TYPE_AGGR_TGID:
    struct nlattr* nested_attr = static_cast<struct nlattr*>(nla_data(attr));
    struct taskstats taskstats;
    pid_t ret;

    ret = ParseAggregateTaskStats(nested_attr, nla_len(attr), &taskstats);
    if (ret < 0) {
      ERROR("Bad AGGR_PID contents");
    } else if (ret == taskstats_request->requested_pid) {
      taskstats_request->taskstats = taskstats;
    } else {
      WARN("got taskstats for unexpected pid %d (expected %d), continuing...",
          ret, taskstats_request->requested_pid);
    }
  }
  return NL_OK;
}

bool TaskstatsSocket::GetStats(int pid, int type, TaskStatistics& stats) {
  struct TaskStatsRequest taskstats_request = TaskStatsRequest();
  taskstats_request.requested_pid = pid;

  std::unique_ptr<struct nl_msg, decltype(&nlmsg_free)> message(
      nlmsg_alloc(), nlmsg_free);

  genlmsg_put(message.get(), NL_AUTO_PID, NL_AUTO_SEQ, family_id_, 0, 0,
      TASKSTATS_CMD_GET, TASKSTATS_VERSION);
  nla_put_u32(message.get(), type, pid);

  int result = nl_send_auto_complete(nl_.get(), message.get());
  if (result < 0) {
    nl_perror(result, "Failed to send netlink taskstats pid command");
    return false;
  }

  std::unique_ptr<struct nl_cb, decltype(&nl_cb_put)> callbacks(
      nl_cb_alloc(NL_CB_DEFAULT), nl_cb_put);
  nl_cb_set(callbacks.get(), NL_CB_VALID, NL_CB_CUSTOM, &ParseTaskStats,
      static_cast<void*>(&taskstats_request));

  result = nl_recvmsgs(nl_.get(), callbacks.get());
  if (result < 0) {
    nl_perror(result, "Failed to receive netlink taskstats pid command");
    return false;
  }
  nl_wait_for_ack(nl_.get());

  stats = TaskStatistics(taskstats_request.taskstats);

  return true;
}

bool TaskstatsSocket::GetPidStats(int pid, TaskStatistics& stats) {
  return GetStats(pid, TASKSTATS_CMD_ATTR_PID, stats);
}

bool TaskstatsSocket::GetTgidStats(int tgid, TaskStatistics& stats) {
  bool ret = GetStats(tgid, TASKSTATS_CMD_ATTR_TGID, stats);
  if (ret) {
    stats.pid = tgid;
  }
  return ret;
}

TaskStatistics::TaskStatistics(struct taskstats& taskstats_stats) {
  comm = std::string(taskstats_stats.ac_comm);
  pid = taskstats_stats.ac_pid;

  uid = taskstats_stats.ac_uid;
  gid = taskstats_stats.ac_gid;
  pid = taskstats_stats.ac_pid;
  ppid = taskstats_stats.ac_ppid;

  cpu_delay_count = taskstats_stats.cpu_count;
  cpu_delay_ns = taskstats_stats.cpu_delay_total;

  block_io_delay_count = taskstats_stats.blkio_count;
  block_io_delay_ns = taskstats_stats.blkio_delay_total;

  swap_in_delay_count = taskstats_stats.swapin_count;
  swap_in_delay_ns = taskstats_stats.swapin_delay_total;

  reclaim_delay_count = taskstats_stats.freepages_count;
  reclaim_delay_ns = taskstats_stats.freepages_delay_total;

  cpu_time_real = taskstats_stats.cpu_run_real_total;
  cpu_time_virtual = taskstats_stats.cpu_run_virtual_total;

  read_bytes = taskstats_stats.read_bytes;
  write_bytes = taskstats_stats.write_bytes;
  cancelled_write_bytes = taskstats_stats.cancelled_write_bytes;
  threads = 1;
}

void TaskStatistics::AddPidToTgid(const TaskStatistics& pid_statistics) {
  // tgid statistics already contain delay values totalled across all pids
  // only add IO statistics
  read_bytes            += pid_statistics.read_bytes;
  write_bytes           += pid_statistics.write_bytes;
  cancelled_write_bytes += pid_statistics.cancelled_write_bytes;
  if (pid == pid_statistics.pid) {
    comm = pid_statistics.comm;
    uid = pid_statistics.uid;
    gid = pid_statistics.pid;
    ppid = pid_statistics.ppid;
  } else {
    threads++;
  }
}

TaskStatistics TaskStatistics::Delta(const TaskStatistics& old_statistics) const {
  TaskStatistics ret = *this;
  ret.cpu_delay_count       -= old_statistics.cpu_delay_count;
  ret.cpu_delay_ns          -= old_statistics.cpu_delay_ns;
  ret.block_io_delay_count  -= old_statistics.block_io_delay_count;
  ret.block_io_delay_ns     -= old_statistics.block_io_delay_ns;
  ret.swap_in_delay_count   -= old_statistics.swap_in_delay_count;
  ret.swap_in_delay_ns      -= old_statistics.swap_in_delay_ns;
  ret.reclaim_delay_count   -= old_statistics.reclaim_delay_count;
  ret.reclaim_delay_ns      -= old_statistics.reclaim_delay_ns;
  ret.cpu_time_real         -= old_statistics.cpu_time_real;
  ret.cpu_time_virtual      -= old_statistics.cpu_time_virtual;
  ret.read_bytes            -= old_statistics.read_bytes;
  ret.write_bytes           -= old_statistics.write_bytes;
  ret.cancelled_write_bytes -= old_statistics.cancelled_write_bytes;
  return ret;
}

