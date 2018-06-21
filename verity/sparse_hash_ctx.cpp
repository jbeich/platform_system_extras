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

#include "sparse_hash_ctx.h"

#include <string.h>

#include <limits>

#include <android-base/file.h>
#include <android-base/logging.h>

size_t verity_tree_blocks(uint64_t data_size, size_t block_size,
                          size_t hash_size, size_t level) {
  uint64_t level_blocks = div_round_up(data_size, block_size);
  uint64_t hashes_per_block = div_round_up(block_size, hash_size);

  do {
    level_blocks = div_round_up(level_blocks, hashes_per_block);
  } while (level--);

  CHECK_LE(level_blocks, std::numeric_limits<size_t>::max());
  return level_blocks;
}

sparseHashCtx::sparseHashCtx(size_t block_size)
    : block_size_(block_size), data_size_(0), md_(EVP_sha256()) {
  CHECK(md_ != nullptr) << "Failed to initialize md";

  hash_size_ = EVP_MD_size(md_);
  CHECK_LT(hash_size_ * 2, block_size_);
}

bool sparseHashCtx::initialize(int64_t expected_data_size,
                               const std::vector<unsigned char>& salt) {
  data_size_ = expected_data_size;
  salt_ = salt;

  if (data_size_ % block_size_ != 0) {
    LOG(ERROR) << "file size " << data_size_
               << " is not a multiple of block size" << block_size_;
    return false;
  }

  // Reserve enough space for the hash of the input data.
  size_t base_level_blocks =
      verity_tree_blocks(data_size_, block_size_, hash_size_, 0);
  std::vector<unsigned char> base_level;
  base_level.reserve(base_level_blocks * block_size_);
  verity_tree_.emplace_back(std::move(base_level));

  // Save the hash of the zero block to avoid future recalculation.
  unsigned char zero_block[block_size_];
  memset(zero_block, 0, block_size_);
  zero_block_hash_.resize(hash_size_);
  hash_block(zero_block, block_size_, zero_block_hash_.data());

  return true;
}

bool sparseHashCtx::hash_block(const unsigned char* block, size_t len,
                               unsigned char* out) {
  CHECK_EQ(block_size_, len);

  unsigned int s;
  int ret = 1;

  EVP_MD_CTX* mdctx = EVP_MD_CTX_create();
  CHECK(mdctx != nullptr);
  ret &= EVP_DigestInit_ex(mdctx, md_, nullptr);
  ret &= EVP_DigestUpdate(mdctx, salt_.data(), salt_.size());
  ret &= EVP_DigestUpdate(mdctx, block, len);
  ret &= EVP_DigestFinal_ex(mdctx, out, &s);
  EVP_MD_CTX_destroy(mdctx);

  CHECK_EQ(1, ret);
  CHECK_EQ(hash_size_, s);
  return true;
}

bool sparseHashCtx::hash_blocks(const unsigned char* data, size_t len,
                                std::vector<unsigned char>* output_vector) {
  if (len == 0) {
    return true;
  }
  CHECK_EQ(0, len % block_size_);

  if (data == nullptr) {
    for (size_t i = 0; i < len; i += block_size_) {
      output_vector->insert(output_vector->end(), zero_block_hash_.begin(),
                            zero_block_hash_.end());
    }
    return true;
  }

  for (size_t i = 0; i < len; i += block_size_) {
    unsigned char hash_buffer[hash_size_];
    if (!hash_block(data + i, block_size_, hash_buffer)) {
      return false;
    }
    output_vector->insert(output_vector->end(), hash_buffer,
                          hash_buffer + hash_size_);
  }

  return true;
}

bool sparseHashCtx::hash_input_data(const unsigned char* data, size_t len) {
  CHECK_GT(data_size_, 0);

  return hash_blocks(data, len, &verity_tree_[0]);
}

bool sparseHashCtx::build_hash_tree() {
  // Expects only the base level in the verity_tree_.
  CHECK_EQ(1, verity_tree_.size());

  // Expects the base level to have the same size as the total hash size of
  // input data.
  append_paddings(&verity_tree_.back());
  size_t base_level_blocks =
      verity_tree_blocks(data_size_, block_size_, hash_size_, 0);
  CHECK_EQ(base_level_blocks * block_size_, verity_tree_[0].size());

  while (verity_tree_.back().size() > block_size_) {
    const auto& current_level = verity_tree_.back();
    // Computes the next level of the verity tree based on the hash of the
    // current level.
    size_t next_level_blocks =
        verity_tree_blocks(current_level.size(), block_size_, hash_size_, 0);
    std::vector<unsigned char> next_level;
    next_level.reserve(next_level_blocks * block_size_);

    hash_blocks(current_level.data(), current_level.size(), &next_level);
    append_paddings(&next_level);

    // Checks the size of the next level.
    CHECK_EQ(next_level_blocks * block_size_, next_level.size());
    verity_tree_.emplace_back(std::move(next_level));
  }

  CHECK_EQ(block_size_, verity_tree_.back().size());
  hash_blocks(verity_tree_.back().data(), block_size_, &root_hash_);

  return true;
}

bool sparseHashCtx::write_hash_tree_to_fd(int fd) const {
  CHECK(!verity_tree_.empty());

  // Reads reversely to output the verity tree top-down.
  for (size_t i = verity_tree_.size(); i > 0; i--) {
    const auto& level_blocks = verity_tree_[i - 1];
    if (!android::base::WriteFully(fd, level_blocks.data(),
                                   level_blocks.size())) {
      PLOG(ERROR) << "Failed to write the hash tree level " << i;
      return false;
    }
  }

  return true;
}

void sparseHashCtx::append_paddings(std::vector<unsigned char>* data) {
  size_t remainder = data->size() % 4096;
  if (remainder != 0) {
    data->resize(data->size() + 4096 - remainder, 0);
  }
}
