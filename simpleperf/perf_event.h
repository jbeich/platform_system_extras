// This file is copied from bionic/libc/kernel/uapi/linux/perf_event.h.
/****************************************************************************
 ****************************************************************************
 ***
 ***   This header was automatically generated from a Linux kernel header
 ***   of the same name, to make information necessary for userspace to
 ***   call into the kernel available to libc.  It contains only constants,
 ***   structures, and macros generated from the original header, and thus,
 ***   contains no copyrightable information.
 ***
 ***   To edit the content of this header, modify the corresponding
 ***   source file (e.g. under external/kernel-headers/original/) then
 ***   run bionic/libc/kernel/tools/update_all.py
 ***
 ***   Any manual change here will be lost the next time this script will
 ***   be run. You've been warned!
 ***
 ****************************************************************************
 ****************************************************************************/
#ifndef _UAPI_LINUX_PERF_EVENT_H
#define _UAPI_LINUX_PERF_EVENT_H
#include <linux/types.h>
#include <linux/ioctl.h>
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#include <asm/byteorder.h>
enum perf_type_id {
  PERF_TYPE_HARDWARE = 0,
  PERF_TYPE_SOFTWARE = 1,
  /* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  PERF_TYPE_TRACEPOINT = 2,
  PERF_TYPE_HW_CACHE = 3,
  PERF_TYPE_RAW = 4,
  PERF_TYPE_BREAKPOINT = 5,
  /* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  PERF_TYPE_MAX,
};
enum perf_hw_id {
  PERF_COUNT_HW_CPU_CYCLES = 0,
  /* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  PERF_COUNT_HW_INSTRUCTIONS = 1,
  PERF_COUNT_HW_CACHE_REFERENCES = 2,
  PERF_COUNT_HW_CACHE_MISSES = 3,
  PERF_COUNT_HW_BRANCH_INSTRUCTIONS = 4,
  /* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  PERF_COUNT_HW_BRANCH_MISSES = 5,
  PERF_COUNT_HW_BUS_CYCLES = 6,
  PERF_COUNT_HW_STALLED_CYCLES_FRONTEND = 7,
  PERF_COUNT_HW_STALLED_CYCLES_BACKEND = 8,
  /* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  PERF_COUNT_HW_REF_CPU_CYCLES = 9,
  PERF_COUNT_HW_MAX,
};
enum perf_hw_cache_id {
  /* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  PERF_COUNT_HW_CACHE_L1D = 0,
  PERF_COUNT_HW_CACHE_L1I = 1,
  PERF_COUNT_HW_CACHE_LL = 2,
  PERF_COUNT_HW_CACHE_DTLB = 3,
  /* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  PERF_COUNT_HW_CACHE_ITLB = 4,
  PERF_COUNT_HW_CACHE_BPU = 5,
  PERF_COUNT_HW_CACHE_NODE = 6,
  PERF_COUNT_HW_CACHE_MAX,
  /* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
enum perf_hw_cache_op_id {
  PERF_COUNT_HW_CACHE_OP_READ = 0,
  PERF_COUNT_HW_CACHE_OP_WRITE = 1,
  /* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  PERF_COUNT_HW_CACHE_OP_PREFETCH = 2,
  PERF_COUNT_HW_CACHE_OP_MAX,
};
enum perf_hw_cache_op_result_id {
  /* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  PERF_COUNT_HW_CACHE_RESULT_ACCESS = 0,
  PERF_COUNT_HW_CACHE_RESULT_MISS = 1,
  PERF_COUNT_HW_CACHE_RESULT_MAX,
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
enum perf_sw_ids {
  PERF_COUNT_SW_CPU_CLOCK = 0,
  PERF_COUNT_SW_TASK_CLOCK = 1,
  PERF_COUNT_SW_PAGE_FAULTS = 2,
  /* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  PERF_COUNT_SW_CONTEXT_SWITCHES = 3,
  PERF_COUNT_SW_CPU_MIGRATIONS = 4,
  PERF_COUNT_SW_PAGE_FAULTS_MIN = 5,
  PERF_COUNT_SW_PAGE_FAULTS_MAJ = 6,
  /* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  PERF_COUNT_SW_ALIGNMENT_FAULTS = 7,
  PERF_COUNT_SW_EMULATION_FAULTS = 8,
  PERF_COUNT_SW_DUMMY = 9,
  PERF_COUNT_SW_MAX,
  /* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
enum perf_event_sample_format {
  PERF_SAMPLE_IP = 1U << 0,
  PERF_SAMPLE_TID = 1U << 1,
  /* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  PERF_SAMPLE_TIME = 1U << 2,
  PERF_SAMPLE_ADDR = 1U << 3,
  PERF_SAMPLE_READ = 1U << 4,
  PERF_SAMPLE_CALLCHAIN = 1U << 5,
  /* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  PERF_SAMPLE_ID = 1U << 6,
  PERF_SAMPLE_CPU = 1U << 7,
  PERF_SAMPLE_PERIOD = 1U << 8,
  PERF_SAMPLE_STREAM_ID = 1U << 9,
  /* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  PERF_SAMPLE_RAW = 1U << 10,
  PERF_SAMPLE_BRANCH_STACK = 1U << 11,
  PERF_SAMPLE_REGS_USER = 1U << 12,
  PERF_SAMPLE_STACK_USER = 1U << 13,
  /* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  PERF_SAMPLE_WEIGHT = 1U << 14,
  PERF_SAMPLE_DATA_SRC = 1U << 15,
  PERF_SAMPLE_IDENTIFIER = 1U << 16,
  PERF_SAMPLE_TRANSACTION = 1U << 17,
  /* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  PERF_SAMPLE_MAX = 1U << 18,
};
enum perf_branch_sample_type {
  PERF_SAMPLE_BRANCH_USER = 1U << 0,
  /* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  PERF_SAMPLE_BRANCH_KERNEL = 1U << 1,
  PERF_SAMPLE_BRANCH_HV = 1U << 2,
  PERF_SAMPLE_BRANCH_ANY = 1U << 3,
  PERF_SAMPLE_BRANCH_ANY_CALL = 1U << 4,
  /* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  PERF_SAMPLE_BRANCH_ANY_RETURN = 1U << 5,
  PERF_SAMPLE_BRANCH_IND_CALL = 1U << 6,
  PERF_SAMPLE_BRANCH_ABORT_TX = 1U << 7,
  PERF_SAMPLE_BRANCH_IN_TX = 1U << 8,
  /* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  PERF_SAMPLE_BRANCH_NO_TX = 1U << 9,
  PERF_SAMPLE_BRANCH_COND = 1U << 10,
  PERF_SAMPLE_BRANCH_MAX = 1U << 11,
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define PERF_SAMPLE_BRANCH_PLM_ALL \
  (PERF_SAMPLE_BRANCH_USER | PERF_SAMPLE_BRANCH_KERNEL | PERF_SAMPLE_BRANCH_HV)
enum perf_sample_regs_abi {
  PERF_SAMPLE_REGS_ABI_NONE = 0,
  PERF_SAMPLE_REGS_ABI_32 = 1,
  /* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  PERF_SAMPLE_REGS_ABI_64 = 2,
};
enum {
  PERF_TXN_ELISION = (1 << 0),
  /* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  PERF_TXN_TRANSACTION = (1 << 1),
  PERF_TXN_SYNC = (1 << 2),
  PERF_TXN_ASYNC = (1 << 3),
  PERF_TXN_RETRY = (1 << 4),
  /* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  PERF_TXN_CONFLICT = (1 << 5),
  PERF_TXN_CAPACITY_WRITE = (1 << 6),
  PERF_TXN_CAPACITY_READ = (1 << 7),
  PERF_TXN_MAX = (1 << 8),
  /* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  PERF_TXN_ABORT_MASK = (0xffffffffULL << 32),
  PERF_TXN_ABORT_SHIFT = 32,
};
enum perf_event_read_format {
  /* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  PERF_FORMAT_TOTAL_TIME_ENABLED = 1U << 0,
  PERF_FORMAT_TOTAL_TIME_RUNNING = 1U << 1,
  PERF_FORMAT_ID = 1U << 2,
  PERF_FORMAT_GROUP = 1U << 3,
  /* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  PERF_FORMAT_MAX = 1U << 4,
};
#define PERF_ATTR_SIZE_VER0 64
#define PERF_ATTR_SIZE_VER1 72
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define PERF_ATTR_SIZE_VER2 80
#define PERF_ATTR_SIZE_VER3 96
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
#define perf_flags(attr) (*(&(attr)->read_format + 1))
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define PERF_EVENT_IOC_ENABLE _IO('$', 0)
#define PERF_EVENT_IOC_DISABLE _IO('$', 1)
#define PERF_EVENT_IOC_REFRESH _IO('$', 2)
#define PERF_EVENT_IOC_RESET _IO('$', 3)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define PERF_EVENT_IOC_PERIOD _IOW('$', 4, __u64)
#define PERF_EVENT_IOC_SET_OUTPUT _IO('$', 5)
#define PERF_EVENT_IOC_SET_FILTER _IOW('$', 6, char*)
#define PERF_EVENT_IOC_ID _IOR('$', 7, __u64*)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
enum perf_event_ioc_flags {
  PERF_IOC_FLAG_GROUP = 1U << 0,
};
struct perf_event_mmap_page {
  /* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u32 version;
  __u32 compat_version;
  __u32 lock;
  __u32 index;
  /* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __s64 offset;
  __u64 time_enabled;
  __u64 time_running;
  union {
    /* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
    __u64 capabilities;
    struct {
      __u64 cap_bit0 : 1, cap_bit0_is_deprecated : 1, cap_user_rdpmc : 1, cap_user_time : 1,
          cap_user_time_zero : 1, cap_____res : 59;
    };
    /* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  };
  __u16 pmc_width;
  __u16 time_shift;
  __u32 time_mult;
  /* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u64 time_offset;
  __u64 time_zero;
  __u32 size;
  __u8 __reserved[118 * 8 + 4];
  /* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u64 data_head;
  __u64 data_tail;
};
#define PERF_RECORD_MISC_CPUMODE_MASK (7 << 0)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define PERF_RECORD_MISC_CPUMODE_UNKNOWN (0 << 0)
#define PERF_RECORD_MISC_KERNEL (1 << 0)
#define PERF_RECORD_MISC_USER (2 << 0)
#define PERF_RECORD_MISC_HYPERVISOR (3 << 0)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define PERF_RECORD_MISC_GUEST_KERNEL (4 << 0)
#define PERF_RECORD_MISC_GUEST_USER (5 << 0)
#define PERF_RECORD_MISC_MMAP_DATA (1 << 13)
#define PERF_RECORD_MISC_COMM_EXEC (1 << 13)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define PERF_RECORD_MISC_EXACT_IP (1 << 14)
#define PERF_RECORD_MISC_EXT_RESERVED (1 << 15)
struct perf_event_header {
  __u32 type;
  /* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u16 misc;
  __u16 size;
};
enum perf_event_type {
  /* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  PERF_RECORD_MMAP = 1,
  PERF_RECORD_LOST = 2,
  PERF_RECORD_COMM = 3,
  PERF_RECORD_EXIT = 4,
  /* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  PERF_RECORD_THROTTLE = 5,
  PERF_RECORD_UNTHROTTLE = 6,
  PERF_RECORD_FORK = 7,
  PERF_RECORD_READ = 8,
  /* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  PERF_RECORD_SAMPLE = 9,
  PERF_RECORD_MMAP2 = 10,
  PERF_RECORD_MAX,
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define PERF_MAX_STACK_DEPTH 127
enum perf_callchain_context {
  PERF_CONTEXT_HV = (__u64)-32,
  PERF_CONTEXT_KERNEL = (__u64)-128,
  /* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  PERF_CONTEXT_USER = (__u64)-512,
  PERF_CONTEXT_GUEST = (__u64)-2048,
  PERF_CONTEXT_GUEST_KERNEL = (__u64)-2176,
  PERF_CONTEXT_GUEST_USER = (__u64)-2560,
  /* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  PERF_CONTEXT_MAX = (__u64)-4095,
};
#define PERF_FLAG_FD_NO_GROUP (1UL << 0)
#define PERF_FLAG_FD_OUTPUT (1UL << 1)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define PERF_FLAG_PID_CGROUP (1UL << 2)
#define PERF_FLAG_FD_CLOEXEC (1UL << 3)
union perf_mem_data_src {
  __u64 val;
  /* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  struct {
    __u64 mem_op : 5, mem_lvl : 14, mem_snoop : 5, mem_lock : 2, mem_dtlb : 7, mem_rsvd : 31;
  };
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define PERF_MEM_OP_NA 0x01
#define PERF_MEM_OP_LOAD 0x02
#define PERF_MEM_OP_STORE 0x04
#define PERF_MEM_OP_PFETCH 0x08
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define PERF_MEM_OP_EXEC 0x10
#define PERF_MEM_OP_SHIFT 0
#define PERF_MEM_LVL_NA 0x01
#define PERF_MEM_LVL_HIT 0x02
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define PERF_MEM_LVL_MISS 0x04
#define PERF_MEM_LVL_L1 0x08
#define PERF_MEM_LVL_LFB 0x10
#define PERF_MEM_LVL_L2 0x20
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define PERF_MEM_LVL_L3 0x40
#define PERF_MEM_LVL_LOC_RAM 0x80
#define PERF_MEM_LVL_REM_RAM1 0x100
#define PERF_MEM_LVL_REM_RAM2 0x200
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define PERF_MEM_LVL_REM_CCE1 0x400
#define PERF_MEM_LVL_REM_CCE2 0x800
#define PERF_MEM_LVL_IO 0x1000
#define PERF_MEM_LVL_UNC 0x2000
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define PERF_MEM_LVL_SHIFT 5
#define PERF_MEM_SNOOP_NA 0x01
#define PERF_MEM_SNOOP_NONE 0x02
#define PERF_MEM_SNOOP_HIT 0x04
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define PERF_MEM_SNOOP_MISS 0x08
#define PERF_MEM_SNOOP_HITM 0x10
#define PERF_MEM_SNOOP_SHIFT 19
#define PERF_MEM_LOCK_NA 0x01
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define PERF_MEM_LOCK_LOCKED 0x02
#define PERF_MEM_LOCK_SHIFT 24
#define PERF_MEM_TLB_NA 0x01
#define PERF_MEM_TLB_HIT 0x02
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define PERF_MEM_TLB_MISS 0x04
#define PERF_MEM_TLB_L1 0x08
#define PERF_MEM_TLB_L2 0x10
#define PERF_MEM_TLB_WK 0x20
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define PERF_MEM_TLB_OS 0x40
#define PERF_MEM_TLB_SHIFT 26
#define PERF_MEM_S(a, s) (((__u64)PERF_MEM_##a##_##s) << PERF_MEM_##a##_SHIFT)
struct perf_branch_entry {
  /* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u64 from;
  __u64 to;
  __u64 mispred : 1, predicted : 1, in_tx : 1, abort : 1, reserved : 60;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#endif
