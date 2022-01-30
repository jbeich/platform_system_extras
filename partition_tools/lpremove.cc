// Copyright (C) 2019 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <getopt.h>
#include <sysexits.h>
#include <unistd.h>

#include <iostream>
#include <optional>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/unique_fd.h>
#include <liblp/builder.h>
#include <sparse/sparse.h>

using android::base::borrowed_fd;
using android::base::unique_fd;
using android::fs_mgr::LpMetadata;
using android::fs_mgr::MetadataBuilder;
using android::fs_mgr::ReadMetadata;
using android::fs_mgr::UpdatePartitionTable;
using SparsePtr = std::unique_ptr<sparse_file, decltype(&sparse_file_destroy)>;

std::optional<TemporaryDir> gTempDir;

static int usage(const char* program) {
    std::cerr << program << " - command-line tool for removing partitions from a super.img\n";
    std::cerr << "\n";
    std::cerr << "Usage:\n";
    std::cerr << " " << program << " [options] SUPER PARTNAME\n";
    std::cerr << "\n";
    std::cerr << "  SUPER                         Path to the super image. It can be sparsed or\n"
              << "                                unsparsed. If sparsed, it will be unsparsed\n"
              << "                                temporarily and re-sparsed over the original\n"
              << "                                file. This will consume extra space during the\n"
              << "                                execution of " << program << ".\n";
    std::cerr << "  PARTNAME                      Name of the partition to remove.\n";
    std::cerr << "\n";
    return EX_USAGE;
}

enum class OptionCode : int {
    kReadonly = 1,

    // Special options.
    kHelp = (int)'h',
};

static std::string GetTemporaryDir() {
    if (!gTempDir) {
        gTempDir.emplace();
        int saved_errno = errno;
        if (access(gTempDir->path, F_OK) != 0) {
            std::cerr << "Could not create temporary dir: " << gTempDir->path << ": "
                      << strerror(saved_errno) << std::endl;
            abort();
        }
    }
    return gTempDir->path;
}

class LocalSuperOpener final : public android::fs_mgr::PartitionOpener {
  public:
    LocalSuperOpener(const std::string& path, borrowed_fd fd)
        : local_super_(path), local_super_fd_(fd) {}

    unique_fd Open(const std::string& partition_name, int flags) const override {
        if (partition_name == local_super_) {
            return unique_fd{dup(local_super_fd_.get())};
        }
        return PartitionOpener::Open(partition_name, flags);
    }

  private:
    std::string local_super_;
    borrowed_fd local_super_fd_;
};

class SuperHelper final {
  public:
    explicit SuperHelper(const std::string& super_path) : super_path_(super_path) {}

    bool Open();
    bool RemovePartition(const std::string& partition_name);
    bool Finalize();

  private:
    bool OpenSuperFile();
    bool UpdateSuper();

    // Returns true if |fd| does not contain a sparsed file. If |fd| does
    // contain a sparsed file, |temp_file| will contain the unsparsed output.
    // If |fd| cannot be read or failed to unsparse, false is returned.
    bool MaybeUnsparse(const std::string& file, borrowed_fd fd,
                       std::optional<TemporaryFile>* temp_file, uint32_t* block_size = nullptr);

    std::string super_path_;
    std::string abs_super_path_;
    bool was_empty_ = false;
    // fd for the super file, sparsed or temporarily unsparsed.
    int super_fd_;
    // fd for the super file if unsparsed.
    unique_fd output_fd_;
    // If the super file is sparse, this holds the temp unsparsed file.
    std::optional<TemporaryFile> temp_super_;
    uint32_t sparse_block_size_ = 0;
    std::unique_ptr<LpMetadata> metadata_;
    std::unique_ptr<MetadataBuilder> builder_;
};

bool SuperHelper::Open() {
    if (!OpenSuperFile()) {
        return false;
    }

    was_empty_ = android::fs_mgr::IsEmptySuperImage(abs_super_path_);
    if (was_empty_) {
        metadata_ = android::fs_mgr::ReadFromImageFile(abs_super_path_);
    } else {
        metadata_ = android::fs_mgr::ReadMetadata(abs_super_path_, 0);
    }
    if (!metadata_) {
        std::cerr << "Could not read super partition metadata for " << super_path_ << "\n";
        return false;
    }
    builder_ = MetadataBuilder::New(*metadata_.get());
    if (!builder_) {
        std::cerr << "Could not create MetadataBuilder for " << super_path_ << "\n";
        return false;
    }
    return true;
}

bool SuperHelper::RemovePartition(const std::string& partition_name) {
    if (was_empty_) {
        std::cerr << "Cannot remove a partition image from an empty super file.\n";
        return false;
    }

    auto partition = builder_->FindPartition(partition_name);
    if (!partition) {
        std::cerr << "Could not find partition: " << partition_name << "\n";
        return false;
    }

    builder_->RemovePartition(partition_name);

    // Write the new metadata out. We do this by re-using the on-device flashing
    // logic, and using the local file instead of a block device.
    if (!UpdateSuper()) {
        return false;
    }

    return true;
}

bool SuperHelper::OpenSuperFile() {
    auto actual_path = super_path_;

    output_fd_.reset(open(actual_path.c_str(), O_RDWR | O_CLOEXEC));
    if (output_fd_ < 0) {
        std::cerr << "open failed: " << actual_path << ": " << strerror(errno) << "\n";
        return false;
    }
    super_fd_ = output_fd_.get();

    if (!MaybeUnsparse(super_path_, super_fd_, &temp_super_, &sparse_block_size_)) {
        return false;
    }
    if (temp_super_) {
        actual_path = temp_super_->path;
        super_fd_ = temp_super_->fd;
    }

    // PartitionOpener will decorate relative paths with /dev/block/by-name
    // so get an absolute path here.
    if (!android::base::Realpath(actual_path, &abs_super_path_)) {
        std::cerr << "realpath failed: " << actual_path << ": " << strerror(errno) << "\n";
        return false;
    }
    return true;
}

bool SuperHelper::MaybeUnsparse(const std::string& file, borrowed_fd fd,
                                std::optional<TemporaryFile>* temp_file,
                                uint32_t* block_size) {
    SparsePtr sf(sparse_file_import(fd.get(), false, false), sparse_file_destroy);
    if (!sf) {
        return true;
    }

    temp_file->emplace(GetTemporaryDir());
    if ((*temp_file)->fd < 0) {
        std::cerr << "mkstemp failed: " << strerror(errno) << "\n";
        return false;
    }

    std::cout << "Unsparsing " << file << "... " << std::endl;

    if (sparse_file_write(sf.get(), (*temp_file)->fd, false, false, false) != 0) {
        std::cerr << "Could not write unsparsed file.\n";
        return false;
    }
    if (block_size) {
        *block_size = sparse_file_block_size(sf.get());
    }
    return true;
}

bool SuperHelper::UpdateSuper() {
    metadata_ = builder_->Export();
    if (!metadata_) {
        std::cerr << "Failed to export new metadata.\n";
        return false;
    }

    // Empty images get written at the very end.
    if (was_empty_) {
        return true;
    }

    // Note: A/B devices have an extra metadata slot that is unused, so we cap
    // the writes to the first two slots.
    LocalSuperOpener opener(abs_super_path_, super_fd_);
    uint32_t slots = std::min(metadata_->geometry.metadata_slot_count, (uint32_t)2);
    for (uint32_t i = 0; i < slots; i++) {
        if (!UpdatePartitionTable(opener, abs_super_path_, *metadata_.get(), i)) {
            std::cerr << "Could not write new super partition metadata.\n";
            return false;
        }
    }
    return true;
}

static bool Truncate(borrowed_fd fd) {
    if (ftruncate(fd.get(), 0) < 0) {
        std::cerr << "truncate failed: " << strerror(errno) << "\n";
        return false;
    }
    if (lseek(fd.get(), 0, SEEK_SET) < 0) {
        std::cerr << "lseek failed: " << strerror(errno) << "\n";
        return false;
    }
    return true;
}

bool SuperHelper::Finalize() {
    if (was_empty_) {
        if (!Truncate(super_fd_)) {
            return false;
        }
        if (!android::fs_mgr::WriteToImageFile(super_fd_, *metadata_.get())) {
            std::cerr << "Could not write image file.\n";
            return false;
        }
    }

    // If the super image wasn't original sparsed, we don't have to do anything
    // else.
    if (!temp_super_) {
        return true;
    }

    // Otherwise, we have to sparse the temporary file. Find its length.
    auto len = lseek(super_fd_, 0, SEEK_END);
    if (len < 0 || lseek(super_fd_, 0, SEEK_SET < 0)) {
        std::cerr << "lseek failed: " << strerror(errno) << "\n";
        return false;
    }

    SparsePtr sf(sparse_file_new(sparse_block_size_, len), sparse_file_destroy);
    if (!sf) {
        std::cerr << "Could not allocate sparse file.\n";
        return false;
    }
    sparse_file_verbose(sf.get());

    std::cout << "Writing sparse super image... " << std::endl;
    if (sparse_file_read(sf.get(), super_fd_, SPARSE_READ_MODE_NORMAL, false) != 0) {
        std::cerr << "Could not import super partition for sparsing.\n";
        return false;
    }
    if (!Truncate(output_fd_)) {
        return false;
    }
    if (sparse_file_write(sf.get(), output_fd_, false, true, false)) {
        return false;
    }
    return true;
}

static void ErrorLogger(android::base::LogId, android::base::LogSeverity severity, const char*,
                        const char*, unsigned int, const char* msg) {
    if (severity < android::base::WARNING) {
        return;
    }
    std::cerr << msg << std::endl;
}

int main(int argc, char* argv[]) {
    int rv, index;
    while ((rv = getopt(argc, argv, "h")) != -1) {
        switch ((OptionCode)rv) {
            case OptionCode::kHelp:
                usage(argv[0]);
                return EX_OK;
            default:
                return usage(argv[0]);
        }
    }

    if (optind + 2 > argc) {
        std::cerr << "Missing required arguments.\n\n";
        return usage(argv[0]);
    }

    std::string super_path = argv[optind++];
    std::string partition_name = argv[optind++];

    if (optind != argc) {
        std::cerr << "Unexpected arguments.\n\n";
        return usage(argv[0]);
    }

    // Suppress log spam from liblp.
    android::base::SetLogger(ErrorLogger);

    SuperHelper super(super_path);
    if (!super.Open()) {
        return EX_SOFTWARE;
    }

    if (!super.RemovePartition(partition_name)) {
        return EX_SOFTWARE;
    }
    if (!super.Finalize()) {
        return EX_SOFTWARE;
    }

    std::cout << "Done.\n";
    return EX_OK;
}
