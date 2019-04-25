/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include <iostream>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include <android-base/file.h>
#include <android-base/parseint.h>
#include <liblp/liblp.h>
#include <sparse/sparse.h>

using namespace android::fs_mgr;
using android::base::unique_fd;
using SparsePtr = std::unique_ptr<sparse_file, decltype(&sparse_file_destroy)>;

class ImageExtractor final {
  public:
    ImageExtractor(unique_fd&& image_fd, std::unique_ptr<LpMetadata>&& metadata,
                   std::unordered_set<std::string>&& partitions, const std::string& output_dir);

    bool Extract();

    void set_should_sparse(bool should_sparse) { should_sparse_ = should_sparse; }

  private:
    bool BuildPartitionList();
    bool ExtractOne(const LpMetadataPartition* partition);
    bool ExtractExtent(const LpMetadataExtent& extent, int output_fd);
    bool WriteSparseImage(int temp_fd, const std::string& partition_name);

    unique_fd image_fd_;
    std::unique_ptr<LpMetadata> metadata_;
    std::unordered_set<std::string> partitions_;
    std::string output_dir_;
    std::unordered_map<std::string, const LpMetadataPartition*> partition_map_;
    bool should_sparse_ = false;
};

/* Prints program usage to |where|. */
static int usage(int /* argc */, char* argv[]) {
    fprintf(stderr,
            "%s - command-line tool for extracting partition images from super\n"
            "\n"
            "Usage:\n"
            "  %s [options...] SUPER_IMAGE [OUTPUT_DIR]\n"
            "\n"
            "Options:\n"
            "  -p, --partition=NAME     Extract the named partition. This can\n"
            "                           be specified multiple times.\n"
            "  -s, --sparse             Write sparse images.\n"
            "  -S, --slot=NUM           Slot number (default is 0).\n",
            argv[0], argv[0]);
    return EX_USAGE;
}

int main(int argc, char* argv[]) {
    // clang-format off
    struct option options[] = {
        { "partition",  required_argument,  nullptr, 'p' },
        { "sparse",     no_argument,        nullptr, 's' },
        { "slot",       required_argument,  nullptr, 'S' },
        { nullptr,      0,                  nullptr, 0 },
    };
    // clang-format on

    bool should_sparse = false;
    uint32_t slot_num = 0;
    std::unordered_set<std::string> partitions;

    int rv, index;
    while ((rv = getopt_long_only(argc, argv, "+p:sh", options, &index)) != -1) {
        switch (rv) {
            case 'h':
                return usage(argc, argv);
            case '?':
                std::cerr << "Unrecognized argument.\n";
                return usage(argc, argv);
            case 's':
                should_sparse = true;
                break;
            case 'S':
                if (!android::base::ParseUint(optarg, &slot_num)) {
                    std::cerr << "Slot must be a valid unsigned number.\n";
                    return usage(argc, argv);
                }
                break;
            case 'p':
                partitions.emplace(optarg);
                break;
        }
    }

    if (optind + 1 > argc) {
        std::cerr << "Missing super image argument.\n";
        return usage(argc, argv);
    }
    std::string super_path = argv[optind++];

    std::string output_dir = ".";
    if (optind + 1 <= argc) {
        output_dir = argv[optind++];
    }

    if (optind < argc) {
        std::cerr << "Unrecognized command-line arguments.\n";
        return usage(argc, argv);
    }

    // Done reading arguments; open super.img. PartitionOpener will decorate
    // relative paths with /dev/block/by-name, so get an absolute path here.
    std::string abs_super_path;
    if (!android::base::Realpath(super_path, &abs_super_path)) {
        std::cerr << "realpath failed: " << super_path << ": " << strerror(errno) << "\n";
        return EX_OSERR;
    }

    unique_fd fd(open(super_path.c_str(), O_RDONLY | O_CLOEXEC));
    if (fd < 0) {
        std::cerr << "open failed: " << abs_super_path << ": " << strerror(errno) << "\n";
        return EX_OSERR;
    }

    auto metadata = ReadMetadata(abs_super_path, slot_num);
    if (!metadata) {
        SparsePtr ptr(sparse_file_import(fd, false, false), sparse_file_destroy);
        if (ptr) {
            std::cerr << "This image appears to be a sparse image. It must be unsparsed to be"
                      << " unpacked.\n";
            return EX_USAGE;
        }
        std::cerr << "Image does not appear to be in super-partition format.\n";
        return EX_USAGE;
    }

    ImageExtractor extractor(std::move(fd), std::move(metadata), std::move(partitions), output_dir);
    extractor.set_should_sparse(should_sparse);

    if (!extractor.Extract()) {
        return EX_SOFTWARE;
    }
    return EX_OK;
}

ImageExtractor::ImageExtractor(unique_fd&& image_fd, std::unique_ptr<LpMetadata>&& metadata,
                               std::unordered_set<std::string>&& partitions,
                               const std::string& output_dir)
    : image_fd_(std::move(image_fd)),
      metadata_(std::move(metadata)),
      partitions_(std::move(partitions)),
      output_dir_(output_dir) {}

bool ImageExtractor::Extract() {
    if (!BuildPartitionList()) {
        return false;
    }

    for (const auto& [name, info] : partition_map_) {
        if (!ExtractOne(info)) {
            return false;
        }
    }
    return true;
}

bool ImageExtractor::BuildPartitionList() {
    bool extract_all = partitions_.empty();

    for (const auto& partition : metadata_->partitions) {
        auto name = GetPartitionName(partition);
        if (extract_all || partitions_.count(name)) {
            partition_map_[name] = &partition;
            partitions_.erase(name);
        }
    }

    if (!extract_all && !partitions_.empty()) {
        std::cerr << "Could not find partition: " << *partitions_.begin() << "\n";
        return false;
    }
    return true;
}

bool ImageExtractor::ExtractOne(const LpMetadataPartition* partition) {
    // Validate the extents and find the total image size.
    uint64_t total_size = 0;
    for (uint32_t i = 0; i < partition->num_extents; i++) {
        uint32_t index = partition->first_extent_index + i;
        const LpMetadataExtent& extent = metadata_->extents[index];

        if (extent.target_type != LP_TARGET_TYPE_LINEAR) {
            std::cerr << "Unsupported target type in extent: " << extent.target_type << "\n";
            return false;
        }
        if (extent.target_source != 0) {
            std::cerr << "Split super devices are not supported.\n";
            return false;
        }
        total_size += extent.num_sectors * LP_SECTOR_SIZE;
    }

    // When sparsing, we need to make a temporary file.
    std::string output_path = output_dir_ + "/" + GetPartitionName(*partition) + ".img";
    if (should_sparse_) {
        output_path = "." + output_path;
    }

    unique_fd output_fd(open(output_path.c_str(), O_RDWR | O_CLOEXEC | O_CREAT | O_TRUNC, 0644));
    if (output_fd < 0) {
        std::cerr << "open failed: " << output_path << ": " << strerror(errno) << "\n";
        return false;
    }

    for (uint32_t i = 0; i < partition->num_extents; i++) {
        uint32_t index = partition->first_extent_index + i;
        const LpMetadataExtent& extent = metadata_->extents[index];

        if (!ExtractExtent(extent, output_fd)) {
            return false;
        }
    }

    if (should_sparse_) {
        if (unlink(output_path.c_str()) < 0) {
            std::cerr << "unable to delete temp file " << output_path << "\n";
            return false;
        }
        return WriteSparseImage(output_fd, GetPartitionName(*partition));
    }
    return true;
}

bool ImageExtractor::ExtractExtent(const LpMetadataExtent& extent, int output_fd) {
    static const size_t kChunkSize = 1024 * 1024;
    auto buffer = std::make_unique<char[]>(kChunkSize);

    // Note: don't use sendfile() since we may want this tool on Windows.
    off_t position = extent.target_data * LP_SECTOR_SIZE;
    uint64_t remaining_bytes = extent.num_sectors * LP_SECTOR_SIZE;
    while (remaining_bytes) {
        // We don't use std::min here because the casts get ugly.
        size_t bytes_to_read =
                (remaining_bytes < kChunkSize) ? static_cast<size_t>(remaining_bytes) : kChunkSize;
        if (lseek(image_fd_, position, SEEK_SET) < 0) {
            std::cerr << "lseek failed: " << strerror(errno) << "\n";
            return false;
        }
        if (!android::base::ReadFully(image_fd_, buffer.get(), bytes_to_read)) {
            std::cerr << "read failed: " << strerror(errno) << "\n";
            return false;
        }
        if (!android::base::WriteFully(output_fd, buffer.get(), bytes_to_read)) {
            std::cerr << "write failed: " << strerror(errno) << "\n";
            return false;
        }
        remaining_bytes -= bytes_to_read;
        position += bytes_to_read;
    }
    return true;
}

bool ImageExtractor::WriteSparseImage(int temp_fd, const std::string& partition_name) {
    off_t len = lseek(temp_fd, 0, SEEK_END);
    if (len < 0) {
        std::cerr << "lseek failed: " << strerror(errno) << "\n";
        return false;
    }
    if (lseek(temp_fd, 0, SEEK_SET) < 0) {
        std::cerr << "lseek failed: " << strerror(errno) << "\n";
        return false;
    }

    uint32_t block_size = metadata_->geometry.logical_block_size;
    if (len % block_size != 0) {
        std::cerr << "image size (" << len << ") is not a multiple of the block size "
                  << "(" << block_size << ")\n";
        return false;
    }

    SparsePtr file(sparse_file_new(block_size, len), sparse_file_destroy);
    if (!file) {
        std::cerr << "Could not allocate sparse file.\n";
        return false;
    }
    sparse_file_verbose(file.get());

    if (sparse_file_read(file.get(), temp_fd, false, false)) {
        std::cerr << "sparse_file_read failed.\n";
        return false;
    }

    std::string output_path = output_dir_ + "/" + partition_name + ".img";
    unique_fd output_fd(open(output_path.c_str(), O_RDWR | O_CLOEXEC | O_CREAT | O_TRUNC, 0644));
    if (output_fd < 0) {
        std::cerr << "open failed: " << output_path << ": " << strerror(errno) << "\n";
        return false;
    }
    if (sparse_file_write(file.get(), output_fd, false, true, false)) {
        std::cerr << "sparse_file_write failed.\n";
        return false;
    }
    return true;
}
