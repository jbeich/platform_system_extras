/*
 * Copyright (C) 2017 The Android Open Source Project
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
 * See the License for the specic language governing permissions and
 * limitations under the License.
 */

#include <android-base/file.h>
#include <android-base/logging.h>

#include "perfmgr/NodeLooperThread.h"

namespace android {
namespace perfmgr {

bool NodeLooperThread::Request(const std::vector<NodeAction>& actions,
                               const std::string& hint_type) {
    if (::android::Thread::exitPending()) {
        LOG(WARNING) << "NodeLooperThread is exiting";
        return false;
    }
    if (!::android::Thread::isRunning()) {
        LOG(FATAL) << "NodeLooperThread stopped, abort...";
    }

    bool ret = true;
    ::android::AutoMutex _l(lock_);
    for (const auto& a : actions) {
        if (a.node_index >= nodes_.size()) {
            LOG(ERROR) << "Node index out of bound: " << a.node_index
                       << " ,size: " << nodes_.size();
            ret = false;
        } else {
            // End time set to steady time point max
            ReqTime end_time = ReqTime::max();
            // Timeout is non-zero
            if (a.timeout_ms != std::chrono::milliseconds::zero()) {
                auto now = std::chrono::steady_clock::now();
                // Overflow protection in case timeout_ms is too big to overflow
                // time point which is unsigned integer
                if (std::chrono::duration_cast<std::chrono::milliseconds>(
                        ReqTime::max() - now) > a.timeout_ms) {
                    end_time = now + a.timeout_ms;
                }
            }
            ret = nodes_[a.node_index]->AddRequest(a.value_index, hint_type,
                                                   end_time) &&
                  ret;
        }
    }
    wake_cond_.signal();
    return ret;
}

bool NodeLooperThread::Cancel(const std::vector<NodeAction>& actions,
                              const std::string& hint_type) {
    if (::android::Thread::exitPending()) {
        LOG(WARNING) << "NodeLooperThread is exiting";
        return false;
    }
    if (!::android::Thread::isRunning()) {
        LOG(FATAL) << "NodeLooperThread stopped, abort...";
    }

    bool ret = true;
    ::android::AutoMutex _l(lock_);
    for (const auto& a : actions) {
        if (a.node_index >= nodes_.size()) {
            LOG(ERROR) << "Node index out of bound: " << a.node_index
                       << " ,size: " << nodes_.size();
            ret = false;
        } else {
            nodes_[a.node_index]->RemoveRequest(hint_type);
        }
    }
    wake_cond_.signal();
    return ret;
}

bool NodeLooperThread::threadLoop() {
    ::android::AutoMutex _l(lock_);
    std::chrono::milliseconds timeout_ms = kMaxUpdatePeriod;
    for (auto& n : nodes_) {
        auto t = n->Update();
        timeout_ms = std::min(t, timeout_ms);
    }
    // For unsigned types, convert to float to avoid wrap around
    nsecs_t sleep_timeout_ns =
        static_cast<nsecs_t>(timeout_ms.count() * 1000.0f * 1000.0f);
    // VERBOSE level won't print by default in user/userdebug build
    LOG(VERBOSE) << "NodeLooperThread will wait for " << sleep_timeout_ns
                 << "ns";
    wake_cond_.waitRelative(lock_, sleep_timeout_ns);
    return true;
}

void NodeLooperThread::onFirstRef() {
    auto ret = this->run("NodeLooperThread", PRIORITY_HIGHEST);
    if (ret != NO_ERROR) {
        LOG(ERROR) << "NodeLooperThread start fail";
    } else {
        LOG(INFO) << "NodeLooperThread started";
    }
}

void NodeLooperThread::Stop() {
    if (::android::Thread::isRunning()) {
        LOG(INFO) << "NodeLooperThread stopping";
        {
            ::android::AutoMutex _l(lock_);
            wake_cond_.signal();
            ::android::Thread::requestExit();
        }
        ::android::Thread::join();
        LOG(INFO) << "NodeLooperThread stopped";
    }
}

}  // namespace perfmgr
}  // namespace android
