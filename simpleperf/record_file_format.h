/*
 * Copyright (C) 2015 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef SIMPLE_PERF_RECORD_FILE_FORMAT_H_
#define SIMPLE_PERF_RECORD_FILE_FORMAT_H_

// The file structure of perf.data:
//    file_header
//    id_section
//    file_attr section
//    data section
//    feature section
//
//  feature section contains following structure:
//    a file_section array, each element contains the storage information of one add_feature.
//    data of feature 1
//    data of feature 2
//    ....

enum {
  FEAT_RESERVED = 0,
  FEAT_FIRST_FEATURE = 1,
  FEAT_TRACING_DATA = 1,
  FEAT_BUILD_ID,
  FEAT_HOSTNAME,
  FEAT_OSRELEASE,
  FEAT_VERSION,
  FEAT_ARCH,
  FEAT_NRCPUS,
  FEAT_CPUDESC,
  FEAT_CPUID,
  FEAT_TOTAL_MEM,
  FEAT_CMDLINE,
  FEAT_EVENT_DESC,
  FEAT_CPU_TOPOLOGY,
  FEAT_NUMA_TOPOLOGY,
  FEAT_BRANCH_STACK,
  FEAT_PMU_MAPPINGS,
  FEAT_GROUP_DESC,
  FEAT_LAST_FEATURE,
  FEAT_MAX_NUM  = 256,
};

struct file_section {
  uint64_t offset;
  uint64_t size;
};

#define PERF_MAGIC "PERFILE2"

struct file_header {
  char magic[8];
  uint64_t header_size;
  uint64_t attr_size;
  file_section attrs;
  file_section data;
  file_section event_types;
  unsigned char adds_features[FEAT_MAX_NUM / 8];
};

struct file_attr {
  perf_event_attr attr;
  file_section ids;
};

#endif  // SIMPLE_PERF_RECORD_FILE_FORMAT_H_
