#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <chrono>
#include <vector>

#include <openssl/bn.h>

#include "build_verity_tree.h"

constexpr size_t kBlockSize = 4096;

#define FATAL(x...)     \
  {                     \
    fprintf(stderr, x); \
    exit(1);            \
  }

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
        {NULL, 0, 0, 0}};
    int c = getopt_long(argc, argv, "a:A:hSs:v", long_options, NULL);
    if (c < 0) {
      break;
    }

    switch (c) {
      case 'a':
        salt.clear();
        salt.insert(salt.end(), optarg, &optarg[strlen(optarg)]);
        break;
      case 'A': {
        BIGNUM* bn = NULL;
        if (!BN_hex2bn(&bn, optarg)) {
          FATAL("failed to convert salt from hex\n");
        }
        size_t salt_size = BN_num_bytes(bn);
        salt.resize(salt_size);
        if (BN_bn2bin(bn, salt.data()) != salt_size) {
          FATAL("failed to convert salt to bytes\n");
        }
      } break;
      case 'h':
        usage();
        return 1;
      case 'S':
        sparse = true;
        break;
      case 's': {
        char* endptr;
        errno = 0;
        unsigned long long int inSize = strtoull(optarg, &endptr, 0);
        if (optarg[0] == '\0' || *endptr != '\0' ||
            (errno == ERANGE && inSize == ULLONG_MAX)) {
          FATAL("invalid value of verity-size\n");
        }
        if (inSize > UINT64_MAX) {
          FATAL("invalid value of verity-size\n");
        }
        calculate_size = (uint64_t)inSize;
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

  if (calculate_size) {
    if (argc != 0) {
      usage();
      return 1;
    }

    size_t verity_size = calculate_verity_tree_size(calculate_size, kBlockSize);
    printf("%" PRIu64 "\n", verity_size);
    return 0;
  }

  if (argc != 2) {
    usage();
    return 1;
  }

  // auto start = std::chrono::steady_clock::now();
  if (!generate_verity_tree(argv[0], argv[1], salt, kBlockSize, sparse,
                            verbose)) {
    FATAL("Failed to generate verity tree\n");
  }

  // auto now = std::chrono::steady_clock::now();
  // double duration =
  //      std::chrono::duration_cast<std::chrono::duration<double>>(now -
  //      start).count();
  // fprintf(stderr, "duration: [%12.6lf]\n", duration);
}