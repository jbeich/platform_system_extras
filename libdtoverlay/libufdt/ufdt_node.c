
#include "libufdt.h"
#include "ufdt_util.h"

int node_cmp(const void *a, const void *b) {
  const struct ufdt_node *na = *(struct ufdt_node **)a;
  const struct ufdt_node *nb = *(struct ufdt_node **)b;
  return dto_strcmp(na->name, nb->name);
}

bool node_name_eq(const struct ufdt_node *node, const char *name, int len) {
  if (!node) return false;
  if (!name) return false;
  if (dto_strncmp(node->name, name, len) != 0) return false;
  if (node->name[len] != '\0') return false;
  return true;
}

/*
 * ufdt_node methods.
 */

struct ufdt_node *ufdt_node_construct(void *fdtp, fdt32_t *fdt_tag_ptr) {
  struct ufdt_node *res = dto_malloc(sizeof(*res));
  if (res == NULL) return NULL;
  res->fdt_tag_ptr = fdt_tag_ptr;
  res->name = name_of(fdtp, res);
  res->prop_dict = NULL;
  res->node_dict = NULL;
  return res;
}

void ufdt_node_destruct(struct ufdt_node *node) {
  if (node == NULL) return;

  struct ufdt_node **it;
  for_each_prop(it, node) ufdt_node_destruct(*it);
  for_each_node(it, node) ufdt_node_destruct(*it);

  ufdt_node_dict_destruct(node->prop_dict);
  ufdt_node_dict_destruct(node->node_dict);

  dto_free(node);

  return;
}

void ufdt_node_add_child(struct ufdt_node *parent, struct ufdt_node *child) {
  if (!parent || !child) return;

  uint32_t child_tag = tag_of(child);

  switch (child_tag) {
    case FDT_PROP:
      parent->prop_dict = ufdt_node_dict_add(parent->prop_dict, child);
      break;

    case FDT_BEGIN_NODE:
      parent->node_dict = ufdt_node_dict_add(parent->node_dict, child);
      break;

    default:
      dto_error("invalid children tag type\n");
  }

  return;
}

/*
 * BEGIN of FDT_PROP related methods.
 */

struct ufdt_node *ufdt_node_get_property_by_name_len(
    const struct ufdt_node *node, const char *name, int len) {
  if (!node) return NULL;
  return ufdt_node_dict_find_node_len(node->prop_dict, name, len);
}

struct ufdt_node *ufdt_node_get_property_by_name(const struct ufdt_node *node,
                                                 const char *name) {
  return ufdt_node_get_property_by_name_len(node, name, dto_strlen(name));
}

void *ufdt_node_get_fdt_prop_data(const struct ufdt_node *node, int *out_len) {
  if (!node || tag_of(node) != FDT_PROP) {
    return NULL;
  }
  const struct fdt_property *prop;
  prop = node->fdt_tag_ptr;
  if (out_len != NULL) {
    *out_len = fdt32_to_cpu(prop->len);
  }
  return (void *)prop->data;
}

void *ufdt_node_get_fdt_prop_data_by_name_len(const struct ufdt_node *node,
                                              const char *name, int len,
                                              int *out_len) {
  return ufdt_node_get_fdt_prop_data(
      ufdt_node_get_property_by_name_len(node, name, len), out_len);
}

void *ufdt_node_get_fdt_prop_data_by_name(const struct ufdt_node *node,
                                          const char *name, int *out_len) {
  return ufdt_node_get_fdt_prop_data(ufdt_node_get_property_by_name(node, name),
                                     out_len);
}

/*
 * END of FDT_PROP related methods.
 */

/*
 * BEGIN of searching-in-ufdt_node methods.
 */

uint32_t ufdt_node_get_phandle(const struct ufdt_node *node) {
  if (!node || tag_of(node) != FDT_BEGIN_NODE) {
    return 0;
  }
  int len = 0;
  void *ptr = ufdt_node_get_fdt_prop_data_by_name(node, "phandle", &len);
  if (!ptr || len != sizeof(fdt32_t)) {
    ptr = ufdt_node_get_fdt_prop_data_by_name(node, "linux,phandle", &len);
    if (!ptr || len != sizeof(fdt32_t)) {
      return 0;
    }
  }
  return fdt32_to_cpu(*((fdt32_t *)ptr));
}

struct ufdt_node *ufdt_node_get_node_by_path_len(const struct ufdt_node *node,
                                                 const char *path, int len) {
  const char *end = path + len;

  struct ufdt_node *cur = node;

  while (path < end) {
    while (path[0] == '/') path++;
    if (path == end) return cur;

    const char *next_slash;
    next_slash = dto_memchr(path, '/', end - path);
    if (!next_slash) next_slash = end;

    struct ufdt_node *next = NULL;

    next =
        ufdt_node_dict_find_node_len(cur->node_dict, path, next_slash - path);

    cur = next;
    path = next_slash;
    if (!cur) return cur;
  }

  return cur;
}

struct ufdt_node *ufdt_node_get_node_by_path(const struct ufdt_node *node,
                                             const char *path) {
  return ufdt_node_get_node_by_path_len(node, path, dto_strlen(path));
}

/*
 * END of searching-in-ufdt_node methods.
 */

int output_property_to_fdt(struct ufdt_node *prop_node, void *fdtp,
                           struct ufdt_node_dict *props_dict) {
  int err = 0;
  int len = 0;
  void *data = ufdt_node_get_fdt_prop_data(prop_node, &len);
  int nameoff = 0;
  struct ufdt_node *same_name_prop =
      ufdt_node_dict_find_node(props_dict, prop_node->name);

  /*
   * There's already a property with same name, take its nameoff instead.
   */
  if (same_name_prop != NULL) {
    const struct fdt_property *prop =
        (const struct fdt_property *)same_name_prop->fdt_tag_ptr;
    nameoff = fdt32_to_cpu(prop->nameoff);
  }
  /*
   * Modifies prop_node->fdt_tag_ptr to point to the node in new fdtp.
   */
  err = fast_fdt_sw_property(fdtp, prop_node->name, data, len, &nameoff,
                             &(prop_node->fdt_tag_ptr));

  if (err < 0) {
    dto_error("Not enough space for the string space: %d\n",
              fdt_size_dt_strings(fdtp));
  }
  ufdt_node_dict_add(props_dict, prop_node);
  return err;
}

int output_ufdt_node_to_fdt(struct ufdt_node *node, void *fdtp,
                   struct ufdt_node_dict *props_dict) {
  uint32_t tag = tag_of(node);

  if (tag == FDT_PROP) {
    int err = output_property_to_fdt(node, fdtp, props_dict);
    return err;
  }

  int err = fdt_begin_node(fdtp, node->name);
  if (err < 0) return -1;

  struct ufdt_node **it;
  for_each_prop(it, node) {
    err = output_ufdt_node_to_fdt(*it, fdtp, props_dict);
    if (err < 0) return -1;
  }

  for_each_node(it, node) {
    err = output_ufdt_node_to_fdt(*it, fdtp, props_dict);
    if (err < 0) return -1;
  }

  err = fdt_end_node(fdtp);
  if (err < 0) return -1;

  return 0;
}

#define TAB_SIZE 2

void ufdt_node_print(const struct ufdt_node *node, int depth) {
  if (!node) return;

  int i;
  for (i = 0; i < depth * TAB_SIZE; i++) dto_print(" ");

  uint32_t tag;
  tag = tag_of(node);

  switch (tag) {
    case FDT_BEGIN_NODE:
      dto_print("NODE ");
      break;
    case FDT_PROP:
      dto_print("PROP ");
      break;
    default:
      dto_print("UNKNOWN ");
      break;
  }

  if (node->name) {
    dto_print(":%s:\n", node->name);
  } else {
    dto_print("node name is NULL.\n");
  }

  struct ufdt_node **it;

  for_each_prop(it, node) ufdt_node_print(*it, depth + 1);

  for_each_node(it, node) ufdt_node_print(*it, depth + 1);

  return;
}

void ufdt_node_map(struct ufdt_node *node, struct ufdt_node_closure closure) {
  if (node == NULL) return;
  closure.func(node, closure.env);
  struct ufdt_node **it;
  for_each_prop(it, node) ufdt_node_map(*it, closure);
  for_each_node(it, node) ufdt_node_map(*it, closure);
  return;
}
