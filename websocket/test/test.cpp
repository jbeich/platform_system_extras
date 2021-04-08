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

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <string_view>

#include <android-base/file.h>
#include <android-base/strings.h>
#include <gflags/gflags.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <websocket/websocket_handler.h>
#include <websocket/websocket_server.h>
#include <websocket/ws_connection.h>

DEFINE_int32(port, 8443, "The port for the http server.");
DEFINE_bool(lwsl, false, "Show libwebsockets debug logs.");

using testing::_;
using testing::Bool;
using testing::Eq;
using testing::InitGoogleMock;
using testing::Invoke;
using testing::MatcherCast;
using testing::NiceMock;
using testing::SafeMatcherCast;
using testing::StrEq;
using testing::TestWithParam;

using std::chrono_literals::operator""s;

namespace android::ws::client {
class MockObserver : public WsConnectionObserver {
  public:
    MockObserver() {
        ON_CALL(*this, OnError(_)).WillByDefault(Invoke([](const auto& error) {
            LOG(ERROR) << "OnError " << error;
        }));
    }
    MOCK_METHOD(void, OnOpen, (), (override));
    MOCK_METHOD(void, OnClose, (), (override));
    MOCK_METHOD(void, OnReceive, (const uint8_t* msg, size_t length, bool is_binary), (override));
    MOCK_METHOD(void, OnError, (const std::string&), (override));
};

}  // namespace android::ws::client

namespace android::ws::server {
class AbstractHandler : public WebSocketHandler {
  public:
    AbstractHandler(struct lws* wsi) : WebSocketHandler(wsi) {}
    void OnReceive(const uint8_t* /*msg*/, size_t /*len*/, bool /* binary */) override {
        LOG(INFO) << "OnReceive";
    }
    void OnConnected() override { LOG(INFO) << "OnConnected"; }
    void OnClosed() override { LOG(INFO) << "OnClosed"; }
};

class EchoHandler : public AbstractHandler {
  public:
    EchoHandler(struct lws* wsi) : AbstractHandler(wsi) {}
    void OnReceive(const uint8_t* msg, size_t len, bool /* binary */) override {
        std::string_view msg_sv(reinterpret_cast<const char*>(msg), len);
        EnqueueMessage(msg_sv.data(), msg_sv.size());
    }
};

class EchoHandlerFactory : public WebSocketHandlerFactory {
  public:
    std::shared_ptr<android::ws::WebSocketHandler> Build(struct lws* wsi) override {
        return std::shared_ptr<android::ws::WebSocketHandler>(new EchoHandler(wsi));
    }
};
}  // namespace android::ws::server

namespace android::ws {

static constexpr const char* kTestProtocol = "test-protocol";
static constexpr const char* kEchoUriPath = "/echo";

enum class ServerStatus {
    NONE,
    RUNNING,
    SCHEDULE_STOP,
    STOPPED,
};

class AtomicOptionalString {
  public:
    operator bool() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return !!data_;
    }
    bool operator==(std::string_view other) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return data_ == other;
    }
    void operator=(std::string_view data) {
        std::lock_guard<std::mutex> lock(mutex_);
        data_ = data;
    }

  private:
    mutable std::mutex mutex_;
    std::optional<std::string> data_;
};

template <typename Data>
class Atomic {
  public:
    Atomic(Data data) : data_(data) {}
    operator Data() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return data_;
    }
    void operator=(Data data) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            data_ = data;
        }
        cv_.notify_all();
    }
    template <class Rep, class Period>
    bool WaitFor(const std::chrono::duration<Rep, Period>& rel_time, Data expected_value) {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv_.wait_for(lock, rel_time, [&]() { return data_ == expected_value; });
    }

  private:
    Data data_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
};

class WebsocketWrapperTest : public TestWithParam<bool> {
  public:
    void SetUp() override { SetUpAndRunServerWithThread(GetParam()); }

    void TearDown() override {
        server_status_ = ServerStatus::SCHEDULE_STOP;
        if (server_thread_.joinable()) server_thread_.join();
        EXPECT_EQ(server_status_, ServerStatus::STOPPED);
    }

    struct Client {
        std::shared_ptr<WsConnectionContext> ws_context;
        std::shared_ptr<WsConnection> server_connection;
    };
    Client SetUpClient(std::shared_ptr<WsConnectionObserver> observer, bool secure) {
        auto ws_context = WsConnectionContext::Create(false /* do not start */);
        CHECK(ws_context) << "Failed to create websocket context";
        auto server_connection =
                ws_context->CreateConnection(FLAGS_port, "localhost", kEchoUriPath,
                                             secure ? WsConnection::Security::kAllowSelfSigned
                                                    : WsConnection::Security::kInsecure,
                                             "test-protocol", observer, {});
        return {ws_context, server_connection};
    }

  protected:
    void SetUpServer(bool secure) {
        CHECK(server_status_ == ServerStatus::NONE);
        auto assets_dir = android::base::GetExecutableDirectory() + "/root";
        auto certs_dir = android::base::GetExecutableDirectory() + "/certs";
        wss_ = std::make_shared<WebSocketServer>(kTestProtocol, certs_dir, assets_dir, FLAGS_port,
                                                 secure);
        auto echo_handler_factory_p =
                std::unique_ptr<WebSocketHandlerFactory>(new server::EchoHandlerFactory());
        wss_->RegisterHandlerFactory(kEchoUriPath, std::move(echo_handler_factory_p));
    }
    void RunServer() {
        CHECK(server_status_ == ServerStatus::NONE);
        server_status_ = ServerStatus::RUNNING;
        while (server_status_ == ServerStatus::RUNNING && wss_->ServeOnce())
            ;
        server_status_ = ServerStatus::STOPPED;
    }

    void SetUpAndRunServerWithThread(bool secure) {
        server_thread_ = std::thread([this, &secure]() {
            SetUpServer(secure);
            RunServer();
        });
    }

    Atomic<ServerStatus> server_status_{ServerStatus::NONE};
    std::shared_ptr<WebSocketServer> wss_;
    std::thread server_thread_;
};

TEST_P(WebsocketWrapperTest, SendDataAndEchoOnce) {
    ASSERT_TRUE(server_status_.WaitFor(5s, ServerStatus::RUNNING));
    const std::string data = "great!";
    AtomicOptionalString received_data;

    auto observer = std::make_shared<NiceMock<client::MockObserver>>();
    auto client = SetUpClient(observer, GetParam());
    EXPECT_CALL(*observer, OnOpen()).WillOnce(Invoke([&]() {
        client.server_connection->Send(reinterpret_cast<const uint8_t*>(data.data()), data.size());
    }));
    ON_CALL(*observer, OnError(_)).WillByDefault(Invoke([&](const std::string& error) {
        ADD_FAILURE() << error;
        received_data = "";
    }));
    EXPECT_CALL(*observer, OnReceive(_, _, _))
            .WillOnce(Invoke([&](const uint8_t* data, size_t len, bool) {
                received_data = std::string_view(reinterpret_cast<const char*>(data), len);
            }));

    client.server_connection->Connect();
    auto time_end = std::chrono::system_clock::now() + 5s;
    while (true) {
        if (std::chrono::system_clock::now() >= time_end) break;
        if (received_data) break;
        if (!client.ws_context->ServeOnce()) break;
    }
    EXPECT_EQ(received_data, data);
}

INSTANTIATE_TEST_CASE_P(WebsocketWrapper, WebsocketWrapperTest, Bool());

}  // namespace android::ws

void LogLws(int level, const char* line) {
    android::base::LogSeverity severity;
    switch (level) {
        case LLL_ERR:
            severity = android::base::LogSeverity::ERROR;
            break;
        case LLL_WARN:
            severity = android::base::LogSeverity::WARNING;
            break;
        case LLL_NOTICE:
            [[fallthrough]];
        case LLL_INFO:
            severity = android::base::LogSeverity::INFO;
            break;
        default:
            severity = android::base::LogSeverity::DEBUG;
            break;
    }
    std::string_view line_sv(line);
    android::base::ConsumeSuffix(&line_sv, "\n");
    LOG(severity) << line_sv;
}

int main(int argc, char** argv) {
    InitGoogleMock(&argc, argv);
    gflags::AllowCommandLineReparsing();
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    if (FLAGS_lwsl) {
        int logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE | LLL_INFO | LLL_PARSER | LLL_HEADER |
                   LLL_EXT | LLL_CLIENT | LLL_LATENCY | LLL_DEBUG;
#ifdef __ANDROID__
        lws_set_log_level(logs, LogLws);
#else
        lws_set_log_level(logs, nullptr);
#endif
    }

    return RUN_ALL_TESTS();
}
