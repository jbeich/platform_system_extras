/*
 * Copyright (C) 2015 The Android Open Source Project
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

#include <libavb.h>
#include "fec_private.h"

static int fec_avb_parse_footer(struct fec_handle *f, size_t *vbmeta_offset,
                                size_t *vbmeta_size) {
    uint8_t footer_buf[AVB_FOOTER_SIZE];
    AvbFooter footer;

    if (fec_pread(f, &ecc_footer_buf, AVB_FOOTER_SIZE,
                  f->size - AVB_FOOTER_SIZE) != AVB_FOOTER_SIZE) {
        error("failed to read AVB footer: %s", strerror(errno));
        return -1;
    }

    if (!avb_footer_validate_and_byteswap((const AvbFooter *)footer_buf,
                                          &footer)) {
        error("Error validating AVB footer");
        return -1;
    }

    /* Basic footer sanity check since the data is untrusted. */
    check(footer.vbmeta_size <= VBMETA_MAX_SIZE);

    *vbmeta_offset = footer.vbmeta_offset;
    *vbmeta_size = footer.vbmeta_size;

    return 0;
}

static int fec_avb_get_hashtree_descriptor(struct fec_handle *f,
                                           AvbHashtreeDescriptor *hashtree_desc,
                                           const uint8_t *expected_public_key,
                                           size_t expected_public_key_length,
                                           bool allow_disable) {
    size_t vbmeta_offset;
    size_t vbmeta_size;
    uint8_t *vbmeta_buf = NULL;
    AvbVBMetaVerifyResult vbmeta_ret;
    const uint8_t *pk_data;
    size_t pk_len;
    const AvbDescriptor **descriptors = NULL;
    size_t num_descriptors;
    int ret = -1;

    if (fec_avb_parse_footer(f, &vbmeta_offset, &vbmeta_size) == -1) {
        return ret;
    }

    vbmeta_buf = malloc(vbmeta_size);

    if (vbmeta_buf == NULL) {
        error("Failed to malloc %d bytes for vbmeta", vbmeta_size);
        return ret;
    }

    if (!raw_pread(f, &vbmeta_buf, vbmeta_size, vbmeta_offset)) {
        error("failed to read AVB vbmeta: %s", strerror(errno));
        return ret;
    }

    vbmeta_ret = avb_vbmeta_image_verify(vbmeta_buf, vbmeta_size, &pk_data,
                                         &pk_len, allow_disable);

    if (vbmeta_ret == AVB_VBMETA_VERIFY_RESULT_DISABLE) {
        warn("Verity disabled in vbmeta image");
        ret = 0;
        goto out;
    } else if (vbmeta_ret != AVB_VBMETA_VERIFY_RESULT_OK) {
        /* raw vbmeta footer is invalid; this could be due to corruption, or
           due to missing vbmeta. Try to use FEC to correct it. */
        if (fec_pread(f, &vbmeta_buf, vbmeta_size, vbmeta_offset) !=
            vbmeta_size) {
            error("failed to read AVB ecc vbmeta: %s", strerror(errno));
            goto out;
        }

        vbmeta_ret = avb_vbmeta_image_verify(vbmeta_buf, vbmeta_size, &pk_data,
                                             &pk_len, allow_disable);

        if (vbmeta_ret != AVB_VBMETA_VERIFY_RESULT_OK) {
            goto out;
        }
    }

    if (expected_public_key != NULL) {
        if (expected_public_key_length != pk_len ||
            memcmp(expected_public_key, pk_data, pk_len) != 0) {
            error(
                "Public key used to sign data does not match key in chain "
                "partition descriptor");
            goto out;
        }
    }

    descriptors =
        avb_descriptor_get_all(vbmeta_buf, vbmeta_num_read, &num_descriptors);

    check(num_descriptors == 1);
    AvbDescriptor desc;

    if (!avb_descriptor_validate_and_byteswap(descriptors[0], &desc)) {
        error("Descriptor is invalid.");
        goto out;
    }
    check(desc.tag == AVB_DESCRIPTOR_TAG_HASHTREE);

    if (!avb_hashtree_descriptor_validate_and_byteswap(
            (const AvbHashtreeDescriptor *)desc, &hashtree_desc)) {
        goto out;
    }
    ret = 0;

out:
    if (vbmeta_buf != NULL) {
        free(vbmeta_buf);
    }
    if (descriptors != NULL) {
        free(descriptors);
    }
    return ret;
}
