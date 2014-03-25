#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

__BEGIN_DECLS
#include <pagemap/pagemap.h>
__END_DECLS

size_t getPss(pid_t pid) {
  pm_kernel_t* kernel;
  if (pm_kernel_create(&kernel)) {
    printf("Error creating kernel interface.\n");
    return 0;
  }

  pm_process_t* process;
  if (pm_process_create(kernel, pid, &process)) {
    printf("Error creating process.\n");
    return 0;
  }

  pm_map_t** maps;
  size_t num_maps;
  if (pm_process_maps(process, &maps, &num_maps)) {
    printf("Error getting maps.\n");
    return 0;
  }

  size_t total_pss = 0;
  for (size_t i = 0; i < num_maps; i++) {
    pm_memusage_t usage;
    if (pm_map_usage(maps[i], &usage)) {
      printf("Error getting usage for a map\n");
      continue;
    }
    total_pss += usage.pss;
  }
  return total_pss;
}

int main(int argc, char** argv) {
  if (argc != 2) {
    printf("Requires the pid to dump\n");
    return 1;
  }

  size_t pss = getPss(atoi(argv[1]));
  printf("%zu %zuK\n", pss, pss/1024);
  return 0;
}
