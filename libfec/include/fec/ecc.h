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

#ifndef ___FEC_ECC_H___
#define ___FEC_ECC_H___

#include <fec/io.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ecc parameters */
#define FEC_RSM 255
#define FEC_PARAMS(roots) \
    8, 0x11d, 0, 1, (roots), 0

/* helper macros */
#define fec_div_round_up(x, y) \
    (((x) + (y) - 1) / (y))

#define fec_round_up(x,y) \
    (fec_div_round_up(x, y) * (y))

inline uint64_t fec_ecc_interleave(uint64_t offset, int rsn, uint64_t rounds)
{
    return (offset / rsn) + (offset % rsn) * rounds * FEC_BLOCKSIZE;
}

inline uint64_t fec_ecc_get_size(uint64_t file_size, int roots)
{
    return fec_div_round_up(fec_div_round_up(file_size, FEC_BLOCKSIZE),
                FEC_RSM - roots)
                    * roots * FEC_BLOCKSIZE
                + fec_round_up(sizeof(fec_header), FEC_BLOCKSIZE);
}


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ___FEC_ECC_H___ */
