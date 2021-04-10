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

#include <map>
#include <memory>
#include <string>
#include <unordered_set>

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

using std::string_literals::operator""s;

namespace {

constexpr auto kEchoUriPath = "/echo";

using ReadCb = std::function<bool(std::string_view)>;

class SyncServer;

enum class ConnectionStatus {
    NONE,
    CONNECTED,
    CLOSED,
};

class EchoHandler : public android::ws::WebSocketHandler,
                    public std::enable_shared_from_this<EchoHandler> {
  public:
    EchoHandler(struct lws* wsi, std::shared_ptr<SyncServer> sync_server)
        : WebSocketHandler(wsi), sync_server_(std::move(sync_server)) {}
    ~EchoHandler() {
//        LOG(FATAL) << "???";
    }

    void OnReceive(const uint8_t* msg, size_t len, bool binary) override;
    void OnConnected() override;
    void OnClosed() override;

    bool GetAndExecuteCommand();

  private:
    std::shared_ptr<SyncServer> sync_server_;
    ConnectionStatus status_ = ConnectionStatus::NONE;
    ReadCb read_cb_;

    bool SyncWaitForConnected();
    bool SyncRead(ReadCb read_cb);
    bool Transact(std::string_view received, std::string* response);
    bool SyncWrite(std::string_view data);
};

class EchoHandlerFactory : public android::ws::WebSocketHandlerFactory {
  public:
    EchoHandlerFactory(std::shared_ptr<SyncServer> sync_server)
        : sync_server_(std::move(sync_server)) {}
    std::shared_ptr<android::ws::WebSocketHandler> Build(struct lws* wsi) override {
        return std::shared_ptr<android::ws::WebSocketHandler>(new EchoHandler(wsi, sync_server_));
    }

  private:
    std::shared_ptr<SyncServer> sync_server_;
};

// Synchronous server implementation.
class SyncServer : public std::enable_shared_from_this<SyncServer> {
  public:
    SyncServer() {}

    void Init() {
        wss_ = std::make_unique<android::ws::WebSocketServer>(
                "test-protocol", FLAGS_certs_dir, FLAGS_assets_dir, FLAGS_port, FLAGS_secure);
        auto increment_handler_factory_p = std::unique_ptr<android::ws::WebSocketHandlerFactory>(
                new EchoHandlerFactory(shared_from_this()));
        wss_->RegisterHandlerFactory(kEchoUriPath, std::move(increment_handler_factory_p));
    }

    // Delegate to wss_->ServeOnce()
    bool ServeOnce() {
        return wss_->ServeOnce();
    }

    // Main entry of the loop
    bool LoopOnce();

    void AddConnection(std::shared_ptr<EchoHandler> handler) {
        LOG(ERROR) << "Adding connection " << handler.get();
        connections_.emplace(std::move(handler));
        LOG(ERROR) << "Num connections: " << connections_.size();
    }

    void DeleteConnection(const std::shared_ptr<EchoHandler>& handler) {
        LOG(ERROR) << "Deleting connection " << handler.get();
        connections_.erase(handler);
        LOG(ERROR) << "Num connections: " << connections_.size();
    }

  private:
    std::unique_ptr<android::ws::WebSocketServer> wss_;
    std::unordered_set<std::shared_ptr<EchoHandler>> connections_;
};

void EchoHandler::OnConnected() {
    LOG(INFO) << "OnConnected " << this;
    status_ = ConnectionStatus::CONNECTED;

    sync_server_->AddConnection(shared_from_this());
}

void EchoHandler::OnReceive(const uint8_t* msg, size_t len, bool /* binary */) {
    std::string_view msg_sv(reinterpret_cast<const char*>(msg), len);
    LOG(INFO) << "OnReceive " << msg_sv;

    if (!read_cb_) {
        LOG(ERROR) << "No read scheduled, dropping data!";
        return;
    }
    if (!read_cb_(msg_sv)) {
        LOG(ERROR) << "read_cb_ failed!";
        return;
    }
    // AckRead
    read_cb_ = nullptr;
}

void EchoHandler::OnClosed() {
    LOG(INFO) << "OnClosed";

    sync_server_->DeleteConnection(shared_from_this());
    status_ = ConnectionStatus::CLOSED;
    read_cb_ = nullptr;
}

bool EchoHandler::GetAndExecuteCommand() {
    if (!SyncWaitForConnected()) return false;
    std::string response;
    ReadCb cb = [&response, this](std::string_view data) { return Transact(data, &response); };
    if (!SyncRead(cb)) return false;
    return SyncWrite(response);
}

bool EchoHandler::SyncWaitForConnected() {
    while (status_ == ConnectionStatus::NONE) {
        // FIXME this does not work because it mangles with other connections as well!
        //   Should be fixed if I manage the accept()ed fd and call lws_service_fd()?
        if (!sync_server_->ServeOnce()) {
            LOG(ERROR) << "Shutting down!";
            return false;
        }
    }
    return status_ == ConnectionStatus::CONNECTED;
}

bool EchoHandler::SyncRead(ReadCb read_cb) {
    if (status_ != ConnectionStatus::CONNECTED) {
        LOG(ERROR) << "client already disconnects";
        return false;
    }
    if (read_cb_) {
        LOG(ERROR) << "Already reading!";
        return false;
    }
    LOG(INFO) << "Setting read_cb: " << this;
    read_cb_ = read_cb;
    while (read_cb_) {
        // FIXME this does not work because it mangles with other connections as well!
        //   Should be fixed if I manage the accept()ed fd?
        if (!sync_server_->ServeOnce()) {
            LOG(ERROR) << "Server shutting down";
            return false;
        }
    }
    if (status_ != ConnectionStatus::CONNECTED) {
        LOG(ERROR) << "client already disconnects";
        return false;
    }
    return true;
}

bool EchoHandler::Transact(std::string_view received, std::string* response) {
    *response = "Response to "s + std::string(received);
    return true;
}

bool EchoHandler::SyncWrite(std::string_view data) {
    EnqueueMessage(data.data(), data.size());
    while (!Empty()) {
        LOG(INFO) << "Write: Serving once";
        // FIXME this does not work because it mangles with other connections as well!
        //   Should be fixed if I manage the accept()ed fd?
        if (!sync_server_->ServeOnce()) {
            LOG(ERROR) << "Server shutting down";
            return false;
        }
    }
    return true;
}

bool SyncServer::LoopOnce() {
    if (connections_.empty()) {
        // bootstrap
        return ServeOnce();
    }
    // GetAndExecuteCommand modifies the list, so take a snapshot.
    auto connections = connections_;
    for (const auto& conn : connections) {
        (void)conn->GetAndExecuteCommand();
    }
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    FLAGS_assets_dir = android::base::GetExecutableDirectory() + "/root";
    FLAGS_certs_dir = android::base::GetExecutableDirectory() + "/certs";
    ::gflags::ParseCommandLineFlags(&argc, &argv, true);
    android::base::InitLogging(argv, android::base::StderrLogger);
    android::base::SetMinimumLogSeverity(android::base::LogSeverity::INFO);

    if (FLAGS_lwsl) {
        int logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE | LLL_INFO | LLL_PARSER | LLL_HEADER |
                   LLL_EXT | LLL_CLIENT | LLL_LATENCY | LLL_DEBUG;
        lws_set_log_level(logs, nullptr /* log to stderr */);
    }

    auto sync_server = std::make_shared<SyncServer>();
    sync_server->Init();

    while (sync_server->LoopOnce())
        ;
    return 0;
}
