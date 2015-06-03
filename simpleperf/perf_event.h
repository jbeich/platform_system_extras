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
 * perf_event_attr structure is copyed from linux-4.0.
 * Hardware event_id to monitor via a performance monitoring event:
 */
struct perf_event_attr {
  /*
   * Major type: hardware/software/tracepoint/etc.
   */
  __u32 type;

  /*
   * Size of the attr structure, for fwd/bwd compat.
   */
  __u32 size;

  /*
   * Type specific configuration information.
   */
  __u64 config;

  union {
    __u64 sample_period;
    __u64 sample_freq;
  };

  __u64 sample_type;
  __u64 read_format;

  __u64 disabled : 1,     /* off by default        */
      inherit : 1,        /* children inherit it   */
      pinned : 1,         /* must always be on PMU */
      exclusive : 1,      /* only group on PMU     */
      exclude_user : 1,   /* don't count user      */
      exclude_kernel : 1, /* ditto kernel          */
      exclude_hv : 1,     /* ditto hypervisor      */
      exclude_idle : 1,   /* don't count when idle */
      mmap : 1,           /* include mmap data     */
      comm : 1,           /* include comm data     */
      freq : 1,           /* use freq, not period  */
      inherit_stat : 1,   /* per task counts       */
      enable_on_exec : 1, /* next exec enables     */
      task : 1,           /* trace fork/exit       */
      watermark : 1,      /* wakeup_watermark      */
                          /*
                           * precise_ip:
                           *
                           *  0 - SAMPLE_IP can have arbitrary skid
                           *  1 - SAMPLE_IP must have constant skid
                           *  2 - SAMPLE_IP requested to have 0 skid
                           *  3 - SAMPLE_IP must have 0 skid
                           *
                           *  See also PERF_RECORD_MISC_EXACT_IP
                           */
      precise_ip : 2,     /* skid constraint       */
      mmap_data : 1,      /* non-exec mmap data    */
      sample_id_all : 1,  /* sample_type all events */

      exclude_host : 1,  /* don't count in host   */
      exclude_guest : 1, /* don't count in guest  */

      exclude_callchain_kernel : 1, /* exclude kernel callchains */
      exclude_callchain_user : 1,   /* exclude user callchains */
      mmap2 : 1,                    /* include mmap with inode data     */
      comm_exec : 1,                /* flag comm events that are due to an exec */
      __reserved_1 : 39;

  union {
    __u32 wakeup_events;    /* wakeup every n events */
    __u32 wakeup_watermark; /* bytes before wakeup   */
  };

  __u32 bp_type;
  union {
    __u64 bp_addr;
    __u64 config1; /* extension of config */
  };
  union {
    __u64 bp_len;
    __u64 config2; /* extension of config1 */
  };
  __u64 branch_sample_type; /* enum perf_branch_sample_type */

  /*
   * Defines set of user regs to dump on samples.
   * See asm/perf_regs.h for details.
   */
  __u64 sample_regs_user;

  /*
   * Defines size of the user stack to dump on samples.
   */
  __u32 sample_stack_user;

  /* Align to u64. */
  __u32 __reserved_2;
  /*
   * Defines set of regs to dump for each sample
   * state captured on:
   *  - precise = 0: PMU interrupt
   *  - precise > 0: sampled instruction
   *
   * See asm/perf_regs.h for details.
   */
  __u64 sample_regs_intr;
};

#endif

#endif  // SIMPLE_PERF_PERF_EVENT_H_
