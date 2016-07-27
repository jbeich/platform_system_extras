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

#include "avb_descriptor.h"
#include "avb_util.h"
#include "avb_vbmeta_image.h"

int avb_descriptor_validate_and_byteswap(const AvbDescriptor* src,
                                         AvbDescriptor* dest) {
  dest->tag = avb_be64toh(src->tag);
  dest->num_bytes_following = avb_be64toh(src->num_bytes_following);

  if ((dest->num_bytes_following & 0x07) != 0) {
    avb_warning("Descriptor size is not divisible by 8.\n");
    return 0;
  }
  return 1;
}

int avb_descriptor_foreach(const uint8_t* image_data, size_t image_size,
                           AvbDescriptorForeachFunc foreach_func,
                           void* user_data) {
  const AvbVBMetaImageHeader* header = NULL;
  int ret = 0;
  const uint8_t* image_end;
  const uint8_t* desc_start;
  const uint8_t* desc_end;
  const uint8_t* p;

  if (image_data == NULL) {
    avb_warning("image_data is NULL\n.");
    goto out;
  }

  if (foreach_func == NULL) {
    avb_warning("foreach_func is NULL\n.");
    goto out;
  }

  if (image_size < sizeof(AvbVBMetaImageHeader)) {
    avb_warning("Length is smaller than header.\n");
    goto out;
  }

  // Ensure magic is correct.
  if (avb_memcmp(image_data, AVB_MAGIC, AVB_MAGIC_LEN) != 0) {
    avb_warning("Magic is incorrect.\n");
    goto out;
  }

  // Careful, not byteswapped - also ensure it's aligned properly.
  avb_assert_word_aligned(image_data);
  header = (const AvbVBMetaImageHeader*)image_data;
  image_end = image_data + image_size;

  desc_start = image_data + sizeof(AvbVBMetaImageHeader) +
               avb_be64toh(header->authentication_data_block_size) +
               avb_be64toh(header->descriptors_offset);

  desc_end = desc_start + avb_be64toh(header->descriptors_size);

  if (desc_start < image_data || desc_start > image_end ||
      desc_end < image_data || desc_end > image_end || desc_end < desc_start) {
    avb_warning("Descriptors not inside passed-in data.\n");
    goto out;
  }

  for (p = desc_start; p < desc_end;) {
    const AvbDescriptor* dh = (const AvbDescriptor*)p;
    avb_assert_word_aligned(dh);
    uint64_t nb_following = avb_be64toh(dh->num_bytes_following);
    uint64_t nb_total = sizeof(AvbDescriptor) + nb_following;

    if ((nb_total & 7) != 0) {
      avb_warning("Invalid descriptor length.\n");
      goto out;
    }

    if (nb_total + p < desc_start || nb_total + p > desc_end) {
      avb_warning("Invalid data in descriptors array.\n");
      goto out;
    }

    if (foreach_func(dh, user_data) == 0) {
      goto out;
    }

    p += nb_total;
  }

  ret = 1;

out:
  return ret;
}

static int count_descriptors(const AvbDescriptor* descriptor, void* user_data) {
  size_t* num_descriptors = user_data;
  *num_descriptors += 1;
  return 1;
}

typedef struct {
  size_t n;
  const AvbDescriptor** ret;
} SetDescriptorData;

static int set_descriptors(const AvbDescriptor* descriptor, void* user_data) {
  SetDescriptorData* data = user_data;
  data->ret[data->n++] = descriptor;
  return 1;
}

const AvbDescriptor** avb_descriptor_get_all(const uint8_t* image_data,
                                             size_t image_size,
                                             size_t* out_num_descriptors) {
  size_t num_descriptors = 0;
  SetDescriptorData data;

  avb_descriptor_foreach(image_data, image_size, count_descriptors,
                         &num_descriptors);

  data.n = 0;
  data.ret = avb_calloc(sizeof(const AvbDescriptor*) * (num_descriptors + 1));
  if (data.ret == NULL) {
    return NULL;
  }
  avb_descriptor_foreach(image_data, image_size, set_descriptors, &data);
  avb_assert(data.n == num_descriptors);

  if (out_num_descriptors != NULL) {
    *out_num_descriptors = num_descriptors;
  }

  return data.ret;
}
