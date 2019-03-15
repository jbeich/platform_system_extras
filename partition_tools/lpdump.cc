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
#include <sys/stat.h>
#include <sys/types.h>
#include <sysexits.h>
#include <unistd.h>

#include <iostream>
#include <string>
#include <vector>

#include <android-base/parseint.h>
#include <android-base/properties.h>
#include <android-base/strings.h>
#ifdef __ANDROID__
#include <fs_mgr.h>
#endif
#include <jsonpb/jsonpb.h>
#include <liblp/liblp.h>
#include <liblp/builder.h>

#include "dynamic_partitions_device_info.pb.h"
using namespace android;
using namespace android::fs_mgr;

static int usage(int /* argc */, char* argv[], std::ostream& cerr) {
    cerr << argv[0]
         << " - command-line tool for dumping Android Logical Partition images.\n"
            "\n"
            "Usage:\n"
            "  "
         << argv[0]
         << " [-s <SLOT#>|--slot=<SLOT#>] [FILE|DEVICE]\n"
            "\n"
            "Options:\n"
            "  -s, --slot=N     Slot number or suffix.\n";
    return EX_USAGE;
}

static std::string BuildFlagString(const std::vector<std::string>& strings) {
    return strings.empty() ? "none" : android::base::Join(strings, ",");
}

static std::string BuildAttributeString(uint32_t attrs) {
    std::vector<std::string> strings;
    if (attrs & LP_PARTITION_ATTR_READONLY) strings.emplace_back("readonly");
    if (attrs & LP_PARTITION_ATTR_SLOT_SUFFIXED) strings.emplace_back("slot-suffixed");
    return BuildFlagString(strings);
}

static std::string BuildGroupFlagString(uint32_t flags) {
    std::vector<std::string> strings;
    if (flags & LP_GROUP_SLOT_SUFFIXED) strings.emplace_back("slot-suffixed");
    return BuildFlagString(strings);
}

static std::string BuildBlockDeviceFlagString(uint32_t flags) {
    std::vector<std::string> strings;
    if (flags & LP_BLOCK_DEVICE_SLOT_SUFFIXED) strings.emplace_back("slot-suffixed");
    return BuildFlagString(strings);
}

static bool IsBlockDevice(const char* file) {
    struct stat s;
    return !stat(file, &s) && S_ISBLK(s.st_mode);
}

static std::string RemoveSuffix(std::string& s, const std::string& suffix) {
    if (base::EndsWith(s, suffix)) {
        return s.substr(0, s.length() - suffix.length());
    }
    return s;
}

static int PrintJson(const LpMetadata& metadata, std::ostream& cout, std::ostream& cerr) {
    auto builder = MetadataBuilder::New(metadata);

    DynamicPartitionsDeviceInfoProto proto;

    if (base::GetBoolProperty("ro.boot.dynamic_partitions", false)) {
        proto.set_enabled(true);
    }

    if (base::GetBoolProperty("ro.boot.dynamic_partitions_retrofit", false)) {
        proto.set_retrofit(true);
    }

    std::string slot_suffix = base::GetProperty("ro.boot.slot_suffix", "");

    for (const auto& partition : metadata.partitions) {
        std::string partition_name = GetPartitionName(partition);
        std::string group_name = GetPartitionGroupName(metadata.groups[partition.group_index]);
        if (!base::EndsWith(partition_name, slot_suffix) ||
            !base::EndsWith(group_name, slot_suffix))
            continue;
        auto partition_proto = proto.add_partitions();
        partition_proto->set_is_dynamic(true);
        partition_proto->set_name(RemoveSuffix(partition_name, slot_suffix));
        partition_proto->set_group_name(RemoveSuffix(group_name, slot_suffix));

        if (!builder) continue;
        auto builder_partition = builder->FindPartition(partition_name);
        if (!builder_partition) continue;
        partition_proto->set_size(builder_partition->size());
    }

    for (const auto& group : metadata.groups) {
        std::string group_name = GetPartitionGroupName(group);
        if (!base::EndsWith(group_name, slot_suffix)) continue;
        auto group_proto = proto.add_groups();
        group_proto->set_name(RemoveSuffix(group_name, slot_suffix));
        group_proto->set_maximum_size(group.maximum_size);
    }

    for (const auto& block_device : metadata.block_devices) {
        std::string name = GetBlockDevicePartitionName(block_device);
        auto block_device_proto = proto.add_block_devices();
        block_device_proto->set_name(RemoveSuffix(name, slot_suffix));
        block_device_proto->set_size(block_device.size);
    }

    auto error_or_json = jsonpb::MessageToJsonString(proto);
    if (!error_or_json.ok()) {
        cerr << error_or_json.error() << "\n";
        return EX_SOFTWARE;
    }
    cout << *error_or_json;
    return EX_OK;
}

class FileOrBlockDeviceOpener final : public PartitionOpener {
public:
    android::base::unique_fd Open(const std::string& path, int flags) const override {
        // Try a local file first.
        android::base::unique_fd fd(open(path.c_str(), flags));
        if (fd >= 0) {
            return fd;
        }
        return PartitionOpener::Open(path, flags);
    }
};

int LpdumpMain(int argc, char* argv[], std::ostream& cout, std::ostream& cerr) {
    struct option options[] = {
        {"slot", required_argument, nullptr, 's'},
        {"help", no_argument, nullptr, 'h'},
        {"json", no_argument, nullptr, 'j'},
        {nullptr, 0, nullptr, 0},
    };

    int rv;
    int index;
    uint32_t slot = 0;
    bool json = false;
    while ((rv = getopt_long_only(argc, argv, "s:h", options, &index)) != -1) {
        switch (rv) {
            case 'h':
                return usage(argc, argv, cerr);
            case 's':
                if (!android::base::ParseUint(optarg, &slot)) {
                    slot = SlotNumberForSlotSuffix(optarg);
                }
                break;
            case 'j':
                json = true;
                break;
        }
    }

    std::unique_ptr<LpMetadata> pt;
    if (optind < argc) {
        const char* file = argv[optind++];

        FileOrBlockDeviceOpener opener;
        pt = ReadMetadata(opener, file, slot);
        if (!pt && !IsBlockDevice(file)) {
            pt = ReadFromImageFile(file);
        }
    } else {
#ifdef __ANDROID__
        auto slot_number = SlotNumberForSlotSuffix(fs_mgr_get_slot_suffix());
        pt = ReadMetadata(fs_mgr_get_super_partition_name(), slot_number);
#else
        return usage(argc, argv, cerr);
#endif
    }
    if (!pt) {
        cerr << "Failed to read metadata.\n";
        return EX_NOINPUT;
    }

    if (json) {
        return PrintJson(*pt, cout, cerr);
    }
    cout << "Metadata version: " << pt->header.major_version << "." << pt->header.minor_version
         << "\n";
    cout << "Metadata size: " << (pt->header.header_size + pt->header.tables_size) << " bytes\n";
    cout << "Metadata max size: " << pt->geometry.metadata_max_size << " bytes\n";
    cout << "Metadata slot count: " << pt->geometry.metadata_slot_count << "\n";
    cout << "Partition table:\n";
    cout << "------------------------\n";

    for (const auto& partition : pt->partitions) {
        std::string name = GetPartitionName(partition);
        std::string group_name = GetPartitionGroupName(pt->groups[partition.group_index]);
        cout << "  Name: " << name << "\n";
        cout << "  Group: " << group_name << "\n";
        cout << "  Attributes: " << BuildAttributeString(partition.attributes) << "\n";
        cout << "  Extents:\n";
        uint64_t first_sector = 0;
        for (size_t i = 0; i < partition.num_extents; i++) {
            const LpMetadataExtent& extent = pt->extents[partition.first_extent_index + i];
            cout << "    " << first_sector << " .. " << (first_sector + extent.num_sectors - 1)
                 << " ";
            first_sector += extent.num_sectors;
            if (extent.target_type == LP_TARGET_TYPE_LINEAR) {
                const auto& block_device = pt->block_devices[extent.target_source];
                std::string device_name = GetBlockDevicePartitionName(block_device);
                cout << "linear " << device_name.c_str() << " " << extent.target_data;
            } else if (extent.target_type == LP_TARGET_TYPE_ZERO) {
                cout << "zero";
            }
            cout << "\n";
        }
        cout << "------------------------\n";
    }

    cout << "Block device table:\n";
    cout << "------------------------\n";
    for (const auto& block_device : pt->block_devices) {
        std::string partition_name = GetBlockDevicePartitionName(block_device);
        cout << "  Partition name: " << partition_name << "\n";
        cout << "  First sector: " << block_device.first_logical_sector << "\n";
        cout << "  Size: " << block_device.size << " bytes\n";
        cout << "  Flags: " << BuildBlockDeviceFlagString(block_device.flags) << "\n";
        cout << "------------------------\n";
    }

    cout << "Group table:\n";
    cout << "------------------------\n";
    for (const auto& group : pt->groups) {
        std::string group_name = GetPartitionGroupName(group);
        cout << "  Name: " << group_name << "\n";
        cout << "  Maximum size: " << group.maximum_size << " bytes\n";
        cout << "  Flags: " << BuildGroupFlagString(group.flags) << "\n";
        cout << "------------------------\n";
    }

    return EX_OK;
}

int LpdumpMain(int argc, char* argv[]) {
    return LpdumpMain(argc, argv, std::cout, std::cerr);
}
