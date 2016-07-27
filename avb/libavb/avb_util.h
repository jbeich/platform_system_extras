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

#ifndef AVB_UTIL_H_
#define AVB_UTIL_H_

#include "avb_sysdeps.h"
#include "avb_vbmeta_image.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Converts a 32-bit unsigned integer from big-endian to host byte order. */
uint32_t avb_be32toh(uint32_t in) AVB_ATTR_WARN_UNUSED_RESULT;

/* Converts a 64-bit unsigned integer from big-endian to host byte order. */
uint64_t avb_be64toh(uint64_t in) AVB_ATTR_WARN_UNUSED_RESULT;

/* Adds |value_to_add| to |value| with overflow protection.
 *
 * Returns zero if the addition overflows, non-zero otherwise. In
 * either case, |value| is always modified.
 */
int avb_safe_add_to(uint64_t* value,
                    uint64_t value_to_add) AVB_ATTR_WARN_UNUSED_RESULT;

/* Adds |a| and |b| with overflow protection, returning the value in
 * |out_result|.
 *
 * It's permissible to pass NULL for |out_result| if you just want to
 * check that the addition would not overflow.
 *
 * Returns zero if the addition overflows, non-zero otherwise.
 */
int avb_safe_add(uint64_t* out_result, uint64_t a,
                 uint64_t b) AVB_ATTR_WARN_UNUSED_RESULT;

/* Checks if |num_bytes| data at |data| is a valid UTF-8
 * string. Returns non-zero if valid UTF-8, zero otherwise.
 */
int avb_validate_utf8(const uint8_t* data,
                      size_t num_bytes) AVB_ATTR_WARN_UNUSED_RESULT;

/* Concatenates |str1| (of |str1_len| bytes) and |str2| (of |str2_len|
 * bytes) and puts the result in |buf| which holds |buf_size|
 * bytes. The result is also guaranteed to be NUL terminated. Fail if
 * there is not enough room in |buf| for the resulting string plus
 * terminating NUL byte.
 *
 * Returns non-zero if the operation succeeds, zero otherwise.
 */
int avb_str_concat(char* buf, size_t buf_size, const char* str1,
                   size_t str1_len, const char* str2, size_t str2_len);

/* Like avb_malloc_() but prints a warning using avb_warning if memory
 * allocation fails.
 */
void* avb_malloc(size_t size) AVB_ATTR_WARN_UNUSED_RESULT;

/* Like avb_malloc() but sets the memory with zeroes. */
void* avb_calloc(size_t size) AVB_ATTR_WARN_UNUSED_RESULT;

/* Duplicates a NUL-terminated string. Returns NULL on OOM. */
char* avb_strdup(const char* str) AVB_ATTR_WARN_UNUSED_RESULT;

/* Finds the first occurrence of |needle| in the string |haystack|
 * where both strings are NUL-terminated strings. The terminating NUL
 * bytes are not compared.
 *
 * Returns NULL if not found, otherwise points into |haystack| for the
 * first occurrence of |needle|.
 */
const char* avb_strstr(const char* haystack,
                       const char* needle) AVB_ATTR_WARN_UNUSED_RESULT;

/* Replaces all occurrences of |search| with |replace| in |str|.
 *
 * Returns a newly allocated string or NULL if out of memory.
 */
char* avb_replace(const char* str, const char* search,
                  const char* replace) AVB_ATTR_WARN_UNUSED_RESULT;

#ifdef __cplusplus
}
#endif

#endif /* AVB_UTIL_H_ */
