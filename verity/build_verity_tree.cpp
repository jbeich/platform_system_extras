
#include "build_verity_tree.h"

#include <openssl/evp.h>
#include <sparse/sparse.h>

#undef NDEBUG

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <android-base/file.h>

struct sparse_hash_ctx {
    unsigned char *hashes;
    const unsigned char *salt;
    uint64_t salt_size;
    uint64_t hash_size;
    uint64_t block_size;
    const unsigned char *zero_block_hash;
    const EVP_MD *md;
};

#define div_round_up(x,y) (((x) + (y) - 1)/(y))

#define RETURN_FALSE(x...) \
  {                        \
    fprintf(stderr, x);    \
    return false;          \
  }

size_t verity_tree_blocks(uint64_t data_size, size_t block_size,
                          size_t hash_size, int level) {
  size_t level_blocks = div_round_up(data_size, block_size);
  int hashes_per_block = div_round_up(block_size, hash_size);

  do {
    level_blocks = div_round_up(level_blocks, hashes_per_block);
  } while (level--);

  return level_blocks;
}

int hash_block(const EVP_MD *md,
               const unsigned char *block, size_t len,
               const unsigned char *salt, size_t salt_len,
               unsigned char *out, size_t *out_size)
{
    EVP_MD_CTX *mdctx;
    unsigned int s;
    int ret = 1;

    mdctx = EVP_MD_CTX_create();
    assert(mdctx);
    ret &= EVP_DigestInit_ex(mdctx, md, NULL);
    ret &= EVP_DigestUpdate(mdctx, salt, salt_len);
    ret &= EVP_DigestUpdate(mdctx, block, len);
    ret &= EVP_DigestFinal_ex(mdctx, out, &s);
    EVP_MD_CTX_destroy(mdctx);
    assert(ret == 1);
    if (out_size) {
        *out_size = s;
    }
    return 0;
}

int hash_blocks(const EVP_MD *md,
                const unsigned char *in, size_t in_size,
                unsigned char *out, size_t *out_size,
                const unsigned char *salt, size_t salt_size,
                size_t block_size)
{
    size_t s;
    *out_size = 0;
    for (size_t i = 0; i < in_size; i += block_size) {
        hash_block(md, in + i, block_size, salt, salt_size, out, &s);
        out += s;
        *out_size += s;
    }

    return 0;
}

int hash_chunk(void *priv, const void *data, size_t len)
{
    struct sparse_hash_ctx *ctx = (struct sparse_hash_ctx *)priv;
    assert(len % ctx->block_size == 0);
    if (data) {
        size_t s;
        hash_blocks(ctx->md, (const unsigned char *)data, len,
                    ctx->hashes, &s,
                    ctx->salt, ctx->salt_size, ctx->block_size);
        ctx->hashes += s;
    } else {
        for (size_t i = 0; i < len; i += ctx->block_size) {
            memcpy(ctx->hashes, ctx->zero_block_hash, ctx->hash_size);
            ctx->hashes += ctx->hash_size;
        }
    }
    return 0;
}

uint64_t calculate_verity_tree_size(uint64_t calculate_size,
                                    size_t block_size) {
  const size_t kSHA256HashSize = 32;

  size_t verity_blocks = 0;
  size_t level_blocks;
  int levels = 0;
  do {
    level_blocks =
        verity_tree_blocks(calculate_size, block_size, kSHA256HashSize, levels);
    levels++;
    verity_blocks += level_blocks;
  } while (level_blocks > 1);

  return static_cast<uint64_t>(verity_blocks) * block_size;
}

bool generate_verity_tree(const std::string &data_filename,
                          const std::string &verity_filename,
                          const std::vector<unsigned char> &salt_content,
                          size_t block_size, bool sparse, bool verbose) {
  const EVP_MD *md = EVP_sha256();
  if (!md) {
    RETURN_FALSE("failed to get digest\n");
  }

  size_t hash_size = EVP_MD_size(md);
  assert(hash_size * 2 < block_size);

  std::vector<unsigned char> salt = salt_content;
  if (salt.empty()) {
    salt.resize(hash_size);

    int random_fd = open("/dev/urandom", O_RDONLY);
    if (random_fd < 0) {
      RETURN_FALSE("failed to open /dev/urandom\n");
    }

    ssize_t ret = read(random_fd, salt.data(), salt.size());
    if (ret != static_cast<ssize_t>(salt.size())) {
      RETURN_FALSE("failed to read %zu bytes from /dev/urandom: %zd %d\n",
                   salt.size(), ret, errno);
    }
    close(random_fd);
  }

  int fd = open(data_filename.c_str(), O_RDONLY);
  if (fd < 0) {
    RETURN_FALSE("failed to open %s\n", data_filename.c_str());
  }

  struct sparse_file *file;
  if (sparse) {
    file = sparse_file_import(fd, false, false);
  } else {
    file = sparse_file_import_auto(fd, false, verbose);
  }

  if (!file) {
    RETURN_FALSE("failed to read file %s\n", data_filename.c_str());
  }

  int64_t len = sparse_file_len(file, false, false);
  if (len % block_size != 0) {
    RETURN_FALSE("file size %" PRIu64 " is not a multiple of %zu bytes\n", len,
                 block_size);
  }

  int levels = 0;
  size_t verity_blocks = 0;
  size_t level_blocks;

  do {
    level_blocks = verity_tree_blocks(len, block_size, hash_size, levels);
    levels++;
    verity_blocks += level_blocks;
  } while (level_blocks > 1);

  unsigned char *verity_tree = new unsigned char[verity_blocks * block_size]();
  unsigned char **verity_tree_levels = new unsigned char *[levels + 1]();
  size_t *verity_tree_level_blocks = new size_t[levels]();
  if (verity_tree == NULL || verity_tree_levels == NULL ||
      verity_tree_level_blocks == NULL) {
    RETURN_FALSE("failed to allocate memory for verity tree\n");
  }

  unsigned char *ptr = verity_tree;
  for (int i = levels - 1; i >= 0; i--) {
    verity_tree_levels[i] = ptr;
    verity_tree_level_blocks[i] =
        verity_tree_blocks(len, block_size, hash_size, i);
    ptr += verity_tree_level_blocks[i] * block_size;
  }
  assert(ptr == verity_tree + verity_blocks * block_size);
  assert(verity_tree_level_blocks[levels - 1] == 1);

  unsigned char zero_block_hash[hash_size];
  unsigned char zero_block[block_size];
  memset(zero_block, 0, block_size);
  hash_block(md, zero_block, block_size, salt.data(), salt.size(),
             zero_block_hash, NULL);

  unsigned char root_hash[hash_size];
  verity_tree_levels[levels] = root_hash;

  struct sparse_hash_ctx ctx;
  ctx.hashes = verity_tree_levels[0];
  ctx.salt = salt.data();
  ctx.salt_size = salt.size();
  ctx.hash_size = hash_size;
  ctx.block_size = block_size;
  ctx.zero_block_hash = zero_block_hash;
  ctx.md = md;

  sparse_file_callback(file, false, false, hash_chunk, &ctx);

  sparse_file_destroy(file);
  close(fd);

  for (int i = 0; i < levels; i++) {
    size_t out_size;
    hash_blocks(md, verity_tree_levels[i],
                verity_tree_level_blocks[i] * block_size,
                verity_tree_levels[i + 1], &out_size, salt.data(), salt.size(),
                block_size);
    if (i < levels - 1) {
      assert(div_round_up(out_size, block_size) ==
             verity_tree_level_blocks[i + 1]);
    } else {
      assert(out_size == hash_size);
    }
  }

  for (size_t i = 0; i < hash_size; i++) {
    printf("%02x", root_hash[i]);
  }
  printf(" ");
  for (size_t i = 0; i < salt.size(); i++) {
    printf("%02x", salt[i]);
  }
  printf("\n");

  fd = open(verity_filename.c_str(), O_WRONLY | O_CREAT, 0666);
  if (fd < 0) {
    RETURN_FALSE("failed to open output file '%s'\n", verity_filename.c_str());
  }
  if (!android::base::WriteFully(fd, verity_tree, verity_blocks * block_size)) {
    RETURN_FALSE("failed to write '%s'\n", verity_filename.c_str());
  }
  close(fd);

  delete[] verity_tree_levels;
  delete[] verity_tree_level_blocks;
  delete[] verity_tree;

  return true;
}
