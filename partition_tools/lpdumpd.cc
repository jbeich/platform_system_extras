#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include <android-base/logging.h>
#include <android-base/strings.h>
#include <android/lpdump/BnLpdump.h>
#include <android/lpdump/ILpdump.h>
#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>

int LpdumpMain(int argc, char* argv[], std::ostream&, std::ostream&);

namespace android {
namespace lpdump {

using binder::Status;

class Lpdump : public BnLpdump {
  public:
    Lpdump() = default;
    virtual ~Lpdump() = default;

    Status run(const std::vector<std::string>& args, std::string* aidl_return) override {
        if (args.size() > std::numeric_limits<int>::max()) {
            return Status::fromExceptionCode(Status::EX_ILLEGAL_ARGUMENT);
        }
        std::vector<std::string> m_args = args;
        char* argv[m_args.size()];
        for (size_t i = 0; i < m_args.size(); ++i) {
            argv[i] = m_args[i].data();
        }
        LOG(INFO) << "Dumping with args: " << base::Join(args, " ");
        std::stringstream output;
        int ret = LpdumpMain((int)m_args.size(), argv, output, output);
        if (ret == 0) {
            *aidl_return = output.str();
            return Status::ok();
        } else {
            return Status::fromServiceSpecificError(ret, output.str().c_str());
        }
    }
};

}  // namespace lpdump
}  // namespace android

int main(int, char**) {
    using namespace android;

    sp<lpdump::Lpdump> service = new lpdump::Lpdump();
    defaultServiceManager()->addService(String16("lpdump_service"), service);
    LOG(INFO) << "lpdumpd starting";
    ProcessState::self()->startThreadPool();
    IPCThreadState::self()->joinThreadPool();
    return 0;
}
