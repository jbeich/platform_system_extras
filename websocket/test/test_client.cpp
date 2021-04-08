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
#include <android-base/logging.h>
#include <gflags/gflags.h>
#include <websocket/ws_connection.h>

DEFINE_int32(port, 8443, "The port for the http server.");
DEFINE_bool(secure, true, "Whether to use HTTPS or HTTP.");
DEFINE_bool(lwsl, false, "Show libwebsockets debug logs.");

namespace {

class ObserverImpl : public android::ws::WsConnectionObserver {
  public:
    void OnOpen() {
        LOG(INFO) << "OnOpen";

        // TODO PostTask
        CHECK(server_connection_);
        std::string_view data("{}");
        server_connection_->Send(reinterpret_cast<const uint8_t*>(data.data()), data.size());
    }
    void OnClose() { LOG(INFO) << "OnClose"; }
    void OnError(const std::string& error) { LOG(INFO) << "OnError " << error; }
    void OnReceive(const uint8_t* msg, size_t length, bool /*is_binary*/) {
        std::string_view msg_sv(reinterpret_cast<const char*>(msg), length);
        LOG(INFO) << "OnReceive " << msg_sv;
    }
    void set_server_connection(std::shared_ptr<android::ws::WsConnection> sc) {
        server_connection_ = sc;
    }
    std::shared_ptr<android::ws::WsConnection> server_connection() const {
        return server_connection_;
    }

  private:
    std::shared_ptr<android::ws::WsConnection> server_connection_;
};

}  // namespace

int main(int argc, char** argv) {
    ::gflags::ParseCommandLineFlags(&argc, &argv, true);
    android::base::InitLogging(argv, android::base::StdioLogger);

    if (FLAGS_lwsl) {
        int logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE | LLL_INFO | LLL_PARSER | LLL_HEADER |
                   LLL_EXT | LLL_CLIENT | LLL_LATENCY | LLL_DEBUG;
        lws_set_log_level(logs, nullptr /* log to stderr */);
    }

    auto ws_context = android::ws::WsConnectionContext::Create(false /* do not start */);
    CHECK(ws_context) << "Failed to create websocket context";

    auto observer = std::make_shared<ObserverImpl>();

    observer->set_server_connection(ws_context->CreateConnection(
            FLAGS_port, "localhost", "/increment",
            FLAGS_secure ? android::ws::WsConnection::Security::kAllowSelfSigned
                         : android::ws::WsConnection::Security::kInsecure,
            "test-protocol", observer, {}));
    observer->server_connection()->Connect();

    while (ws_context->ServeOnce())
        ;
}
