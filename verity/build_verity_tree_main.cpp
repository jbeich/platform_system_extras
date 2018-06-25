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

#include <fcntl.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <vector>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/parseint.h>
#include <android-base/unique_fd.h>
#include <openssl/bn.h>

#include "build_verity_tree.h"
#include "build_verity_tree_utils.h"

void usage(void) {
  printf(
      "usage: build_verity_tree [ <options> ] -s <size> | <data> <verity>\n"
      "options:\n"
      "  -a,--salt-str=<string>       set salt to <string>\n"
      "  -A,--salt-hex=<hex digits>   set salt to <hex digits>\n"
      "  -h                           show this help\n"
      "  -s,--verity-size=<data size> print the size of the verity tree\n"
      "  -v,                          enable verbose logging\n"
      "  -S                           treat <data image> as a sparse file\n");
}

int main(int argc, char** argv) {
  constexpr size_t kBlockSize = 4096;

  std::vector<unsigned char> salt;
  bool sparse = false;
  uint64_t calculate_size = 0;
  bool verbose = false;

  while (1) {
    const static struct option long_options[] = {
        {"salt-str", required_argument, 0, 'a'},
        {"salt-hex", required_argument, 0, 'A'},
        {"help", no_argument, 0, 'h'},
        {"sparse", no_argument, 0, 'S'},
        {"verity-size", required_argument, 0, 's'},
        {"verbose", no_argument, 0, 'v'},
        {nullptr, 0, 0, 0}};
    int c = getopt_long(argc, argv, "a:A:hSs:v", long_options, nullptr);
    if (c < 0) {
      break;
    }

    switch (c) {
      case 'a':
        salt.clear();
        salt.insert(salt.end(), optarg, &optarg[strlen(optarg)]);
        break;
      case 'A': {
        BIGNUM* bn = nullptr;
        if (!BN_hex2bn(&bn, optarg)) {
          LOG(ERROR) << "Failed to convert salt from hex";
          return 1;
        }
        size_t salt_size = BN_num_bytes(bn);
        salt.resize(salt_size);
        if (BN_bn2bin(bn, salt.data()) != salt_size) {
          LOG(ERROR) << "Failed to convert salt to bytes";
          return 1;
        }
      } break;
      case 'h':
        usage();
        return 1;
      case 'S':
        sparse = true;
        break;
      case 's': {
        if (!android::base::ParseUint(optarg, &calculate_size,
                                      std::numeric_limits<uint64_t>::max())) {
          LOG(ERROR) << "Invalid input size: " << optarg;
          return 1;
        }

      } break;
      case 'v':
        verbose = true;
        break;
      case '?':
        usage();
        return 1;
      default:
        abort();
    }
  }

  argc -= optind;
  argv += optind;

  HashTreeBuilder builder(kBlockSize);

  if (calculate_size) {
    if (argc != 0) {
      usage();
      return 1;
    }

    uint64_t tree_size = builder.CalculateSize(calculate_size);
    printf("%" PRIu64 "\n", tree_size);
    return 0;
  }

  if (argc != 2) {
    usage();
    return 1;
  }

  if (salt.empty()) {
    salt.resize(builder.hash_size());

    android::base::unique_fd random_fd(open("/dev/urandom", O_RDONLY));
    if (random_fd < 0) {
      PLOG(ERROR) << "failed to open /dev/urandom";
      return 1;
    }

    ssize_t ret = read(random_fd, salt.data(), salt.size());
    if (ret != static_cast<ssize_t>(salt.size())) {
      PLOG(ERROR) << "failed to read " << salt.size()
                  << " bytes from /dev/urandom: " << ret;
      return 1;
    }
  }

  if (!generate_verity_tree(argv[0], argv[1], &builder, salt, kBlockSize,
                            sparse, verbose)) {
    return 1;
  }

  // Output the root hash and the salt.
  std::string root_hash_string =
      HashTreeBuilder::BytesArrayToString(builder.root_hash());
  std::string salt_string = HashTreeBuilder::BytesArrayToString(salt);
  printf("%s %s\n", root_hash_string.c_str(), salt_string.c_str());

  return 0;
}
