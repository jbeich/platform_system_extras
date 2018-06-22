/*
 * Copyright (C) 2018 The Android Open Source Project
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

/*
 * This h file together with bpf_kern.c is used for compiling the eBPF kernel
 * program. To generate the bpf_kern.o file manually, use the clang prebuilt in
 * this android tree to compile the files with --target=bpf options. For
 * example, in system/netd/ directory, execute the following command:
 * $: ANDROID_BASE_DIRECTORY/prebuilts/clang/host/linux-x86/clang-4691093/bin/clang  \
 *    -I ANDROID_BASE_DIRECTORY/bionic/libc/kernel/uapi/ \
 *    -I ANDROID_BASE_DIRECTORY/system/netd/bpfloader/ \
 *    -I ANDROID_BASE_DIRECTORY/bionic/libc/kernel/android/uapi/ \
 *    -I ANDROID_BASE_DIRECTORY/bionic/libc/include \
 *    -I ANDROID_BASE_DIRECTORY/system/netd/libbpf/include  \
 *    --target=bpf -O2 -c bpfloader/bpf_kern.c -o bpfloader/bpf_kern.o
 */

#include <linux/bpf.h>
#include <stdint.h>

#define ELF_SEC(NAME) __attribute__((section(NAME), used))

#define TEST_PROG_NAME "test_prog"

#define COOKIE_STATS_MAP_A 0xc001eaaaffffffff
#define COOKIE_STATS_MAP_B 0xc001eaabffffffff
#define CONFIGURATION_MAP 0xc0f1a10affffffff

struct stats_value {
    uint64_t rxPackets;
    uint64_t rxBytes;
    uint64_t txPackets;
    uint64_t txBytes;
};

