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

#include "avb_property_descriptor.h"
#include "avb_util.h"

int avb_property_descriptor_validate_and_byteswap(
    const AvbPropertyDescriptor* src, AvbPropertyDescriptor* dest) {
  uint64_t expected_size;

  avb_memcpy(dest, src, sizeof(AvbPropertyDescriptor));

  if (!avb_descriptor_validate_and_byteswap((const AvbDescriptor*)src,
                                            (AvbDescriptor*)dest))
    return 0;

  if (dest->parent_descriptor.tag != AVB_DESCRIPTOR_TAG_PROPERTY) {
    avb_error("Invalid tag %" PRIu64 " for property descriptor.\n",
              dest->parent_descriptor.tag);
    return 0;
  }

  dest->key_num_bytes = avb_be64toh(dest->key_num_bytes);
  dest->value_num_bytes = avb_be64toh(dest->value_num_bytes);

  /* Check that key and value are fully contained. */
  expected_size = sizeof(AvbPropertyDescriptor) - sizeof(AvbDescriptor) + 2;
  if (!avb_safe_add_to(&expected_size, dest->key_num_bytes) ||
      !avb_safe_add_to(&expected_size, dest->value_num_bytes)) {
    avb_error("Overflow while adding up sizes.\n");
    return 0;
  }
  if (expected_size > dest->parent_descriptor.num_bytes_following) {
    avb_error("Descriptor payload size overflow.\n");
    return 0;
  }

  return 1;
}

typedef struct {
  const char* key;
  size_t key_size;
  const char* ret_value;
  size_t ret_value_size;
} PropertyIteratorData;

static int property_lookup_desc_foreach(const AvbDescriptor* header,
                                        void* user_data) {
  PropertyIteratorData* data = (PropertyIteratorData*)user_data;
  AvbPropertyDescriptor ph;
  const uint8_t* p;
  int ret = 1;

  if (header->tag != AVB_DESCRIPTOR_TAG_PROPERTY) goto out;

  if (!avb_property_descriptor_validate_and_byteswap(
          (const AvbPropertyDescriptor*)header, &ph))
    goto out;

  p = (const uint8_t*)header;
  if (p[sizeof(AvbPropertyDescriptor) + ph.key_num_bytes] != 0) {
    avb_error("No terminating NUL byte in key.\n");
    goto out;
  }

  if (data->key_size == ph.key_num_bytes) {
    if (avb_memcmp(p + sizeof(AvbPropertyDescriptor), data->key,
                   data->key_size) == 0) {
      data->ret_value = (const char*)(p + sizeof(AvbPropertyDescriptor) +
                                      ph.key_num_bytes + 1);
      data->ret_value_size = ph.value_num_bytes;
      /* Stop iterating. */
      ret = 0;
      goto out;
    }
  }

out:
  return ret;
}

const char* avb_property_lookup(const uint8_t* image_data, size_t image_size,
                                const char* key, size_t key_size,
                                size_t* out_value_size) {
  PropertyIteratorData data;

  if (key_size == 0) key_size = avb_strlen(key);

  data.key = key;
  data.key_size = key_size;

  if (avb_descriptor_foreach(image_data, image_size,
                             property_lookup_desc_foreach, &data) == 0) {
    if (out_value_size != NULL) *out_value_size = data.ret_value_size;
    return data.ret_value;
  }

  if (out_value_size != NULL) *out_value_size = 0;
  return NULL;
}

int avb_property_lookup_uint64(const uint8_t* image_data, size_t image_size,
                               const char* key, size_t key_size,
                               uint64_t* out_value) {
  const char* value;
  int ret = 0;
  uint64_t parsed_val;
  int base;
  int n;

  value = avb_property_lookup(image_data, image_size, key, key_size, NULL);
  if (value == NULL) goto out;

  base = 10;
  if (avb_memcmp(value, "0x", 2) == 0) {
    base = 16;
    value += 2;
  }

  parsed_val = 0;
  for (n = 0; value[n] != '\0'; n++) {
    int c = value[n];
    int digit;

    parsed_val *= base;

    switch (base) {
      case 10:
        if (c >= '0' && c <= '9') {
          digit = c - '0';
        } else {
          avb_error("Invalid digit.\n");
          goto out;
        }
        break;

      case 16:
        if (c >= '0' && c <= '9') {
          digit = c - '0';
        } else if (c >= 'a' && c <= 'f') {
          digit = c - 'a' + 10;
        } else if (c >= 'A' && c <= 'F') {
          digit = c - 'A' + 10;
        } else {
          avb_error("Invalid digit.\n");
          goto out;
        }
        break;

      default:
        goto out;
    }

    parsed_val += digit;
  }

  ret = 1;
  if (out_value != NULL) *out_value = parsed_val;

out:
  return ret;
}
