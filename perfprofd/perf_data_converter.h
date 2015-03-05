
#include "system/extras/perfprofd/perf_profile.pb.h"

using std::string;

namespace wireless_android_logging_awp {

//
// Return value is a pair <X,Y> where X is a boolean indicating
// whether the perf.data read/encode was successful and Y is the
// encoded data.
//
std::pair<bool, wireless_android_play_playlog::AndroidPerfProfile>
RawPerfDataToAndroidPerfProfile(const string &perf_file);

}  // namespace wireless_android_logging_awp
