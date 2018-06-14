#ifndef BUILD_VERITY_TREE_H
#define BUILD_VERITY_TREE_H

#include <inttypes.h>

#include <string>
#include <vector>

uint64_t calculate_verity_tree_size(uint64_t calculate_size, size_t block_size);

bool generate_verity_tree(const std::string& data_filename,
                          const std::string& verity_filename,
                          const std::vector<unsigned char>& salt,
                          size_t block_size, bool sparse, bool verbose);

#endif