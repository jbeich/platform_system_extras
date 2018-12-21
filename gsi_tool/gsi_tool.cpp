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
        { "image-size", required_argument, nullptr, 's' },
        { nullptr, 0, nullptr, 0 },
    };

    int64_t image_size = 0;

    int rv, index;
    while ((rv = getopt_long_only(argc, argv, "", options, &index)) != -1) {
        switch (rv) {
            case 's':
                if (!android::base::ParseInt(optarg, &image_size) || image_size <= 0) {
                    std::cout << "Could not parse image size: " << optarg << std::endl;
                    return EX_USAGE;
                }
                break;
        }
    }

    if (image_size <= 0) {
        std::cout << "Must specify --image-size." << std::endl;
        return EX_USAGE;
    }

    android::base::unique_fd input(dup(1));
    if (input < 0) {
        perror("dup");
        return EX_SOFTWARE;
    }

    auto status = vold->startGsiInstall(image_size);
    if (!status.isOk()) {
        std::cout << "Could not start live image install: " << status.exceptionMessage().string()
                  << std::endl;
        return EX_SOFTWARE;
    }

    status = vold->commitGsiChunk(input, image_size);
    if (!status.isOk()) {
        std::cout << "Could not commit live image data: " << status.exceptionMessage().string()
                  << std::endl;
        return EX_SOFTWARE;
    }

    status = vold->setGsiBootable();
    if (!status.isOk()) {
        std::cout << "Could not make live image bootable: " << status.exceptionMessage().string()
                  << std::endl;
        return EX_SOFTWARE;
    }
    return 0;
}

static int Wipe(sp<IVold> vold, int argc, char** /* argv */) {
    if (argc > 1) {
        std::cout << "Unrecognized arguments to wipe." << std::endl;
        return EX_USAGE;
    }
    auto status = vold->removeGsiInstall();
    if (!status.isOk()) {
        std::cout << status.exceptionMessage().string() << std::endl;
        return EX_SOFTWARE;
    }
    std::cout << "Live image install successfully removed." << std::endl;
    return 0;
}

int main(int argc, char** argv) {
    auto vold = getService();
    if (!vold) {
        std::cout << "Could not connect to the vold service." << std::endl;
        return EX_NOPERM;
    }

    if (1 >= argc) {
        std::cout << "Expected command." << std::endl;
        return EX_USAGE;
    }

    std::string command = argv[1];
    auto iter = kCommandMap.find(command);
    if (iter == kCommandMap.end()) {
        std::cout << "Unrecognized command: " << command << std::endl;
        return EX_USAGE;
    }
    return iter->second(vold, argc - 1, argv + 1);
}
