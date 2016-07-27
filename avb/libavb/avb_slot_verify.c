/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include "avb_slot_verify.h"
#include "avb_chain_partition_descriptor.h"
#include "avb_footer.h"
#include "avb_hash_descriptor.h"
#include "avb_kernel_cmdline_descriptor.h"
#include "avb_sha.h"
#include "avb_util.h"
#include "avb_vbmeta_image.h"

// Maximum length (in bytes) of a partition name, including ab_suffix.
#define PART_NAME_MAX_SIZE 32

// Maximum size of a vbmeta image - 64 KiB.
#define VBMETA_MAX_SIZE (64 * 1024)

static AvbSlotVerifyResult load_and_verify_hash_partition(
    AvbOps* ops, const char* ab_suffix, const AvbDescriptor* descriptor,
    AvbSlotVerifyData* slot_data) {
  AvbHashDescriptor hash_desc;
  const uint8_t* desc_partition_name;
  const uint8_t* desc_salt;
  const uint8_t* desc_digest;
  char part_name[PART_NAME_MAX_SIZE];
  AvbSlotVerifyResult ret;
  AvbIOResult io_ret;
  uint8_t* image_buf = NULL;
  size_t part_num_read;
  uint8_t* digest;
  size_t digest_len;

  if (!avb_hash_descriptor_validate_and_byteswap(
          (const AvbHashDescriptor*)descriptor, &hash_desc)) {
    ret = AVB_SLOT_VERIFY_RESULT_ERROR_INVALID_METADATA;
    goto out;
  }

  desc_partition_name =
      ((const uint8_t*)descriptor) + sizeof(AvbHashDescriptor);
  desc_salt = desc_partition_name + hash_desc.partition_name_len;
  desc_digest = desc_salt + hash_desc.salt_len;

  if (!avb_validate_utf8(desc_partition_name, hash_desc.partition_name_len)) {
    avb_warning("Partition name is not valid UTF-8.\n");
    ret = AVB_SLOT_VERIFY_RESULT_ERROR_INVALID_METADATA;
    goto out;
  }

  if (!avb_str_concat(
          part_name, sizeof part_name, (const char*)desc_partition_name,
          hash_desc.partition_name_len, ab_suffix, avb_strlen(ab_suffix))) {
    avb_warning(
        "Partition name '%s' and suffix '%s' does not fit in %zd bytes.\n",
        (const char*)desc_partition_name, ab_suffix, sizeof part_name);
    ret = AVB_SLOT_VERIFY_RESULT_ERROR_INVALID_METADATA;
    goto out;
  }

  image_buf = avb_malloc(hash_desc.image_size);
  if (image_buf == NULL) {
    ret = AVB_SLOT_VERIFY_RESULT_ERROR_OOM;
    goto out;
  }

  io_ret = ops->read_from_partition(ops, part_name, image_buf, 0,
                                    hash_desc.image_size, &part_num_read);
  if (io_ret != AVB_IO_RESULT_OK) {
    avb_warning("Error loading data from '%s': %d\n", part_name, io_ret);
    ret = AVB_SLOT_VERIFY_RESULT_ERROR_IO;
    goto out;
  }
  if (part_num_read != hash_desc.image_size) {
    avb_warning("Requested %zd bytes but only read %zd bytes\n",
                hash_desc.image_size, part_num_read);
    ret = AVB_SLOT_VERIFY_RESULT_ERROR_IO;
    goto out;
  }
  avb_debug("Read %zd bytes\n", part_num_read);

  if (avb_strcmp((const char*)hash_desc.hash_algorithm, "sha256") == 0) {
    AvbSHA256Ctx sha256_ctx;
    avb_sha256_init(&sha256_ctx);
    avb_sha256_update(&sha256_ctx, desc_salt, hash_desc.salt_len);
    avb_sha256_update(&sha256_ctx, image_buf, hash_desc.image_size);
    digest = avb_sha256_final(&sha256_ctx);
    digest_len = AVB_SHA256_DIGEST_SIZE;
  } else if (avb_strcmp((const char*)hash_desc.hash_algorithm, "sha512") == 0) {
    AvbSHA512Ctx sha512_ctx;
    avb_sha512_init(&sha512_ctx);
    avb_sha512_update(&sha512_ctx, desc_salt, hash_desc.salt_len);
    avb_sha512_update(&sha512_ctx, image_buf, hash_desc.image_size);
    digest = avb_sha512_final(&sha512_ctx);
    digest_len = AVB_SHA512_DIGEST_SIZE;
  } else {
    avb_warning("Unsupported hash algorithm '%s'.\n", hash_desc.hash_algorithm);
    ret = AVB_SLOT_VERIFY_RESULT_ERROR_INVALID_METADATA;
    goto out;
  }

  if (digest_len != hash_desc.digest_len) {
    avb_warning("Digest in descriptor is %d bytes but expected %zd bytes.\n",
                hash_desc.digest_len, digest_len);
    ret = AVB_SLOT_VERIFY_RESULT_ERROR_INVALID_METADATA;
    goto out;
  }

  if (avb_safe_memcmp(digest, desc_digest, digest_len) != 0) {
    avb_warning("Hash of data does not match digest in descriptor.\n");
    ret = AVB_SLOT_VERIFY_RESULT_ERROR_VERIFICATION;
    goto out;
  }

  ret = AVB_SLOT_VERIFY_RESULT_OK;

  /* If this is the boot partition, copy to slot_data. */
  if (hash_desc.partition_name_len == 4 &&
      avb_memcmp(desc_partition_name, "boot", 4) == 0) {
    if (slot_data->boot_data != NULL) {
      avb_free(slot_data->boot_data);
    }
    slot_data->boot_size = hash_desc.image_size;
    slot_data->boot_data = image_buf;
    image_buf = NULL;
  }

out:
  if (image_buf != NULL) {
    avb_free(image_buf);
  }
  return ret;
}

static AvbSlotVerifyResult load_and_verify_vbmeta(
    AvbOps* ops, const char* ab_suffix, int rollback_index_slot,
    const char* partition_name, size_t partition_name_len,
    const uint8_t* expected_public_key, size_t expected_public_key_length,
    AvbSlotVerifyData* slot_data) {
  char full_partition_name[256];
  AvbSlotVerifyResult ret;
  AvbIOResult io_ret;
  size_t vbmeta_offset;
  size_t vbmeta_size;
  uint8_t* vbmeta_buf = NULL;
  size_t vbmeta_num_read;
  AvbVBMetaVerifyResult vbmeta_ret;
  const uint8_t* pk_data;
  size_t pk_len;
  AvbVBMetaImageHeader vbmeta_header;
  uint64_t stored_rollback_index;
  const AvbDescriptor** descriptors = NULL;
  size_t num_descriptors;
  size_t n;
  int is_main_vbmeta;

  avb_assert(slot_data != NULL);

  is_main_vbmeta = (avb_strcmp(partition_name, "vbmeta") == 0);

  if (!avb_validate_utf8((const uint8_t*)partition_name, partition_name_len)) {
    avb_warning("Partition name is not valid UTF-8.\n");
    ret = AVB_SLOT_VERIFY_RESULT_ERROR_INVALID_METADATA;
    goto out;
  }

  // Construct full partition name.
  if (!avb_str_concat(full_partition_name, sizeof full_partition_name,
                      partition_name, partition_name_len, ab_suffix,
                      avb_strlen(ab_suffix))) {
    avb_warning(
        "Partition name '%s' and suffix '%s' does not fit in %zd bytes.\n",
        partition_name, ab_suffix, sizeof full_partition_name);
    ret = AVB_SLOT_VERIFY_RESULT_ERROR_INVALID_METADATA;
    goto out;
  }

  // If we're loading from the main vbmeta partition, the vbmeta
  // struct is in the beginning. Otherwise we have to locate it via a
  // footer.
  if (is_main_vbmeta) {
    vbmeta_offset = 0;
    vbmeta_size = VBMETA_MAX_SIZE;
  } else {
    uint8_t footer_buf[AVB_FOOTER_SIZE];
    size_t footer_num_read;
    AvbFooter footer;

    io_ret = ops->read_from_partition(ops, full_partition_name, footer_buf,
                                      -AVB_FOOTER_SIZE, AVB_FOOTER_SIZE,
                                      &footer_num_read);
    if (io_ret != AVB_IO_RESULT_OK) {
      avb_warning("%s: Error loading footer: %d\n", full_partition_name,
                  io_ret);
      ret = AVB_SLOT_VERIFY_RESULT_ERROR_IO;
      goto out;
    }
    avb_assert(footer_num_read == AVB_FOOTER_SIZE);

    if (!avb_footer_validate_and_byteswap((const AvbFooter*)footer_buf,
                                          &footer)) {
      avb_warning("%s: Error validating footer.\n", full_partition_name);
      ret = AVB_SLOT_VERIFY_RESULT_ERROR_INVALID_METADATA;
      goto out;
    }

    // Basic footer sanity check since the data is untrusted.
    if (footer.vbmeta_size > VBMETA_MAX_SIZE) {
      avb_warning("%s: Footer size of %" PRIu64 " is invalid.\n",
                  full_partition_name, footer.vbmeta_size);
      ret = AVB_SLOT_VERIFY_RESULT_ERROR_INVALID_METADATA;
      goto out;
    }

    vbmeta_offset = footer.vbmeta_offset;
    vbmeta_size = footer.vbmeta_size;
  }

  vbmeta_buf = avb_malloc(vbmeta_size);
  if (vbmeta_buf == NULL) {
    ret = AVB_SLOT_VERIFY_RESULT_ERROR_OOM;
    goto out;
  }

  avb_debug("%s: Loading %zd bytes of vbmeta from offset %zd\n",
            full_partition_name, vbmeta_size, vbmeta_offset);

  io_ret =
      ops->read_from_partition(ops, full_partition_name, vbmeta_buf,
                               vbmeta_offset, vbmeta_size, &vbmeta_num_read);
  if (io_ret != AVB_IO_RESULT_OK) {
    avb_warning("%s: Error loading %zd bytes from offset %zd: %d\n",
                full_partition_name, vbmeta_offset, vbmeta_size, io_ret);
    ret = AVB_SLOT_VERIFY_RESULT_ERROR_IO;
    goto out;
  }
  avb_assert(vbmeta_num_read <= vbmeta_size);

  // Check if the image is properly signed and get the public key used
  // to sign the image.
  vbmeta_ret =
      avb_vbmeta_image_verify(vbmeta_buf, vbmeta_num_read, &pk_data, &pk_len);
  if (vbmeta_ret != AVB_VBMETA_VERIFY_RESULT_OK) {
    avb_warning(
        "%s: Error verifying vbmeta image at offset %zd and length %zd: %d\n",
        full_partition_name, vbmeta_offset, vbmeta_num_read, vbmeta_ret);
    ret = AVB_SLOT_VERIFY_RESULT_ERROR_VERIFICATION;
    goto out;
  }

  // Check if key used to make signature matches what is expected.
  if (expected_public_key != NULL) {
    if (expected_public_key_length != pk_len ||
        avb_safe_memcmp(expected_public_key, pk_data, pk_len) != 0) {
      avb_warning(
          "%s: Public key used to sign data does not match key in chain "
          "partition descriptor.\n",
          full_partition_name);
      ret = AVB_SLOT_VERIFY_RESULT_ERROR_PUBLIC_KEY_REJECTED;
      goto out;
    }
  } else {
    if (!ops->validate_public_key(ops, pk_data, pk_len)) {
      avb_warning("%s: Public key used to sign data rejected.\n",
                  full_partition_name);
      ret = AVB_SLOT_VERIFY_RESULT_ERROR_PUBLIC_KEY_REJECTED;
      goto out;
    }
  }

  avb_vbmeta_image_header_to_host_byte_order((AvbVBMetaImageHeader*)vbmeta_buf,
                                             &vbmeta_header);

  // Check rollback index.
  if (!ops->read_rollback_index(ops, rollback_index_slot,
                                &stored_rollback_index)) {
    avb_warning("%s: Error getting rollback index for slot %d.\n",
                full_partition_name, rollback_index_slot);
    ret = AVB_SLOT_VERIFY_RESULT_ERROR_IO;
    goto out;
  }
  if (vbmeta_header.rollback_index < stored_rollback_index) {
    avb_warning("%s: Image rollback index %" PRIu64
                " is less than the stored "
                "rollback index %" PRIu64 ".\n",
                full_partition_name, vbmeta_header.rollback_index,
                stored_rollback_index);
    ret = AVB_SLOT_VERIFY_RESULT_ERROR_ROLLBACK_INDEX;
    goto out;
  }

  /* Now go through all descriptors and take the appropriate action:
   *
   * - hash descriptor: Load data from partition, calculate hash, and
   *   checks that it matches what's in the hash descriptor.
   *
   * - hashtree descriptor: Do nothing since verification happens
   *   on-the-fly from within the OS.
   *
   * - chained partition descriptor: Load the footer, load the vbmeta
   *   image, verify vbmeta image (includes rollback checks, hash
   *   checks, bail on chained partitions).
   */
  descriptors =
      avb_descriptor_get_all(vbmeta_buf, vbmeta_num_read, &num_descriptors);
  for (n = 0; n < num_descriptors; n++) {
    AvbDescriptor desc;

    if (!avb_descriptor_validate_and_byteswap(descriptors[n], &desc)) {
      avb_warning("%s: Descriptor %zd is invalid.\n", full_partition_name, n);
      ret = AVB_SLOT_VERIFY_RESULT_ERROR_INVALID_METADATA;
      goto out;
    }

    switch (desc.tag) {
      case AVB_DESCRIPTOR_TAG_HASH: {
        AvbSlotVerifyResult sub_ret;
        sub_ret = load_and_verify_hash_partition(ops, ab_suffix, descriptors[n],
                                                 slot_data);
        if (sub_ret != AVB_SLOT_VERIFY_RESULT_OK) {
          ret = sub_ret;
          goto out;
        }
      } break;

      case AVB_DESCRIPTOR_TAG_CHAIN_PARTITION: {
        AvbSlotVerifyResult sub_ret;
        AvbChainPartitionDescriptor chain_desc;
        const uint8_t* chain_partition_name;
        const uint8_t* chain_public_key;

        // Only allow CHAIN_PARTITION descriptors in the main vbmeta image.
        if (!is_main_vbmeta) {
          avb_warning(
              "%s: Descriptor %zd is a chain partition descriptor and only "
              "allowed in the main image.\n",
              full_partition_name, n);
          ret = AVB_SLOT_VERIFY_RESULT_ERROR_INVALID_METADATA;
          goto out;
        }

        if (!avb_chain_partition_descriptor_validate_and_byteswap(
                (AvbChainPartitionDescriptor*)descriptors[n], &chain_desc)) {
          avb_warning("%s: Chain partition descriptor %zd is invalid.\n",
                      full_partition_name, n);
          ret = AVB_SLOT_VERIFY_RESULT_ERROR_INVALID_METADATA;
          goto out;
        }

        chain_partition_name = ((const uint8_t*)descriptors[n]) +
                               sizeof(AvbChainPartitionDescriptor);
        chain_public_key = chain_partition_name + chain_desc.partition_name_len;

        sub_ret = load_and_verify_vbmeta(
            ops, ab_suffix, chain_desc.rollback_index_slot,
            (const char*)chain_partition_name, chain_desc.partition_name_len,
            chain_public_key, chain_desc.public_key_len, slot_data);
        if (sub_ret != AVB_SLOT_VERIFY_RESULT_OK) {
          ret = sub_ret;
          goto out;
        }
      } break;

      case AVB_DESCRIPTOR_TAG_KERNEL_CMDLINE: {
        const uint8_t* kernel_cmdline;
        AvbKernelCmdlineDescriptor kernel_cmdline_desc;

        if (!avb_kernel_cmdline_descriptor_validate_and_byteswap(
                (AvbKernelCmdlineDescriptor*)descriptors[n],
                &kernel_cmdline_desc)) {
          avb_warning("%s: Kernel cmdline descriptor %zd is invalid.\n",
                      full_partition_name, n);
          ret = AVB_SLOT_VERIFY_RESULT_ERROR_INVALID_METADATA;
          goto out;
        }

        kernel_cmdline = ((const uint8_t*)descriptors[n]) +
                         sizeof(AvbKernelCmdlineDescriptor);

        if (!avb_validate_utf8(kernel_cmdline,
                               kernel_cmdline_desc.kernel_cmdline_length)) {
          avb_warning("Kernel cmdline is not valid UTF-8.\n");
          ret = AVB_SLOT_VERIFY_RESULT_ERROR_INVALID_METADATA;
          goto out;
        }

        if (slot_data->cmdline == NULL) {
          slot_data->cmdline =
              avb_calloc(kernel_cmdline_desc.kernel_cmdline_length + 1);
          if (slot_data->cmdline == NULL) {
            ret = AVB_SLOT_VERIFY_RESULT_ERROR_OOM;
            goto out;
          }
          avb_memcpy(slot_data->cmdline, kernel_cmdline,
                     kernel_cmdline_desc.kernel_cmdline_length);
        } else {
          // new cmdline is: <existing_cmdline> + ' ' + <newcmdline> + '\0'
          size_t orig_size = avb_strlen(slot_data->cmdline);
          size_t new_size =
              orig_size + 1 + kernel_cmdline_desc.kernel_cmdline_length + 1;
          char* new_cmdline = avb_calloc(new_size);
          if (new_cmdline == NULL) {
            ret = AVB_SLOT_VERIFY_RESULT_ERROR_OOM;
            goto out;
          }
          avb_memcpy(new_cmdline, slot_data->cmdline, orig_size);
          new_cmdline[orig_size] = ' ';
          avb_memcpy(new_cmdline + orig_size + 1, kernel_cmdline,
                     kernel_cmdline_desc.kernel_cmdline_length);
          avb_free(slot_data->cmdline);
          slot_data->cmdline = new_cmdline;
        }
      } break;

      /* Explicit fall-through */
      case AVB_DESCRIPTOR_TAG_PROPERTY:
      case AVB_DESCRIPTOR_TAG_HASHTREE:
        /* Do nothing. */
        break;
    }
  }

  ret = AVB_SLOT_VERIFY_RESULT_OK;

  // So far, so good. Copy needed data to user, if requested.
  if (is_main_vbmeta) {
    if (slot_data->vbmeta_data != NULL) {
      avb_free(slot_data->vbmeta_data);
    }
    // Note that |vbmeta_buf| is actually |vbmeta_num_read| bytes long
    // and this includes data past the end of the image. Pass the
    // actual size of the vbmeta image. Also, no need to use
    // avb_safe_add() since the header has already been verified.
    slot_data->vbmeta_size = sizeof(AvbVBMetaImageHeader) +
                             vbmeta_header.authentication_data_block_size +
                             vbmeta_header.auxiliary_data_block_size;
    slot_data->vbmeta_data = vbmeta_buf;
    vbmeta_buf = NULL;
  }

  if (rollback_index_slot >= AVB_MAX_NUMBER_OF_ROLLBACK_INDEX_SLOTS) {
    avb_warning("%s: Invalid rollback_index_slot value %d.\n",
                full_partition_name, rollback_index_slot);
    ret = AVB_SLOT_VERIFY_RESULT_ERROR_INVALID_METADATA;
    goto out;
  }

  slot_data->rollback_indexes[rollback_index_slot] =
      vbmeta_header.rollback_index;

out:
  if (vbmeta_buf != NULL) {
    avb_free(vbmeta_buf);
  }
  if (descriptors != NULL) {
    avb_free(descriptors);
  }
  return ret;
}

/* Substitutes all variables (e.g. $(ANDROID_SYSTEM_PARTUUID)) with
 * values. Returns NULL on OOM, otherwise the cmdline with values
 * replaced.
 */
static char* sub_cmdline(AvbOps* ops, const char* cmdline,
                         const char* ab_suffix) {
  const int NUM_GUIDS = 2;
  const char* part_name_str[NUM_GUIDS] = {"system", "boot"};
  const char* replace_str[NUM_GUIDS] = {"$(ANDROID_SYSTEM_PARTUUID)",
                                        "$(ANDROID_BOOT_PARTUUID)"};
  char* ret = NULL;

  /* Replace unique partition GUIDs */
  for (size_t n = 0; n < NUM_GUIDS; n++) {
    char part_name[PART_NAME_MAX_SIZE];
    char guid_buf[37];
    char* new_ret;

    if (!avb_str_concat(part_name, sizeof part_name, part_name_str[n],
                        avb_strlen(part_name_str[n]), ab_suffix,
                        avb_strlen(ab_suffix))) {
      avb_warning(
          "Partition name '%s' and suffix '%s' does not fit in %zd bytes.\n",
          (const char*)part_name_str[n], ab_suffix, sizeof part_name);
      goto fail;
    }

    if (!ops->get_unique_guid_for_partition(ops, part_name, guid_buf,
                                            sizeof guid_buf)) {
      avb_warning("Error getting unique GUID for partition '%s'.\n", part_name);
      goto fail;
    }

    if (ret == NULL) {
      new_ret = avb_replace(cmdline, replace_str[n], guid_buf);
    } else {
      new_ret = avb_replace(ret, replace_str[n], guid_buf);
    }
    if (new_ret == NULL) {
      goto fail;
    }
    ret = new_ret;
  }

  return ret;

fail:
  if (ret != NULL) {
    avb_free(ret);
  }
  return NULL;
}

static int cmdline_append_option(AvbSlotVerifyData* slot_data, const char* key,
                                 const char* value) {
  size_t offset, key_len, value_len;
  char* new_cmdline;

  key_len = avb_strlen(key);
  value_len = avb_strlen(value);

  offset = 0;
  if (slot_data->cmdline != NULL) {
    offset = avb_strlen(slot_data->cmdline);
    if (offset > 0) {
      offset += 1;
    }
  }

  new_cmdline = avb_calloc(offset + key_len + value_len + 2);
  if (new_cmdline == NULL) {
    return 0;
  }
  if (offset > 0) {
    avb_memcpy(new_cmdline, slot_data->cmdline, offset - 1);
    new_cmdline[offset - 1] = ' ';
  }
  avb_memcpy(new_cmdline + offset, key, key_len);
  new_cmdline[offset + key_len] = '=';
  avb_memcpy(new_cmdline + offset + key_len + 1, value, value_len);
  if (slot_data->cmdline != NULL) {
    avb_free(slot_data->cmdline);
  }
  slot_data->cmdline = new_cmdline;

  return 1;
}

static int cmdline_append_uint64_base10(AvbSlotVerifyData* slot_data,
                                        const char* key, uint64_t value) {
  const int MAX_DIGITS = 32;
  char rev_digits[MAX_DIGITS];
  char digits[MAX_DIGITS];
  size_t n, num_digits;

  for (num_digits = 0; num_digits < MAX_DIGITS - 1;) {
    rev_digits[num_digits++] = (value % 10) + '0';
    value /= 10;
    if (value == 0) {
      break;
    }
  }

  for (n = 0; n < num_digits; n++) {
    digits[n] = rev_digits[num_digits - 1 - n];
  }
  digits[n] = '\0';

  return cmdline_append_option(slot_data, key, digits);
}

static int cmdline_append_hex(AvbSlotVerifyData* slot_data, const char* key,
                              const uint8_t* data, size_t data_len) {
  char hex_digits[17] = "0123456789abcdef";
  char* hex_data;
  int ret;
  size_t n;

  hex_data = avb_malloc(data_len * 2 + 1);
  if (hex_data == NULL) return 0;

  for (n = 0; n < data_len; n++) {
    hex_data[n * 2] = hex_digits[data[n] >> 4];
    hex_data[n * 2 + 1] = hex_digits[data[n] & 0x0f];
  }
  hex_data[n] = '\0';

  ret = cmdline_append_option(slot_data, key, hex_data);
  avb_free(hex_data);
  return ret;
}

AvbSlotVerifyResult avb_slot_verify(AvbOps* ops, const char* ab_suffix,
                                    AvbSlotVerifyData** out_data) {
  AvbSlotVerifyResult ret;
  AvbSlotVerifyData* slot_data = NULL;

  if (out_data != NULL) {
    *out_data = NULL;
  }

  slot_data = avb_calloc(sizeof(AvbSlotVerifyData));
  if (slot_data == NULL) {
    ret = AVB_SLOT_VERIFY_RESULT_ERROR_OOM;
    goto fail;
  }

  ret = load_and_verify_vbmeta(ops, ab_suffix, 0, "vbmeta",
                               avb_strlen("vbmeta"), NULL, 0, slot_data);

  /* If things check out, mangle the kernel command-line as needed. */
  if (ret == AVB_SLOT_VERIFY_RESULT_OK) {
    /* Substitute $(ANDROID_SYSTEM_PARTUUID) and friends. */
    if (slot_data->cmdline != NULL) {
      char* new_cmdline = sub_cmdline(ops, slot_data->cmdline, ab_suffix);
      if (new_cmdline == NULL) {
        ret = AVB_SLOT_VERIFY_RESULT_ERROR_OOM;
        goto fail;
      }
      avb_free(slot_data->cmdline);
      slot_data->cmdline = new_cmdline;
    }

    /* Add androidboot.slot_suffix, if applicable. */
    if (avb_strlen(ab_suffix) > 0) {
      if (!cmdline_append_option(slot_data, "androidboot.slot_suffix",
                                 ab_suffix)) {
        ret = AVB_SLOT_VERIFY_RESULT_ERROR_OOM;
        goto fail;
      }
    }

    /* Set androidboot.avb.device_state to "locked" or "unlocked". */
    int is_unlocked;
    if (!ops->read_is_unlocked(ops, &is_unlocked)) {
      avb_warning("Error getting device state.\n");
      ret = AVB_SLOT_VERIFY_RESULT_ERROR_IO;
      goto fail;
    }
    if (!cmdline_append_option(slot_data, "androidboot.vbmeta.device_state",
                               is_unlocked ? "unlocked" : "locked")) {
      ret = AVB_SLOT_VERIFY_RESULT_ERROR_OOM;
      goto fail;
    }

    /* Set androidboot.vbmeta.{hash_alg, size, digest}. */
    AvbSHA256Ctx ctx;
    avb_sha256_init(&ctx);
    avb_sha256_update(&ctx, slot_data->vbmeta_data, slot_data->vbmeta_size);
    if (!cmdline_append_option(slot_data, "androidboot.vbmeta.hash_alg",
                               "sha256") ||
        !cmdline_append_uint64_base10(slot_data, "androidboot.vbmeta.size",
                                      slot_data->vbmeta_size) ||
        !cmdline_append_hex(slot_data, "androidboot.vbmeta.digest",
                            avb_sha256_final(&ctx), AVB_SHA256_DIGEST_SIZE)) {
      ret = AVB_SLOT_VERIFY_RESULT_ERROR_OOM;
      goto fail;
    }

    if (out_data != NULL) {
      *out_data = slot_data;
    } else {
      avb_slot_verify_data_free(slot_data);
    }
  }

  return ret;

fail:
  if (slot_data != NULL) {
    avb_slot_verify_data_free(slot_data);
  }
  return ret;
}

void avb_slot_verify_data_free(AvbSlotVerifyData* data) {
  if (data->boot_data != NULL) {
    avb_free(data->boot_data);
  }
  if (data->cmdline != NULL) {
    avb_free(data->cmdline);
  }
  avb_free(data);
}
