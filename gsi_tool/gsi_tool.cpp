/*
 * Copyright (C) 2008 The Android Open Source Project
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
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>

#include <functional>
#include <iostream>
#include <map>
#include <string>

#include <android-base/parseint.h>
#include <android-base/unique_fd.h>
#include <android/os/IVold.h>
#include <binder/IServiceManager.h>

using android::sp;
using android::os::IVold;
using CommandCallback = std::function<int(sp<IVold>, int, char**)>;

static int Install(sp<IVold> vold, int argc, char** argv);
static int Wipe(sp<IVold> vold, int argc, char** argv);

static const std::map<std::string, CommandCallback> kCommandMap = {
    { "install", Install },
    { "wipe", Wipe },
};

static constexpr char kRedColor[] = "\x1b[31m";
static constexpr char kGreenColor[] = "\x1b[32m";
static constexpr char kResetColor[] = "\x1b[0m";

class ProgressBar {
  public:
    ProgressBar(uint32_t columns, uint32_t total_units) : columns_(columns), total_(total_units) {
        units_per_column_ = total_ / columns_;
    }

    void Show(uint32_t units_done) {
        if (units_done > total_) {
            fprintf(stderr, "Invalid progress\n");
            return;
        }
        float percentage = ((1.0 * units_done) / total_) * 100;
        uint32_t nr_bars = units_done / units_per_column_;
        uint32_t nr_dashes = columns_ - nr_bars;
        std::string bars = std::string(nr_bars, '|');
        std::string dashes = std::string(nr_dashes, '-');
        printf(kGreenColor);
        printf("\r%5.1f%%", percentage);
        printf(" [");
        printf("%s", bars.c_str());
        printf(kRedColor);
        printf("%s", dashes.c_str());
        printf(kGreenColor);
        printf("]");
        fflush(stdout);
    }

    ~ProgressBar() = default;

  private:
    uint32_t columns_;
    uint32_t total_;
    uint32_t units_per_column_;
};

static sp<IVold> getService() {
    auto sm = android::defaultServiceManager();
    auto name = android::String16("vold");
    android::sp<android::IBinder> res = sm->checkService(name);
    if (!res) {
        return nullptr;
    }
    return android::interface_cast<android::os::IVold>(res);
}

static int Install([[maybe_unused]] sp<IVold> vold, int argc, char** argv) {
    struct option options[] = {
        { "gsi-size", required_argument, nullptr, 's' },
        { "userdata-size", required_argument, nullptr, 'u' },
        { nullptr, 0, nullptr, 0 },
    };

    int64_t gsi_size = 0;
    int64_t userdata_size = 0;

    int rv, index;
    while ((rv = getopt_long_only(argc, argv, "", options, &index)) != -1) {
        switch (rv) {
            case 's':
                if (!android::base::ParseInt(optarg, &gsi_size) || gsi_size <= 0) {
                    fprintf(stderr, "Could not parse image size: %s\n", optarg);
                    return EX_USAGE;
                }
                break;
            case 'u':
                if (!android::base::ParseInt(optarg, &userdata_size) || userdata_size <= 0) {
                    fprintf(stderr, "Could not parse image size: %s\n", optarg);
                    return EX_USAGE;
                }
                break;
        }
    }

    if (gsi_size <= 0) {
        fprintf(stderr, "Must specify --gsi-size.\n");
        return EX_USAGE;
    }
    if (userdata_size <= 0) {
        fprintf(stderr, "Must specify --userdata-size\n");
        return EX_USAGE;
    }

    android::base::unique_fd input(dup(1));
    if (input < 0) {
        perror("dup");
        return EX_SOFTWARE;
    }

    auto status = vold->startGsiInstall(gsi_size, userdata_size);
    if (!status.isOk()) {
        fprintf(stderr, "Could not start live image install: %s\n", status.exceptionMessage().string());
        return EX_SOFTWARE;
    }

    // TODO: Fix this for < 4k blocks
    uint64_t nr_chunks = gsi_size / getpagesize();
    ProgressBar bar(80, gsi_size);
    for (uint64_t chunk = 0; chunk < nr_chunks; chunk++) {
      status = vold->commitGsiChunk(input, getpagesize());
      if (!status.isOk()) {
        fprintf(stderr, "Could not commit live image data: %s\n", status.exceptionMessage().string());
        return EX_SOFTWARE;
      }
      bar.Show((chunk + 1) * getpagesize());
    }

    printf(kResetColor);
    printf("\n");

    status = vold->setGsiBootable();
    if (!status.isOk()) {
        fprintf(stderr, "Could not make live image bootable: %s\n", status.exceptionMessage().string());
        return EX_SOFTWARE;
    }
    return 0;
}

static int Wipe(sp<IVold> vold, int argc, char** /* argv */) {
    if (argc > 1) {
        fprintf(stderr, "Unrecognized arguments to wipe.\n");
        return EX_USAGE;
    }
    auto status = vold->removeGsiInstall();
    if (!status.isOk()) {
        fprintf(stderr, "%s\n", status.exceptionMessage().string());
        return EX_SOFTWARE;
    }
    printf("Live image install successfully removed.\n");
    return 0;
}

int main(int argc, char** argv) {
    auto vold = getService();
    if (!vold) {
        fprintf(stderr, "Could not connect to the vold service.\n");
        return EX_NOPERM;
    }

    if (1 >= argc) {
        fprintf(stderr, "Expected command.\n");
        return EX_USAGE;
    }

    std::string command = argv[1];
    auto iter = kCommandMap.find(command);
    if (iter == kCommandMap.end()) {
        fprintf(stderr, "Unrecognized command: %s\n", command.c_str());
        return EX_USAGE;
    }
    return iter->second(vold, argc - 1, argv + 1);
}
