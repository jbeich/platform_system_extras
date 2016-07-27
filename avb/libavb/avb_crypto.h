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

#ifndef AVB_CRYPTO_H_
#define AVB_CRYPTO_H_

#include "avb_sysdeps.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Algorithms that can be used in the vbmeta image for
 * verification. An algorithm consists of a hash type and a signature
 * type.
 *
 * The data used to calculate the hash is the three blocks mentioned
 * in the documentation for |AvbVBMetaImageHeader| except for the data
 * in the "Authentication data" block.
 *
 * For signatures with RSA keys, PKCS v1.5 padding is used. The public
 * key data is stored in the auxiliary data block, see
 * |AvbRSAPublicKeyHeader| for the serialization format.
 *
 * Each algorithm type is described below:
 *
 * AVB_ALGORITHM_TYPE_NONE: There is no hash, no signature of the
 * data, and no public key. The data cannot be verified. The fields
 * |hash_size|, |signature_size|, and |public_key_size| must be zero.
 *
 * AVB_ALGORITHM_TYPE_SHA256_RSA2048: The hash function used is
 * SHA-256, resulting in 32 bytes of hash digest data. This hash is
 * signed with a 2048-bit RSA key. The field |hash_size| must be 32,
 * |signature_size| must be 256, and the public key data must have
 * |key_num_bits| set to 2048.
 *
 * AVB_ALGORITHM_TYPE_SHA256_RSA4096: Like above, but only with
 * a 4096-bit RSA key and |signature_size| set to 512.
 *
 * AVB_ALGORITHM_TYPE_SHA256_RSA8192: Like above, but only with
 * a 8192-bit RSA key and |signature_size| set to 1024.
 *
 * AVB_ALGORITHM_TYPE_SHA512_RSA2048: The hash function used is
 * SHA-512, resulting in 64 bytes of hash digest data. This hash is
 * signed with a 2048-bit RSA key. The field |hash_size| must be 64,
 * |signature_size| must be 256, and the public key data must have
 * |key_num_bits| set to 2048.
 *
 * AVB_ALGORITHM_TYPE_SHA512_RSA4096: Like above, but only with
 * a 4096-bit RSA key and |signature_size| set to 512.
 *
 * AVB_ALGORITHM_TYPE_SHA512_RSA8192: Like above, but only with
 * a 8192-bit RSA key and |signature_size| set to 1024.
 */
typedef enum {
  AVB_ALGORITHM_TYPE_NONE,
  AVB_ALGORITHM_TYPE_SHA256_RSA2048,
  AVB_ALGORITHM_TYPE_SHA256_RSA4096,
  AVB_ALGORITHM_TYPE_SHA256_RSA8192,
  AVB_ALGORITHM_TYPE_SHA512_RSA2048,
  AVB_ALGORITHM_TYPE_SHA512_RSA4096,
  AVB_ALGORITHM_TYPE_SHA512_RSA8192,
  _AVB_ALGORITHM_NUM_TYPES
} AvbAlgorithmType;

/* The header for a serialized RSA public key.
 *
 * The size of the key is given by |key_num_bits|, for example 2048
 * for a RSA-2048 key. By definition, a RSA public key is the pair (n,
 * e) where |n| is the modulus (which can be represented in
 * |key_num_bits| bits) and |e| is the public exponent. The exponent
 * is not stored since it's assumed to always be 65537.
 *
 * To optimize verification, the key block includes two precomputed
 * values, |n0inv| (fits in 32 bits) and |rr| and can always be
 * represented in |key_num_bits|.

 * The value |n0inv| is the value -1/n[0] (mod 2^32). The value |rr|
 * is (2^key_num_bits)^2 (mod n).
 *
 * Following this header is |key_num_bits| bits of |n|, then
 * |key_num_bits| bits of |rr|. Both values are stored with most
 * significant bit first. Each serialized number takes up
 * |key_num_bits|/8 bytes.
 *
 * All fields in this struct are stored in network byte order when
 * serialized.  To generate a copy with fields swapped to native byte
 * order, use the function avb_rsa_public_key_header_validate_and_byteswap().
 *
 * The avb_rsa_verify() function expects a key in this serialized
 * format.
 *
 * The 'avbtool extract_public_key' command can be used to generate a
 * serialized RSA public key.
 */
typedef struct AvbRSAPublicKeyHeader {
  uint32_t key_num_bits;
  uint32_t n0inv;
} AVB_ATTR_PACKED AvbRSAPublicKeyHeader;

/* Copies |src| to |dest| and validates, byte-swapping fields in the
 * process if needed. Returns true if valid, false if invalid.
 */
bool avb_rsa_public_key_header_validate_and_byteswap(
    const AvbRSAPublicKeyHeader* src,
    AvbRSAPublicKeyHeader* dest) AVB_ATTR_WARN_UNUSED_RESULT;

#ifdef __cplusplus
}
#endif

#endif /* AVB_CRYPTO_H_ */
