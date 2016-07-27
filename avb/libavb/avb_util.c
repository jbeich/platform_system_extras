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

#include "avb_util.h"

uint32_t avb_be32toh(uint32_t in) {
  uint8_t* d = (uint8_t*)&in;
  uint32_t ret;
  ret = ((uint32_t)d[0]) << 24;
  ret |= ((uint32_t)d[1]) << 16;
  ret |= ((uint32_t)d[2]) << 8;
  ret |= ((uint32_t)d[3]);
  return ret;
}

uint64_t avb_be64toh(uint64_t in) {
  uint8_t* d = (uint8_t*)&in;
  uint64_t ret;
  ret = ((uint64_t)d[0]) << 56;
  ret |= ((uint64_t)d[1]) << 48;
  ret |= ((uint64_t)d[2]) << 40;
  ret |= ((uint64_t)d[3]) << 32;
  ret |= ((uint64_t)d[4]) << 24;
  ret |= ((uint64_t)d[5]) << 16;
  ret |= ((uint64_t)d[6]) << 8;
  ret |= ((uint64_t)d[7]);
  return ret;
}

int avb_safe_add_to(uint64_t* value, uint64_t value_to_add) {
  uint64_t original_value;

  avb_assert(value != NULL);

  original_value = *value;

  *value += value_to_add;
  if (*value < original_value) {
    avb_warning("%s: overflow: 0x%016" PRIx64 " + 0x%016" PRIx64 "\n",
                __FUNCTION__, original_value, value_to_add);
    return 0;
  }

  return 1;
}

int avb_safe_add(uint64_t* out_result, uint64_t a, uint64_t b) {
  uint64_t dummy;
  if (out_result == NULL) out_result = &dummy;
  *out_result = a;
  return avb_safe_add_to(out_result, b);
}

int avb_validate_utf8(const uint8_t* data, size_t num_bytes) {
  size_t n;
  unsigned int num_cc;

  for (n = 0, num_cc = 0; n < num_bytes; n++) {
    uint8_t c = data[n];

    if (num_cc > 0) {
      if ((c & (0x80 | 0x40)) == 0x80) {
        // 10xx xxxx
      } else {
        goto fail;
      }
      num_cc--;
    } else {
      if (c < 0x80) {
        num_cc = 0;
      } else if ((c & (0x80 | 0x40 | 0x20)) == (0x80 | 0x40)) {
        // 110x xxxx
        num_cc = 1;
      } else if ((c & (0x80 | 0x40 | 0x20 | 0x10)) == (0x80 | 0x40 | 0x20)) {
        // 1110 xxxx
        num_cc = 2;
      } else if ((c & (0x80 | 0x40 | 0x20 | 0x10 | 0x08)) ==
                 (0x80 | 0x40 | 0x20 | 0x10)) {
        // 1111 0xxx
        num_cc = 3;
      } else {
        goto fail;
      }
    }
  }

  if (num_cc != 0) {
    goto fail;
  }

  return 1;

fail:
  return 0;
}

int avb_str_concat(char* buf, size_t buf_size, const char* str1,
                   size_t str1_len, const char* str2, size_t str2_len) {
  uint64_t combined_len;

  if (!avb_safe_add(&combined_len, str1_len, str2_len)) {
    avb_warning("Overflow when adding string sizes.\n");
    return 0;
  }

  if (combined_len > buf_size - 1) {
    avb_warning("Insufficient buffer space.\n");
    return 0;
  }

  avb_memcpy(buf, str1, str1_len);
  avb_memcpy(buf + str1_len, str2, str2_len);
  buf[combined_len] = '\0';

  return 1;
}

void* avb_malloc(size_t size) {
  void* ret = avb_malloc_(size);
  if (ret == NULL) {
    avb_warning("Failed to allocate %zd bytes.\n", size);
    return NULL;
  }
  return ret;
}

void* avb_calloc(size_t size) {
  void* ret = avb_malloc(size);
  if (ret == NULL) {
    return NULL;
  }

  avb_memset(ret, '\0', size);
  return ret;
}

char* avb_strdup(const char* str) {
  size_t len = avb_strlen(str);
  char* ret = avb_malloc(len + 1);
  if (ret == NULL) {
    return NULL;
  }

  avb_memcpy(ret, str, len);
  ret[len] = '\0';

  return ret;
}

const char* avb_strstr(const char* haystack, const char* needle) {
  size_t n, m;

  // Look through |haystack| and check if the first character of
  // |needle| matches. If so, check the rest of |needle|.
  for (n = 0; haystack[n] != '\0'; n++) {
    if (haystack[n] != needle[0]) continue;

    for (m = 1;; m++) {
      if (needle[m] == '\0') {
        return haystack + n;
      }

      if (haystack[n + m] != needle[m]) break;
    }
  }

  return NULL;
}

char* avb_replace(const char* str, const char* search, const char* replace) {
  char* ret = NULL;
  size_t ret_len = 0;
  size_t search_len, replace_len;
  const char* str_after_last_replace;

  search_len = avb_strlen(search);
  replace_len = avb_strlen(replace);

  str_after_last_replace = str;
  while (*str != '\0') {
    const char* s;
    size_t num_before;
    size_t num_new;

    s = avb_strstr(str, search);
    if (s == NULL) break;

    num_before = s - str;

    if (ret == NULL) {
      num_new = num_before + replace_len + 1;
      ret = avb_malloc(num_new);
      if (ret == NULL) {
        goto out;
      }
      avb_memcpy(ret, str, num_before);
      avb_memcpy(ret + num_before, replace, replace_len);
      ret[num_new - 1] = '\0';
      ret_len = num_new - 1;
    } else {
      char* new_str;
      num_new = ret_len + num_before + replace_len + 1;
      new_str = avb_malloc(num_new);
      if (ret == NULL) {
        goto out;
      }
      avb_memcpy(new_str, ret, ret_len);
      avb_memcpy(new_str + ret_len, str, num_before);
      avb_memcpy(new_str + ret_len + num_before, replace, replace_len);
      new_str[num_new - 1] = '\0';
      avb_free(ret);
      ret = new_str;
      ret_len = num_new - 1;
    }

    str = s + search_len;
    str_after_last_replace = str;
  }

  if (ret == NULL) {
    ret = avb_strdup(str_after_last_replace);
    if (ret == NULL) {
      goto out;
    }
  } else {
    size_t num_remaining = avb_strlen(str_after_last_replace);
    size_t num_new = ret_len + num_remaining + 1;
    char* new_str = avb_malloc(num_new);
    if (ret == NULL) goto out;
    avb_memcpy(new_str, ret, ret_len);
    avb_memcpy(new_str + ret_len, str_after_last_replace, num_remaining);
    new_str[num_new - 1] = '\0';
    avb_free(ret);
    ret = new_str;
    ret_len = num_new - 1;
  }

out:
  return ret;
}
