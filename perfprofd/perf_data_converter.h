
#include "system/extras/perfprofd/perf_profile.pb.h"

using std::string;

namespace wireless_android_logging_awp {

wireless_android_play_playlog::AndroidPerfProfile
RawPerfDataToAndroidPerfProfile(const std::string &perf_file);

}  // namespace wireless_android_logging_awp
