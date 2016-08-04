#include "libufdt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define N 1000000
#define LEN 50

char names[N][LEN + 1];
struct ufdt_node nodes[N];
struct ufdt_node_dict *ht;

int main(int argc, char **argv) {
  srand(514514);
  int n = N, len = LEN;
  if (argc > 1) n = atoi(argv[1]);
  if (argc > 2) len = atoi(argv[2]);
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < len; ++j) {
      char c = rand() % 96 + 32;
      names[i][j] = c;
    }
    nodes[i].name = names[i];
    ht = ufdt_node_dict_add(ht, nodes + i);
  }
  printf("Hash table test completed\n");
  return 0;
}
