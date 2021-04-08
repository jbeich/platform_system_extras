//
// Copyright (C) 2021 The Android Open Source Project
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

#include <memory>
#include <string>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <gflags/gflags.h>

#include <websocket/websocket_handler.h>
#include <websocket/websocket_server.h>

DEFINE_int32(port, 8443, "The port for the http server.");
DEFINE_bool(secure, true, "Whether to use HTTPS or HTTP.");
DEFINE_bool(lwsl, false, "Show libwebsockets debug logs.");
DEFINE_string(assets_dir, "", "Directory with location of webpage assets.");
DEFINE_string(certs_dir, "", "Directory to certificates.");

namespace {

constexpr auto kIncrementUriPath = "/increment";

class IncrementHandler : public android::ws::WebSocketHandler {
  public:
    IncrementHandler(struct lws* wsi) : WebSocketHandler(wsi) {}

    void OnReceive(const uint8_t* msg, size_t len, bool /* binary */) override {
        std::string_view msg_sv(reinterpret_cast<const char*>(msg), len);
        LOG(INFO) << "OnReceive " << msg_sv;
        EnqueueMessage(msg_sv.data(), msg_sv.size());
    }
    void OnConnected() override { LOG(INFO) << "OnConnected"; }
    void OnClosed() override { LOG(INFO) << "OnClosed"; }
};

class IncrementHandlerFactory : public android::ws::WebSocketHandlerFactory {
  public:
    IncrementHandlerFactory() {}
    std::shared_ptr<android::ws::WebSocketHandler> Build(struct lws* wsi) override {
        return std::shared_ptr<android::ws::WebSocketHandler>(new IncrementHandler(wsi));
    }
};

}  // namespace

int main(int argc, char** argv) {
    FLAGS_assets_dir = android::base::GetExecutableDirectory() + "/root";
    FLAGS_certs_dir = android::base::GetExecutableDirectory() + "/certs";
    ::gflags::ParseCommandLineFlags(&argc, &argv, true);
    android::base::InitLogging(argv, android::base::StdioLogger);

    if (FLAGS_lwsl) {
        int logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE | LLL_INFO | LLL_PARSER | LLL_HEADER |
                   LLL_EXT | LLL_CLIENT | LLL_LATENCY | LLL_DEBUG;
        lws_set_log_level(logs, nullptr /* log to stderr */);
    }

    android::ws::WebSocketServer wss("test-protocol", FLAGS_certs_dir, FLAGS_assets_dir, FLAGS_port,
                                     FLAGS_secure);

    auto increment_handler_factory_p =
            std::unique_ptr<android::ws::WebSocketHandlerFactory>(new IncrementHandlerFactory());
    wss.RegisterHandlerFactory(kIncrementUriPath, std::move(increment_handler_factory_p));

    wss.Serve();
    return 0;
}
