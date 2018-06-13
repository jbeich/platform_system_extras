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
#include <sysexits.h>

#include <memory>

#include <android-base/strings.h>
#include <liblp/builder.h>
#include <liblp/writer.h>

using namespace android;
using namespace android::fs_mgr;

/* Prints program usage to |where|. */
static int usage(int /* argc */, char* argv[]) {
    fprintf(stderr,
            "%s - command-line tool for creating Android Logical Partition images.\n"
            "\n"
            "Usage:\n"
            "  %s [options]\n"
            "\n"
            "Required options:\n"
            "  --device-size=SIZE         Size of the block device for logical partitions.\n"
            "  --metadata-size=SIZE       Maximum size to reserve for partition metadata.\n"
            "  --metadata-slots=COUNT     Number of slots to store metadata copies.\n"
            "  --partition=DATA           Add a partition given the data, see below.\n"
            "  --output=FILE              Output file.\n"
            "\n"
            "Partition format:\n"
            "  <name>:<guid>:<attributes>:<size>\n"
            "  Attrs must be 'none' or 'readonly'.\n",
            argv[0], argv[0]);
    return EX_USAGE;
}

static bool ParseUint64(const std::string& value, uint64_t* out) {
    char* endptr;
    *out = strtoll(value.c_str(), &endptr, 10);
    return *endptr == '\0';
}

static bool ParseUint32(const std::string& value, uint32_t* out) {
    char* endptr;
    *out = strtol(value.c_str(), &endptr, 10);
    return *endptr == '\0';
}

int main(int argc, char* argv[]) {
    struct option options[] = {
        { "device-size", required_argument, nullptr, 'd' },
        { "metadata-size", required_argument, nullptr, 'm' },
        { "metadata-slots", required_argument, nullptr, 's' },
        { "partition", required_argument, nullptr, 'p' },
        { "output", required_argument, nullptr, 'o' },
        { "help", no_argument, nullptr, 'h' },
        { nullptr, 0, nullptr, 0 },
    };

    uint64_t blockdevice_size = 0;
    uint32_t metadata_size = 0;
    uint32_t metadata_slots = 0;
    std::string output_path;
    std::vector<std::string> partitions;

    int rv;
    int index;
    while ((rv = getopt_long_only(argc, argv, "d:m:s:p:o:h", options, &index)) != -1) {
        switch (rv) {
            case 'h':
                return usage(argc, argv);
            case 'd':
                if (!ParseUint64(optarg, &blockdevice_size)) {
                    fprintf(stderr, "Invalid argument to --device-size.\n");
                    return EX_USAGE;
                }
                break;
            case 'm':
                if (!ParseUint32(optarg, &metadata_size)) {
                    fprintf(stderr, "Invalid argument to --metadata-size.\n");
                    return EX_USAGE;
                }
                break;
            case 's':
                if (!ParseUint32(optarg, &metadata_slots)) {
                    fprintf(stderr, "Invalid argument to --metadata-slots.\n");
                    return EX_USAGE;
                }
                break;
            case 'p':
                partitions.push_back(optarg);
                break;
            case 'o':
                output_path = optarg;
                break;
            default:
                break;
        }
    }

    // Check for empty arguments so we can print a more helpful message rather
    // than error on each individual missing argument.
    if (optind == 1) {
        return usage(argc, argv);
    }

    if (!blockdevice_size) {
        fprintf(stderr, "--device-size needs more than 0 bytes of disk space.\n");
        return EX_USAGE;
    }
    if (!metadata_size) {
        fprintf(stderr, "--metadata-size must be more than 0 bytes.\n");
        return EX_USAGE;
    }
    if (!metadata_slots) {
        fprintf(stderr, "--metadata-slots must be more than 0.\n");
        return EX_USAGE;
    }
    if (output_path.empty()) {
        fprintf(stderr, "--output must specify a valid path.\n");
        return EX_USAGE;
    }
    if (partitions.empty()) {
        fprintf(stderr, "Partition table must have at least one entry.\n");
        return EX_USAGE;
    }

    std::unique_ptr<MetadataBuilder> builder =
            MetadataBuilder::New(blockdevice_size, metadata_size, metadata_slots);

    for (const auto& partition_info : partitions) {
        std::vector<std::string> parts = android::base::Split(partition_info, ":");
        if (parts.size() != 4) {
            fprintf(stderr, "Partition info has invalid formatting.\n");
            return EX_USAGE;
        }

        std::string name = parts[0];
        if (!name.length()) {
            fprintf(stderr, "Partition must have a valid name.\n");
            return EX_USAGE;
        }

        uint64_t size;
        if (!ParseUint64(parts[3], &size)) {
            fprintf(stderr, "Partition must have a valid size.\n");
            return EX_USAGE;
        }

        uint32_t attribute_flags = 0;
        std::string attributes = parts[2];
        if (attributes == "readonly") {
            attribute_flags |= LP_PARTITION_ATTR_READONLY;
        } else if (attributes != "none") {
            fprintf(stderr, "Attribute not recognized: %s\n", attributes.c_str());
            return EX_USAGE;
        }

        Partition* partition = builder->AddPartition(name, parts[1], attribute_flags);
        if (!builder->GrowPartition(partition, size)) {
            fprintf(stderr, "Not enough space on device for partition %s with size %" PRIu64 "\n",
                    name.c_str(), size);
            return EX_SOFTWARE;
        }
    }

    std::unique_ptr<LpMetadata> metadata = builder->Export();
    if (!WriteToImageFile(output_path.c_str(), *metadata.get())) {
        return EX_CANTCREAT;
    }

    return EX_OK;
}
