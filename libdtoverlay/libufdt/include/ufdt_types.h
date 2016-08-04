#ifndef UFDT_TYPES_H
#define UFDT_TYPES_H

#include "libfdt.h"

#define ASCII_PRINT_S (32)
#define ASCII_PRINT_E (128)
#define ASCII_PRINT_SZ (ASCII_PRINT_E - ASCII_PRINT_S)

#define FDT_PROP_DELI ':'
#define FDT_NODE_DELI '/'

#define DTNL_INIT_SZ 4

/* Empirical values for hash functions. */
#define HASH_BASE 13131

/* it has type : struct ufdt_node** */
#define for_each(it, node_dict)                                  \
  if ((node_dict) != NULL)                                       \
    for (it = (node_dict)->nodes;                                \
         it != (node_dict)->nodes + (node_dict)->mem_size; ++it) \
      if (*it)

#define for_each_prop(it, node) for_each(it, (node)->prop_dict)

#define for_each_node(it, node) for_each(it, (node)->node_dict)

struct ufdt_node_dict;

struct ufdt_node {
  fdt32_t *fdt_tag_ptr;
  const char *name;
  struct ufdt_node_dict *node_dict;
  struct ufdt_node_dict *prop_dict;
};

struct ufdt_node_dict {
  int mem_size;
  int num_used;
  struct ufdt_node **nodes;
};

struct phandle_table_entry {
  uint32_t phandle;
  struct ufdt_node *node;
};

struct static_phandle_table {
  int len;
  struct phandle_table_entry *data;
};

struct ufdt {
  void *fdtp;
  struct ufdt_node *root;
  struct static_phandle_table phandle_table;
};

typedef void func_on_ufdt_node(struct ufdt_node *, void *);

struct ufdt_node_closure {
  func_on_ufdt_node *func;
  void *env;
};

#endif /* UFDT_TYPES_H */
