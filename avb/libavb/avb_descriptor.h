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

#if !defined(AVB_INSIDE_LIBAVB_H) && !defined(AVB_COMPILATION)
#error "Never include this file directly, include libavb.h instead."
#endif

#ifndef AVB_DESCRIPTOR_H_
#define AVB_DESCRIPTOR_H_

#include "avb_sysdeps.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Well-known descriptor tags.
 *
 * AVB_DESCRIPTOR_TAG_PROPERTY: see |AvbPropertyDescriptor| struct.
 * AVB_DESCRIPTOR_TAG_HASHTREE: see |AvbHashtreeDescriptor| struct.
 * AVB_DESCRIPTOR_TAG_HASH: see |AvbHashDescriptor| struct.
 * AVB_DESCRIPTOR_TAG_KERNEL_CMDLINE: see |AvbKernelCmdlineDescriptor| struct.
 * AVB_DESCRIPTOR_TAG_CHAIN_PARTITION: see |AvbChainPartitionDescriptor| struct.
 */
typedef enum {
  AVB_DESCRIPTOR_TAG_PROPERTY,
  AVB_DESCRIPTOR_TAG_HASHTREE,
  AVB_DESCRIPTOR_TAG_HASH,
  AVB_DESCRIPTOR_TAG_KERNEL_CMDLINE,
  AVB_DESCRIPTOR_TAG_CHAIN_PARTITION,
} AvbDescriptorTag;

/* The header for a serialized descriptor.
 *
 * A descriptor always have two fields, a |tag| (denoting its type,
 * see the |AvbDescriptorTag| enumeration) and the size of the bytes
 * following, |num_bytes_following|.
 *
 * For padding, |num_bytes_following| is always a multiple of 8.
 */
typedef struct AvbDescriptor {
  uint64_t tag;
  uint64_t num_bytes_following;
} AVB_ATTR_PACKED AvbDescriptor;

/* Copies |src| to |dest| and validates, byte-swapping fields in the
 * process if needed. Returns non-zero if valid, zero if invalid.
 *
 * Data following the struct is not validated nor copied.
 */
int avb_descriptor_validate_and_byteswap(
    const AvbDescriptor* src, AvbDescriptor* dest) AVB_ATTR_WARN_UNUSED_RESULT;

/* Signature for callback function used in avb_descriptor_foreach().
 * The passed in descriptor is given by |descriptor| and the
 * |user_data| passed to avb_descriptor_foreach() function is in
 * |user_data|. Return non-zero to continue iterating, zero to stop
 * iterating.
 *
 * Note that |descriptor| points into the image passed to
 * avb_descriptor_foreach() - all fields need to be byteswapped!
 */
typedef int AvbDescriptorForeachFunc(const AvbDescriptor* descriptor,
                                     void* user_data);

/* Convenience function to iterate over all descriptors in an vbmeta
 * image.
 *
 * The function given by |foreach_func| will be called for each
 * descriptor. The given function should return non-zero to continue
 * iterating, zero to stop.
 *
 * The |user_data| parameter will be passed to |foreach_func|.
 *
 * Returns zero only if the iteration was short-circuited, that is if
 * an invocation of |foreach_func| returned zero.
 *
 * Before using this function, you MUST verify |image_data| with
 * avb_vbmeta_image_verify() and reject it unless it's signed by a known
 * good public key.
 */
int avb_descriptor_foreach(const uint8_t* image_data, size_t image_size,
                           AvbDescriptorForeachFunc foreach_func,
                           void* user_data);

/* Gets all descriptors in a vbmeta image.
 *
 * The return value is a NULL-pointer terminated array of
 * AvbDescriptor pointers. Free with avb_free() when you are done with
 * it. If |out_num_descriptors| is non-NULL, the number of descriptors
 * will be returned there.
 *
 * Note that each AvbDescriptor pointer in the array points into
 * |image_data| - all fields need to be byteswapped!
 *
 * Before using this function, you MUST verify |image_data| with
 * avb_vbmeta_image_verify() and reject it unless it's signed by a known
 * good public key.
 */
const AvbDescriptor** avb_descriptor_get_all(
    const uint8_t* image_data, size_t image_size,
    size_t* out_num_descriptors) AVB_ATTR_WARN_UNUSED_RESULT;

#ifdef __cplusplus
}
#endif

#endif /* AVB_DESCRIPTOR_H_ */
