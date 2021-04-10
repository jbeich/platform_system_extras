//
// Copyright (C) 2020 The Android Open Source Project
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

#include "websocket/websocket_handler.h"

#include <android-base/logging.h>
#include <libwebsockets.h>

namespace android::ws {

const size_t WebSocketHandler::WsBuffer::kLwsPre = LWS_PRE;

WebSocketHandler::WebSocketHandler(struct lws* wsi) : wsi_(wsi) {}

void WebSocketHandler::EnqueueMessage(const uint8_t* data, size_t len, bool binary) {
    std::vector<uint8_t> buffer(LWS_PRE + len, 0);
    std::copy(data, data + len, buffer.begin() + LWS_PRE);
    buffer_queue_.emplace_front(std::move(buffer), binary);
    lws_callback_on_writable(wsi_);
}

// Attempts to write what's left on a websocket buffer to the websocket,
// updating the buffer.
// Returns true if the entire buffer was successfully written.
bool WebSocketHandler::WriteWsBuffer(WebSocketHandler::WsBuffer& ws_buffer) {
    auto len = ws_buffer.data.size() - ws_buffer.start;
    auto flags =
            lws_write_ws_flags(ws_buffer.binary ? LWS_WRITE_BINARY : LWS_WRITE_TEXT, true, true);
    auto res = lws_write(wsi_, &ws_buffer.data[ws_buffer.start], len,
                         static_cast<enum lws_write_protocol>(flags));
    if (res < 0) {
        // This shouldn't happen since this function is called in response to a
        // LWS_CALLBACK_SERVER_WRITEABLE call.
        LOG(FATAL) << "Failed to write data on the websocket";
        // Close
        return true;
    }
    ws_buffer.start += res;
    return ws_buffer.start == ws_buffer.data.size();
}

bool WebSocketHandler::OnWritable() {
    if (buffer_queue_.empty()) {
        return close_;
    }
    auto wrote_full_buffer = WriteWsBuffer(buffer_queue_.back());
    if (wrote_full_buffer) {
        buffer_queue_.pop_back();
    }
    if (!buffer_queue_.empty()) {
        lws_callback_on_writable(wsi_);
    }
    // Only close if there are no more queued writes
    return buffer_queue_.empty() && close_;
}

void WebSocketHandler::Close() {
    close_ = true;
    lws_callback_on_writable(wsi_);
}

bool WebSocketHandler::Empty() const {
    return buffer_queue_.empty();
}

}  // namespace android::ws
