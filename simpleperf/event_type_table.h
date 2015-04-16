// This file is auto-generated by generate-event_table.py.

{"cpu-cycles", PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES},
{"instructions", PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS},
{"cache-references", PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_REFERENCES},
{"cache-misses", PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_MISSES},
{"branch-instructions", PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_INSTRUCTIONS},
{"branch-misses", PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_MISSES},
{"bus-cycles", PERF_TYPE_HARDWARE, PERF_COUNT_HW_BUS_CYCLES},
{"stalled-cycles-frontend", PERF_TYPE_HARDWARE, PERF_COUNT_HW_STALLED_CYCLES_FRONTEND},
{"stalled-cycles-backend", PERF_TYPE_HARDWARE, PERF_COUNT_HW_STALLED_CYCLES_BACKEND},

{"cpu-clock", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CPU_CLOCK},
{"task-clock", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_TASK_CLOCK},
{"page-faults", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS},
{"context-switches", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CONTEXT_SWITCHES},
{"cpu-migrations", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CPU_MIGRATIONS},
{"minor-faults", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS_MIN},
{"major-faults", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS_MAJ},
{"alignment-faults", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_ALIGNMENT_FAULTS},
{"emulation-faults", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_EMULATION_FAULTS},

{"L1-dcache-loades", PERF_TYPE_HW_CACHE, ((PERF_COUNT_HW_CACHE_L1D) | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))},
{"L1-dcache-load-misses", PERF_TYPE_HW_CACHE, ((PERF_COUNT_HW_CACHE_L1D) | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))},
{"L1-dcache-stores", PERF_TYPE_HW_CACHE, ((PERF_COUNT_HW_CACHE_L1D) | (PERF_COUNT_HW_CACHE_OP_WRITE << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))},
{"L1-dcache-store-misses", PERF_TYPE_HW_CACHE, ((PERF_COUNT_HW_CACHE_L1D) | (PERF_COUNT_HW_CACHE_OP_WRITE << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))},
{"L1-dcache-prefetches", PERF_TYPE_HW_CACHE, ((PERF_COUNT_HW_CACHE_L1D) | (PERF_COUNT_HW_CACHE_OP_PREFETCH << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))},
{"L1-dcache-prefetch-misses", PERF_TYPE_HW_CACHE, ((PERF_COUNT_HW_CACHE_L1D) | (PERF_COUNT_HW_CACHE_OP_PREFETCH << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))},
{"L1-icache-loades", PERF_TYPE_HW_CACHE, ((PERF_COUNT_HW_CACHE_L1I) | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))},
{"L1-icache-load-misses", PERF_TYPE_HW_CACHE, ((PERF_COUNT_HW_CACHE_L1I) | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))},
{"L1-icache-stores", PERF_TYPE_HW_CACHE, ((PERF_COUNT_HW_CACHE_L1I) | (PERF_COUNT_HW_CACHE_OP_WRITE << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))},
{"L1-icache-store-misses", PERF_TYPE_HW_CACHE, ((PERF_COUNT_HW_CACHE_L1I) | (PERF_COUNT_HW_CACHE_OP_WRITE << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))},
{"L1-icache-prefetches", PERF_TYPE_HW_CACHE, ((PERF_COUNT_HW_CACHE_L1I) | (PERF_COUNT_HW_CACHE_OP_PREFETCH << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))},
{"L1-icache-prefetch-misses", PERF_TYPE_HW_CACHE, ((PERF_COUNT_HW_CACHE_L1I) | (PERF_COUNT_HW_CACHE_OP_PREFETCH << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))},
{"LLC-loades", PERF_TYPE_HW_CACHE, ((PERF_COUNT_HW_CACHE_LL) | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))},
{"LLC-load-misses", PERF_TYPE_HW_CACHE, ((PERF_COUNT_HW_CACHE_LL) | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))},
{"LLC-stores", PERF_TYPE_HW_CACHE, ((PERF_COUNT_HW_CACHE_LL) | (PERF_COUNT_HW_CACHE_OP_WRITE << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))},
{"LLC-store-misses", PERF_TYPE_HW_CACHE, ((PERF_COUNT_HW_CACHE_LL) | (PERF_COUNT_HW_CACHE_OP_WRITE << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))},
{"LLC-prefetches", PERF_TYPE_HW_CACHE, ((PERF_COUNT_HW_CACHE_LL) | (PERF_COUNT_HW_CACHE_OP_PREFETCH << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))},
{"LLC-prefetch-misses", PERF_TYPE_HW_CACHE, ((PERF_COUNT_HW_CACHE_LL) | (PERF_COUNT_HW_CACHE_OP_PREFETCH << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))},
{"dTLB-loades", PERF_TYPE_HW_CACHE, ((PERF_COUNT_HW_CACHE_DTLB) | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))},
{"dTLB-load-misses", PERF_TYPE_HW_CACHE, ((PERF_COUNT_HW_CACHE_DTLB) | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))},
{"dTLB-stores", PERF_TYPE_HW_CACHE, ((PERF_COUNT_HW_CACHE_DTLB) | (PERF_COUNT_HW_CACHE_OP_WRITE << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))},
{"dTLB-store-misses", PERF_TYPE_HW_CACHE, ((PERF_COUNT_HW_CACHE_DTLB) | (PERF_COUNT_HW_CACHE_OP_WRITE << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))},
{"dTLB-prefetches", PERF_TYPE_HW_CACHE, ((PERF_COUNT_HW_CACHE_DTLB) | (PERF_COUNT_HW_CACHE_OP_PREFETCH << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))},
{"dTLB-prefetch-misses", PERF_TYPE_HW_CACHE, ((PERF_COUNT_HW_CACHE_DTLB) | (PERF_COUNT_HW_CACHE_OP_PREFETCH << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))},
{"iTLB-loades", PERF_TYPE_HW_CACHE, ((PERF_COUNT_HW_CACHE_ITLB) | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))},
{"iTLB-load-misses", PERF_TYPE_HW_CACHE, ((PERF_COUNT_HW_CACHE_ITLB) | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))},
{"iTLB-stores", PERF_TYPE_HW_CACHE, ((PERF_COUNT_HW_CACHE_ITLB) | (PERF_COUNT_HW_CACHE_OP_WRITE << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))},
{"iTLB-store-misses", PERF_TYPE_HW_CACHE, ((PERF_COUNT_HW_CACHE_ITLB) | (PERF_COUNT_HW_CACHE_OP_WRITE << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))},
{"iTLB-prefetches", PERF_TYPE_HW_CACHE, ((PERF_COUNT_HW_CACHE_ITLB) | (PERF_COUNT_HW_CACHE_OP_PREFETCH << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))},
{"iTLB-prefetch-misses", PERF_TYPE_HW_CACHE, ((PERF_COUNT_HW_CACHE_ITLB) | (PERF_COUNT_HW_CACHE_OP_PREFETCH << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))},
{"branch-loades", PERF_TYPE_HW_CACHE, ((PERF_COUNT_HW_CACHE_BPU) | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))},
{"branch-load-misses", PERF_TYPE_HW_CACHE, ((PERF_COUNT_HW_CACHE_BPU) | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))},
{"branch-stores", PERF_TYPE_HW_CACHE, ((PERF_COUNT_HW_CACHE_BPU) | (PERF_COUNT_HW_CACHE_OP_WRITE << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))},
{"branch-store-misses", PERF_TYPE_HW_CACHE, ((PERF_COUNT_HW_CACHE_BPU) | (PERF_COUNT_HW_CACHE_OP_WRITE << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))},
{"branch-prefetches", PERF_TYPE_HW_CACHE, ((PERF_COUNT_HW_CACHE_BPU) | (PERF_COUNT_HW_CACHE_OP_PREFETCH << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))},
{"branch-prefetch-misses", PERF_TYPE_HW_CACHE, ((PERF_COUNT_HW_CACHE_BPU) | (PERF_COUNT_HW_CACHE_OP_PREFETCH << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))},
{"node-loades", PERF_TYPE_HW_CACHE, ((PERF_COUNT_HW_CACHE_NODE) | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))},
{"node-load-misses", PERF_TYPE_HW_CACHE, ((PERF_COUNT_HW_CACHE_NODE) | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))},
{"node-stores", PERF_TYPE_HW_CACHE, ((PERF_COUNT_HW_CACHE_NODE) | (PERF_COUNT_HW_CACHE_OP_WRITE << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))},
{"node-store-misses", PERF_TYPE_HW_CACHE, ((PERF_COUNT_HW_CACHE_NODE) | (PERF_COUNT_HW_CACHE_OP_WRITE << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))},
{"node-prefetches", PERF_TYPE_HW_CACHE, ((PERF_COUNT_HW_CACHE_NODE) | (PERF_COUNT_HW_CACHE_OP_PREFETCH << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))},
{"node-prefetch-misses", PERF_TYPE_HW_CACHE, ((PERF_COUNT_HW_CACHE_NODE) | (PERF_COUNT_HW_CACHE_OP_PREFETCH << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))},

