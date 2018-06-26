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

#ifndef __HASH_TREE_BUILDER_H__
#define __HASH_TREE_BUILDER_H__

#include <inttypes.h>

#include <string>
#include <vector>

#include <openssl/evp.h>

// This class builds a verity hash tree based on the input data and a salt with
// the length of hash size. It also supports the streaming of input data while
// the total data size should know in advance. Once all the data is ready,
// appropriate functions can be called to build the upper levels of the hash
// tree and output the tree to a file.
// TODO (xunchang) add support of various hash algorithms.
class HashTreeBuilder {
 public:
  explicit HashTreeBuilder(size_t block_size);
  // Returns the size of the verity tree in bytes given the input data size.
  uint64_t CalculateSize(uint64_t input_size) const;
  // Gets ready for the hash tree computation. We expect |expected_data_size|
  // bytes source data.
  bool Initialize(int64_t expected_data_size,
                  const std::vector<unsigned char>& salt);
  // Streams |len| bytes of source data to the hasher, and the |len| is expected
  // to be block aligned. This function can be called multiple until we
  // processed all the source data. And the accumulated data_size is expected to
  // be exactly the |data_size_| when we build the hash tree.
  bool Update(const unsigned char* data, size_t len);
  // Computes the upper levels of the hash tree based on the 0th level.
  bool BuildHashTree();
  // Writes the computed hash tree top-down to |output|.
  bool WriteHashTreeToFile(const std::string& output) const;
  bool WriteHashTreeToFd(int fd, size_t offset) const;

  size_t hash_size() const { return hash_size_; }
  const std::vector<unsigned char>& root_hash() const { return root_hash_; }

 private:
  friend class BuildVerityTreeTest;
  // Calculates the hash of one single block. Write the result to |out|, a
  // buffer allocated by the caller.
  bool HashBlock(const unsigned char* block, unsigned char* out);
  // Calculates the hash of |len| bytes of data starting from |data|. Append the
  // result to |output_vector|.
  bool HashBlocks(const unsigned char* data, size_t len,
                  std::vector<unsigned char>* output_vector);
  // Aligns |data| with block_size by padding 0s to the end.
  void AppendPaddings(std::vector<unsigned char>* data);

  size_t block_size_;
  // Expected size of the source data, which is used to compute the hash for the
  // base level.
  uint64_t data_size_;
  std::vector<unsigned char> salt_;
  const EVP_MD* md_;
  size_t hash_size_;

  // Pre-calculated hash of a zero block.
  std::vector<unsigned char> zero_block_hash_;
  std::vector<unsigned char> root_hash_;
  // Storage of the verity tree. The base level hash stores in verity_tree_[0]
  // and the top level hash stores in verity_tree_.back().
  std::vector<std::vector<unsigned char>> verity_tree_;
};

#endif  // __HASH_TREE_BUILDER_H__
