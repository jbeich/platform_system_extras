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

#include <getopt.h>
#include <inttypes.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sysexits.h>
#include <unistd.h>

#include <string>

#include <liblp/reader.h>

using namespace android;
using namespace android::fs_mgr;

static int usage(int /* argc */, char* argv[]) {
    fprintf(stderr,
            "%s - command-line tool for dumping Android Logical Partition images.\n"
            "\n"
            "Usage:\n"
            "  %s [-s,--slot] file-or-device\n"
            "\n"
            "Options:\n"
            "  -s, --slot=N     Slot number or suffix.\n",
            argv[0], argv[0]);
    return EX_USAGE;
}

static std::string BuildAttributeString(uint32_t attrs) {
    if (attrs & LP_PARTITION_ATTR_READONLY) {
        return "readonly";
    }
    return "none";
}

static bool IsBlockDevice(const char* file) {
    struct stat s;
    if (stat(file, &s) != 0) {
        return false;
    }
    return S_ISBLK(s.st_mode);
}

static bool ParseUint32(const std::string& value, uint32_t* out) {
    char* endptr;
    *out = strtol(value.c_str(), &endptr, 10);
    return *endptr == '\0';
}

int main(int argc, char* argv[]) {
    struct option options[] = {
        { "slot", required_argument, nullptr, 's' },
        { "help", no_argument, nullptr, 'h' },
        { nullptr, 0, nullptr, 0 },
    };

    int rv;
    int index;
    uint32_t slot = 0;
    while ((rv = getopt_long_only(argc, argv, "s:h", options, &index)) != -1) {
        switch (rv) {
            case 'h':
                return usage(argc, argv);
            case 's':
                if (!ParseUint32(optarg, &slot)) {
                    slot = SlotNumberForSlotSuffix(optarg);
                }
                break;
        }
    }

    if (optind >= argc) {
        return usage(argc, argv);
    }
    const char* file = argv[optind++];

    std::unique_ptr<LpMetadata> pt;
    if (IsBlockDevice(file)) {
        pt = ReadMetadata(file, slot);
    } else {
        pt = ReadFromImageFile(file);
    }
    if (!pt) {
        fprintf(stderr, "Failed to read metadata.\n");
        return EX_NOINPUT;
    }

    fprintf(stdout, "Metadata version: %u.%u\n", pt->header.major_version, pt->header.minor_version);
    fprintf(stdout, "Metadata size: %u bytes\n", pt->header.header_size + pt->header.tables_size);
    fprintf(stdout, "Metadata max size: %u bytes\n", pt->geometry.metadata_max_size);
    fprintf(stdout, "Metadata slot count: %u\n", pt->geometry.metadata_slot_count);
    fprintf(stdout, "First logical sector: %" PRIu64 "\n", pt->geometry.first_logical_sector);
    fprintf(stdout, "Last logical sector: %" PRIu64 "\n", pt->geometry.last_logical_sector);
    fprintf(stdout, "Partition table:\n");
    fprintf(stdout, "------------------------\n");

    for (const auto& partition : pt->partitions) {
        std::string name = GetPartitionName(partition);
        std::string guid = GetPartitionGuid(partition);
        fprintf(stdout, "  Name: %s\n", name.c_str());
        fprintf(stdout, "  GUID: %s\n", guid.c_str());
        fprintf(stdout, "  Attributes: %s\n", BuildAttributeString(partition.attributes).c_str());
        fprintf(stdout, "  Extents:\n");
        uint64_t first_sector = 0;
        for (size_t i = 0; i < partition.num_extents; i++) {
            const LpMetadataExtent& extent = pt->extents[partition.first_extent_index + i];
            fprintf(stdout, "    %" PRIu64 " .. %" PRIu64 " ", first_sector,
                    (first_sector + extent.num_sectors - 1));
            first_sector += extent.num_sectors;
            if (extent.target_type == LP_TARGET_TYPE_LINEAR) {
                fprintf(stdout, "linear %" PRIu64, extent.target_data);
            } else if (extent.target_type == LP_TARGET_TYPE_ZERO) {
                fprintf(stdout, "zero");
            }
            fprintf(stdout, "\n");
        }
        fprintf(stdout, "------------------------\n");
    }

    return EX_OK;
}
