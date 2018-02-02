/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <linux/in.h>
#include <linux/unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#define ptr_to_u64(x) ((uint64_t)(uintptr_t)x)
#define DEFAULT_LOG_LEVEL 1

/* instruction set for bpf program */

#define REG_ALU64(OP) (BPF_ALU64 | BPF_OP(OP) | BPF_X)
#define REG_ALU32(OP) (BPF_ALU | BPF_OP(OP) | BPF_X)
#define VAL_ALU64(OP) (BPF_ALU64 | BPF_OP(OP) | BPF_K)
#define VAL_ALU32(OP) (BPF_ALU | BPF_OP(OP) | BPF_K)
#define REG_MOV64 (BPF_ALU64 | BPF_MOV | BPF_X)
#define REG_MOV32 (BPF_ALU | BPF_MOV | BPF_X)
#define VAL_MOV64 (BPF_ALU64 | BPF_MOV | BPF_K)
#define VAL_MOV32 (BPF_ALU | BPF_MOV | BPF_K)
#define REG_ATOMIC_ADD(SIZE) (BPF_STX | BPF_SIZE(SIZE) | BPF_XADD)
#define SKB_LD(SIZE) (BPF_LD | BPF_SIZE(SIZE) | BPF_ABS)
#define MEM_LD(SIZE) (BPF_LDX | BPF_SIZE(SIZE) | BPF_MEM)
#define MEM_SET_BY_REG(SIZE) (BPF_STX | BPF_SIZE(SIZE) | BPF_MEM)
#define MEM_SET_BY_VAL(SIZE) (BPF_ST | BPF_SIZE(SIZE) | BPF_MEM)
#define REG_ALU_JMP(OP) (BPF_JMP | BPF_OP(OP) | BPF_X)
#define VAL_ALU_JMP(OP) (BPF_JMP | BPF_OP(OP) | BPF_K)
#define PROG_EXIT (BPF_JMP | BPF_EXIT)

/* Raw code statement block */

#define BPF_INS_BLK(CODE, DST, SRC, OFF, IMM) \
    ((struct bpf_insn){.code = CODE, .dst_reg = DST, .src_reg = SRC, .off = OFF, .imm = IMM})

#ifndef BPF_PSEUDO_MAP_FD
#define BPF_PSEUDO_MAP_FD 1
#endif

#define LOAD_MAP_FD(DST, MAP_FD)                                                                 \
    BPF_INS_BLK(BPF_LD | BPF_DW | BPF_IMM, DST, BPF_PSEUDO_MAP_FD, 0, (__s32)((__u32)(MAP_FD))), \
        BPF_INS_BLK(0, 0, 0, 0, (__s32)(((__u64)(MAP_FD)) >> 32))

