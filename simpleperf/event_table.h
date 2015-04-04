static Event cpu_cycles_event{"cpu-cycles", PERF_TYPE_HARDWARE,
  PERF_COUNT_HW_CPU_CYCLES};
static Event instructions_event{"instructions", PERF_TYPE_HARDWARE,
  PERF_COUNT_HW_INSTRUCTIONS};
static Event cache_references_event{"cache-references", PERF_TYPE_HARDWARE,
  PERF_COUNT_HW_CACHE_REFERENCES};
static Event cache_misses_event{"cache-misses", PERF_TYPE_HARDWARE,
  PERF_COUNT_HW_CACHE_MISSES};
static Event branch_instructions_event{"branch-instructions", PERF_TYPE_HARDWARE,
  PERF_COUNT_HW_BRANCH_INSTRUCTIONS};
static Event branch_misses_event{"branch-misses", PERF_TYPE_HARDWARE,
  PERF_COUNT_HW_BRANCH_MISSES};
static Event bus_cycles_event{"bus-cycles", PERF_TYPE_HARDWARE,
  PERF_COUNT_HW_BUS_CYCLES};
static Event stalled_cycles_frontend_event{"stalled-cycles-frontend", PERF_TYPE_HARDWARE,
  PERF_COUNT_HW_STALLED_CYCLES_FRONTEND};
static Event stalled_cycles_backend_event{"stalled-cycles-backend", PERF_TYPE_HARDWARE,
  PERF_COUNT_HW_STALLED_CYCLES_BACKEND};

static std::vector<const Event*> hardware_events{
                                                 &cpu_cycles_event,
                                                 &instructions_event,
                                                 &cache_references_event,
                                                 &cache_misses_event,
                                                 &branch_instructions_event,
                                                 &branch_misses_event,
                                                 &bus_cycles_event,
                                                 &stalled_cycles_frontend_event,
                                                 &stalled_cycles_backend_event,
                                                };


static Event cpu_clock_event{"cpu-clock", PERF_TYPE_SOFTWARE,
  PERF_COUNT_SW_CPU_CLOCK};
static Event task_clock_event{"task-clock", PERF_TYPE_SOFTWARE,
  PERF_COUNT_SW_TASK_CLOCK};
static Event page_faults_event{"page-faults", PERF_TYPE_SOFTWARE,
  PERF_COUNT_SW_PAGE_FAULTS};
static Event context_switches_event{"context-switches", PERF_TYPE_SOFTWARE,
  PERF_COUNT_SW_CONTEXT_SWITCHES};
static Event cpu_migrations_event{"cpu-migrations", PERF_TYPE_SOFTWARE,
  PERF_COUNT_SW_CPU_MIGRATIONS};
static Event minor_faults_event{"minor-faults", PERF_TYPE_SOFTWARE,
  PERF_COUNT_SW_PAGE_FAULTS_MIN};
static Event major_faults_event{"major-faults", PERF_TYPE_SOFTWARE,
  PERF_COUNT_SW_PAGE_FAULTS_MAJ};
static Event alignment_faults_event{"alignment-faults", PERF_TYPE_SOFTWARE,
  PERF_COUNT_SW_ALIGNMENT_FAULTS};
static Event emulation_faults_event{"emulation-faults", PERF_TYPE_SOFTWARE,
  PERF_COUNT_SW_EMULATION_FAULTS};

static std::vector<const Event*> software_events{
                                                 &cpu_clock_event,
                                                 &task_clock_event,
                                                 &page_faults_event,
                                                 &context_switches_event,
                                                 &cpu_migrations_event,
                                                 &minor_faults_event,
                                                 &major_faults_event,
                                                 &alignment_faults_event,
                                                 &emulation_faults_event,
                                                };


static Event l1_dcache_loades_event{"L1-dcache-loades", PERF_TYPE_HW_CACHE,
  ((PERF_COUNT_HW_CACHE_L1D) | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))};
static Event l1_dcache_load_misses_event{"L1-dcache-load-misses", PERF_TYPE_HW_CACHE,
  ((PERF_COUNT_HW_CACHE_L1D) | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))};
static Event l1_dcache_stores_event{"L1-dcache-stores", PERF_TYPE_HW_CACHE,
  ((PERF_COUNT_HW_CACHE_L1D) | (PERF_COUNT_HW_CACHE_OP_WRITE << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))};
static Event l1_dcache_store_misses_event{"L1-dcache-store-misses", PERF_TYPE_HW_CACHE,
  ((PERF_COUNT_HW_CACHE_L1D) | (PERF_COUNT_HW_CACHE_OP_WRITE << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))};
static Event l1_dcache_prefetches_event{"L1-dcache-prefetches", PERF_TYPE_HW_CACHE,
  ((PERF_COUNT_HW_CACHE_L1D) | (PERF_COUNT_HW_CACHE_OP_PREFETCH << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))};
static Event l1_dcache_prefetch_misses_event{"L1-dcache-prefetch-misses", PERF_TYPE_HW_CACHE,
  ((PERF_COUNT_HW_CACHE_L1D) | (PERF_COUNT_HW_CACHE_OP_PREFETCH << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))};
static Event l1_icache_loades_event{"L1-icache-loades", PERF_TYPE_HW_CACHE,
  ((PERF_COUNT_HW_CACHE_L1I) | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))};
static Event l1_icache_load_misses_event{"L1-icache-load-misses", PERF_TYPE_HW_CACHE,
  ((PERF_COUNT_HW_CACHE_L1I) | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))};
static Event l1_icache_stores_event{"L1-icache-stores", PERF_TYPE_HW_CACHE,
  ((PERF_COUNT_HW_CACHE_L1I) | (PERF_COUNT_HW_CACHE_OP_WRITE << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))};
static Event l1_icache_store_misses_event{"L1-icache-store-misses", PERF_TYPE_HW_CACHE,
  ((PERF_COUNT_HW_CACHE_L1I) | (PERF_COUNT_HW_CACHE_OP_WRITE << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))};
static Event l1_icache_prefetches_event{"L1-icache-prefetches", PERF_TYPE_HW_CACHE,
  ((PERF_COUNT_HW_CACHE_L1I) | (PERF_COUNT_HW_CACHE_OP_PREFETCH << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))};
static Event l1_icache_prefetch_misses_event{"L1-icache-prefetch-misses", PERF_TYPE_HW_CACHE,
  ((PERF_COUNT_HW_CACHE_L1I) | (PERF_COUNT_HW_CACHE_OP_PREFETCH << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))};
static Event llc_loades_event{"LLC-loades", PERF_TYPE_HW_CACHE,
  ((PERF_COUNT_HW_CACHE_LL) | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))};
static Event llc_load_misses_event{"LLC-load-misses", PERF_TYPE_HW_CACHE,
  ((PERF_COUNT_HW_CACHE_LL) | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))};
static Event llc_stores_event{"LLC-stores", PERF_TYPE_HW_CACHE,
  ((PERF_COUNT_HW_CACHE_LL) | (PERF_COUNT_HW_CACHE_OP_WRITE << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))};
static Event llc_store_misses_event{"LLC-store-misses", PERF_TYPE_HW_CACHE,
  ((PERF_COUNT_HW_CACHE_LL) | (PERF_COUNT_HW_CACHE_OP_WRITE << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))};
static Event llc_prefetches_event{"LLC-prefetches", PERF_TYPE_HW_CACHE,
  ((PERF_COUNT_HW_CACHE_LL) | (PERF_COUNT_HW_CACHE_OP_PREFETCH << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))};
static Event llc_prefetch_misses_event{"LLC-prefetch-misses", PERF_TYPE_HW_CACHE,
  ((PERF_COUNT_HW_CACHE_LL) | (PERF_COUNT_HW_CACHE_OP_PREFETCH << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))};
static Event dtlb_loades_event{"dTLB-loades", PERF_TYPE_HW_CACHE,
  ((PERF_COUNT_HW_CACHE_DTLB) | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))};
static Event dtlb_load_misses_event{"dTLB-load-misses", PERF_TYPE_HW_CACHE,
  ((PERF_COUNT_HW_CACHE_DTLB) | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))};
static Event dtlb_stores_event{"dTLB-stores", PERF_TYPE_HW_CACHE,
  ((PERF_COUNT_HW_CACHE_DTLB) | (PERF_COUNT_HW_CACHE_OP_WRITE << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))};
static Event dtlb_store_misses_event{"dTLB-store-misses", PERF_TYPE_HW_CACHE,
  ((PERF_COUNT_HW_CACHE_DTLB) | (PERF_COUNT_HW_CACHE_OP_WRITE << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))};
static Event dtlb_prefetches_event{"dTLB-prefetches", PERF_TYPE_HW_CACHE,
  ((PERF_COUNT_HW_CACHE_DTLB) | (PERF_COUNT_HW_CACHE_OP_PREFETCH << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))};
static Event dtlb_prefetch_misses_event{"dTLB-prefetch-misses", PERF_TYPE_HW_CACHE,
  ((PERF_COUNT_HW_CACHE_DTLB) | (PERF_COUNT_HW_CACHE_OP_PREFETCH << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))};
static Event itlb_loades_event{"iTLB-loades", PERF_TYPE_HW_CACHE,
  ((PERF_COUNT_HW_CACHE_ITLB) | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))};
static Event itlb_load_misses_event{"iTLB-load-misses", PERF_TYPE_HW_CACHE,
  ((PERF_COUNT_HW_CACHE_ITLB) | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))};
static Event itlb_stores_event{"iTLB-stores", PERF_TYPE_HW_CACHE,
  ((PERF_COUNT_HW_CACHE_ITLB) | (PERF_COUNT_HW_CACHE_OP_WRITE << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))};
static Event itlb_store_misses_event{"iTLB-store-misses", PERF_TYPE_HW_CACHE,
  ((PERF_COUNT_HW_CACHE_ITLB) | (PERF_COUNT_HW_CACHE_OP_WRITE << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))};
static Event itlb_prefetches_event{"iTLB-prefetches", PERF_TYPE_HW_CACHE,
  ((PERF_COUNT_HW_CACHE_ITLB) | (PERF_COUNT_HW_CACHE_OP_PREFETCH << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))};
static Event itlb_prefetch_misses_event{"iTLB-prefetch-misses", PERF_TYPE_HW_CACHE,
  ((PERF_COUNT_HW_CACHE_ITLB) | (PERF_COUNT_HW_CACHE_OP_PREFETCH << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))};
static Event branch_loades_event{"branch-loades", PERF_TYPE_HW_CACHE,
  ((PERF_COUNT_HW_CACHE_BPU) | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))};
static Event branch_load_misses_event{"branch-load-misses", PERF_TYPE_HW_CACHE,
  ((PERF_COUNT_HW_CACHE_BPU) | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))};
static Event branch_stores_event{"branch-stores", PERF_TYPE_HW_CACHE,
  ((PERF_COUNT_HW_CACHE_BPU) | (PERF_COUNT_HW_CACHE_OP_WRITE << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))};
static Event branch_store_misses_event{"branch-store-misses", PERF_TYPE_HW_CACHE,
  ((PERF_COUNT_HW_CACHE_BPU) | (PERF_COUNT_HW_CACHE_OP_WRITE << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))};
static Event branch_prefetches_event{"branch-prefetches", PERF_TYPE_HW_CACHE,
  ((PERF_COUNT_HW_CACHE_BPU) | (PERF_COUNT_HW_CACHE_OP_PREFETCH << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))};
static Event branch_prefetch_misses_event{"branch-prefetch-misses", PERF_TYPE_HW_CACHE,
  ((PERF_COUNT_HW_CACHE_BPU) | (PERF_COUNT_HW_CACHE_OP_PREFETCH << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))};
static Event node_loades_event{"node-loades", PERF_TYPE_HW_CACHE,
  ((PERF_COUNT_HW_CACHE_NODE) | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))};
static Event node_load_misses_event{"node-load-misses", PERF_TYPE_HW_CACHE,
  ((PERF_COUNT_HW_CACHE_NODE) | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))};
static Event node_stores_event{"node-stores", PERF_TYPE_HW_CACHE,
  ((PERF_COUNT_HW_CACHE_NODE) | (PERF_COUNT_HW_CACHE_OP_WRITE << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))};
static Event node_store_misses_event{"node-store-misses", PERF_TYPE_HW_CACHE,
  ((PERF_COUNT_HW_CACHE_NODE) | (PERF_COUNT_HW_CACHE_OP_WRITE << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))};
static Event node_prefetches_event{"node-prefetches", PERF_TYPE_HW_CACHE,
  ((PERF_COUNT_HW_CACHE_NODE) | (PERF_COUNT_HW_CACHE_OP_PREFETCH << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))};
static Event node_prefetch_misses_event{"node-prefetch-misses", PERF_TYPE_HW_CACHE,
  ((PERF_COUNT_HW_CACHE_NODE) | (PERF_COUNT_HW_CACHE_OP_PREFETCH << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))};

static std::vector<const Event*> hwcache_events{
                                                &l1_dcache_loades_event,
                                                &l1_dcache_load_misses_event,
                                                &l1_dcache_stores_event,
                                                &l1_dcache_store_misses_event,
                                                &l1_dcache_prefetches_event,
                                                &l1_dcache_prefetch_misses_event,
                                                &l1_icache_loades_event,
                                                &l1_icache_load_misses_event,
                                                &l1_icache_stores_event,
                                                &l1_icache_store_misses_event,
                                                &l1_icache_prefetches_event,
                                                &l1_icache_prefetch_misses_event,
                                                &llc_loades_event,
                                                &llc_load_misses_event,
                                                &llc_stores_event,
                                                &llc_store_misses_event,
                                                &llc_prefetches_event,
                                                &llc_prefetch_misses_event,
                                                &dtlb_loades_event,
                                                &dtlb_load_misses_event,
                                                &dtlb_stores_event,
                                                &dtlb_store_misses_event,
                                                &dtlb_prefetches_event,
                                                &dtlb_prefetch_misses_event,
                                                &itlb_loades_event,
                                                &itlb_load_misses_event,
                                                &itlb_stores_event,
                                                &itlb_store_misses_event,
                                                &itlb_prefetches_event,
                                                &itlb_prefetch_misses_event,
                                                &branch_loades_event,
                                                &branch_load_misses_event,
                                                &branch_stores_event,
                                                &branch_store_misses_event,
                                                &branch_prefetches_event,
                                                &branch_prefetch_misses_event,
                                                &node_loades_event,
                                                &node_load_misses_event,
                                                &node_stores_event,
                                                &node_store_misses_event,
                                                &node_prefetches_event,
                                                &node_prefetch_misses_event,
                                               };



