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

#include "avb_crypto.h"
#include "avb_rsa.h"
#include "avb_sha.h"
#include "avb_util.h"

bool avb_rsa_public_key_header_validate_and_byteswap(
    const AvbRSAPublicKeyHeader* src, AvbRSAPublicKeyHeader* dest) {
  avb_memcpy(dest, src, sizeof(AvbRSAPublicKeyHeader));

  dest->key_num_bits = avb_be32toh(dest->key_num_bits);
  dest->n0inv = avb_be32toh(dest->n0inv);

  return true;
}
