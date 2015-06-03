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

#ifndef SIMPLE_PERF_PERF_EVENT_H_
#define SIMPLE_PERF_PERF_EVENT_H_

#include <sys/cdefs.h>  // Include __BIONIC__ macro.

#if defined(__BIONIC__)

#include <linux/perf_event.h>

#else
// TODO: Remove work-arounds below when platform glibc version of perf_event.h contains them.

#define perf_event_attr perf_event_attr_old

#include <linux/perf_event.h>

#undef perf_event_attr

enum {
  PERF_SAMPLE_BRANCH_STACK = 1U << 11,
};

enum perf_branch_sample_type {
  PERF_SAMPLE_BRANCH_USER = 1U << 0,   /* user branches */
  PERF_SAMPLE_BRANCH_KERNEL = 1U << 1, /* kernel branches */
  PERF_SAMPLE_BRANCH_HV = 1U << 2,     /* hypervisor branches */

  PERF_SAMPLE_BRANCH_ANY = 1U << 3,        /* any branch types */
  PERF_SAMPLE_BRANCH_ANY_CALL = 1U << 4,   /* any call branch */
  PERF_SAMPLE_BRANCH_ANY_RETURN = 1U << 5, /* any return branch */
  PERF_SAMPLE_BRANCH_IND_CALL = 1U << 6,   /* indirect calls */
  PERF_SAMPLE_BRANCH_ABORT_TX = 1U << 7,   /* transaction aborts */
  PERF_SAMPLE_BRANCH_IN_TX = 1U << 8,      /* in transaction */
  PERF_SAMPLE_BRANCH_NO_TX = 1U << 9,      /* not in transaction */
  PERF_SAMPLE_BRANCH_COND = 1U << 10,      /* conditional branches */

  PERF_SAMPLE_BRANCH_MAX = 1U << 11, /* non-ABI */
};

/*
 * perf_event_attr structure is copied from bionic/libc/kernel/uapi/linux/perf_event.h.
 * Hardware event_id to monitor via a performance monitoring event:
 */
struct perf_event_attr {
  __u32 type;
  /* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u32 size;
  __u64 config;
  union {
    __u64 sample_period;
    /* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
    __u64 sample_freq;
  };
  __u64 sample_type;
  __u64 read_format;
  /* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u64 disabled : 1, inherit : 1, pinned : 1, exclusive : 1, exclude_user : 1, exclude_kernel : 1,
      exclude_hv : 1, exclude_idle : 1, mmap : 1, comm : 1, freq : 1, inherit_stat : 1,
      enable_on_exec : 1, task : 1, watermark : 1, precise_ip : 2, mmap_data : 1, sample_id_all : 1,
      exclude_host : 1, exclude_guest : 1, exclude_callchain_kernel : 1, exclude_callchain_user : 1,
      mmap2 : 1, comm_exec : 1, __reserved_1 : 39;
  union {
    __u32 wakeup_events;
    __u32 wakeup_watermark;
    /* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  };
  __u32 bp_type;
  union {
    __u64 bp_addr;
    /* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
    __u64 config1;
  };
  union {
    __u64 bp_len;
    /* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
    __u64 config2;
  };
  __u64 branch_sample_type;
  __u64 sample_regs_user;
  /* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u32 sample_stack_user;
  __u32 __reserved_2;
};

#endif

#endif  // SIMPLE_PERF_PERF_EVENT_H_
