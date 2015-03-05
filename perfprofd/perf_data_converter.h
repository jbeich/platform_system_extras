
#include "system/extras/perfprofd/perf_profile.pb.h"

namespace wireless_android_logging_awp {


//
// Return value is 1 for success, 0 on error
//
int RawPerfDataToAndroidPerfProfile(const std::string &perf_file,
                                    wireless_android_play_playlog::AndroidPerfProfile &result,
                                    double encoded_sample_threshold);

}  // namespace wireless_android_logging_awp
