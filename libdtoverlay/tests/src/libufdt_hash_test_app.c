#include "libfdt.h"
#include "libufdt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define N 1000000
#define LEN 50

char names[N][LEN + 1];
struct fdt_prop_ufdt_node nodes[N];
struct ufdt_node_dict ht;

int main(int argc, char **argv) {
  srand(514514);
  int n = N, len = LEN;
  if (argc > 1) n = atoi(argv[1]);
  if (argc > 2) len = atoi(argv[2]);
  struct fdt_property fdt_prop;
  fdt_prop.tag = cpu_to_fdt32(FDT_PROP);
  ht = ufdt_node_dict_construct();
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < len; ++j) {
      char c = rand() % 96 + 32;
      names[i][j] = c;
    }
    nodes[i].parent.fdt_tag_ptr = &fdt_prop;
    nodes[i].parent.sibling = NULL;
    nodes[i].name = names[i];
    int err = ufdt_node_dict_add(&ht, nodes + i);
    if (err < 0) break;
  }
  printf("Hash table test completed\n");
  return 0;
}
