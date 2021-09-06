/** hamt-poly -- A polymorphic Hash Array Mapped Trie implementation.
Version 2.1 Sept 2021
Original copyright notice below.
My updates are given under the same Copyright licence
// Martin Josefsson

Copyright (c) 2021, James Barford-Evans
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.

This software is provided by the copyright holders and contributors
"as is" and any express or implied warranties, including, but not
limited to, the implied warranties of merchantability and fitness for
a particular purpose are disclaimed. In no event shall the copyright
owner or contributors be liable for any direct, indirect, incidental,
special, exemplary, or consequential damages (including, but not
limited to, procurement of substitute goods or services; loss of use,
data, or profits; or business interruption) however caused and on any
theory of liability, whether in contract, strict liability, or tort
(including negligence or otherwise) arising in any way out of the use
of this software, even if advised of the possibility of such damage.
*/
#ifndef HAMT_H
#define HAMT_H

enum NODE_TYPE { LEAF, BRANCH, COLLISION, ARRAY_NODE };

#define BITS     5
#define SIZE     32
#define MASK     31

/* This is arbitrary, as a collision should only have two nodes */
#define MIN_COLLISION_NODE_SIZE 8

#define MAX_BRANCH_SIZE         16
#define MIN_ARRAY_NODE_SIZE     8

/**
 * convert a string to a 32bit unsigned integer
 */
static inline unsigned int get_hash(char *str) {
  unsigned int hash = 0;
  char *ptr = str;

  while (*ptr != '\0') {
    hash = ((hash << BITS) - hash) + *(ptr++);
  }

  return hash;
}

// clang-format off
/** HAMT_DEFINE: Macro achieve polymorphism.
Your type must have a single-symbol name.
```
typedef struct MyKeyType {
  char *str;
} MyKeyType;
```
It must be able to check equality, for example when
inserting with a hash collission
```
bool mykeytype_equals(MyKeyType *s0, MyKeyType *s1) {
  bool return_value = strcmp(s0->str, s1->str) == 0;
  return return_value;
}
unsigned int get_hash_of_mykeytype(MyKeyType *s) { return get_hash(s->str); }
```
`HAMT_DEFINE` will define types & functions prefixed
with `name_hamt_`, where `name` in this example is `MyKeyType`.
```
HAMT_DEFINE(MyKeyType, get_hash_of_mykeytype, mykeytype_equals)
```
 */
// clang-format on
#define HAMT_DEFINE(name, hashof, equals)                                            \
  typedef struct name##_hamt_node {                                                  \
    enum NODE_TYPE type;                                                             \
    unsigned int hash;                                                               \
    /**                                                                              \
     * This is only used by the collision node and array_node and is a               \
     * count of the total number of children held in the node                        \
     */                                                                              \
    int bitmap;                                                                      \
    name *key;                                                                       \
    void *value;                                                                     \
    struct name##_hamt_node **children;                                              \
  } name##_hamt_node;                                                                \
                                                                                     \
  typedef struct name##_hamt {                                                       \
    name##_hamt_node *root;                                                          \
  } name##_hamt;                                                                     \
                                                                                     \
  /*======= hashing =========================*/                                      \
  /**                                                                                \
   * From Ideal hash trees Phil Bagwell, page 3                                      \
   * https://lampwww.epfl.ch/papers/idealhashtrees.pdf                               \
   * Count number of bits in a number                                                \
   */                                                                                \
  static const unsigned int name##_hamt_SK5 = 0x55555555;                            \
  static const unsigned int name##_hamt_SK3 = 0x33333333;                            \
  static const unsigned int name##_hamt_SKF0 = 0xF0F0F0F;                            \
                                                                                     \
  static inline int name##_hamt_popcount(unsigned int bits) {                        \
    bits -= ((bits >> 1) & name##_hamt_SK5);                                         \
    bits = (bits & name##_hamt_SK3) + ((bits >> 2) & name##_hamt_SK3);               \
    bits = (bits & name##_hamt_SKF0) + ((bits >> 4) & name##_hamt_SKF0);             \
    bits += bits >> 8;                                                               \
    return (bits + (bits >> 16)) & 0x3F;                                             \
  }                                                                                  \
                                                                                     \
  static inline unsigned int name##_hamt_get_mask(unsigned int frag) {               \
    return 1 << frag;                                                                \
  }                                                                                  \
                                                                                     \
  /* take 5 bits of the hash */                                                      \
  static inline unsigned int name##_hamt_get_frag(unsigned int hash,                 \
                                                  int depth) {                       \
    return ((unsigned int)hash >> (BITS * depth)) & MASK;                            \
  }                                                                                  \
                                                                                     \
  /**                                                                                \
   * Get the position in the array where the child is located                        \
   *                                                                                 \
   */                                                                                \
  static unsigned int name##_hamt_get_position(unsigned int hash,                    \
                                               unsigned int frag) {                  \
    return name##_hamt_popcount(hash & (name##_hamt_get_mask(frag) - 1));            \
  }                                                                                  \
                                                                                     \
  name##_hamt *name##_hamt_new() {                                                   \
    name##_hamt *hamt;                                                               \
                                                                                     \
    if ((hamt = (name##_hamt *)malloc(sizeof(name##_hamt))) == NULL) {               \
      fprintf(stderr, "Failed to allocate memory for hamt\n");                       \
    }                                                                                \
                                                                                     \
    hamt->root = NULL;                                                               \
    return hamt;                                                                     \
  }                                                                                  \
  /* Insertion methods  */                                                           \
  typedef struct name##_hamt_insert_instruction_t {                                  \
    name##_hamt_node *node;                                                          \
    unsigned int hash;                                                               \
    name *key;                                                                       \
    void *value;                                                                     \
    int depth;                                                                       \
  } name##_hamt_insert_instruction_t;                                                \
                                                                                     \
  static name##_hamt_node *name##_hamt_handle_collision_insert(                      \
      name##_hamt_insert_instruction_t *ins);                                        \
  static name##_hamt_node *name##_hamt_handle_branch_insert(                         \
      name##_hamt_insert_instruction_t *ins);                                        \
  static name##_hamt_node *name##_hamt_handle_leaf_insert(                           \
      name##_hamt_insert_instruction_t *ins);                                        \
  static name##_hamt_node *name##_hamt_handle_arraynode_insert(                      \
      name##_hamt_insert_instruction_t *ins);                                        \
                                                                                     \
  /* Removal methods */                                                              \
  typedef struct name##_hamt_removal_t {                                             \
    name##_hamt_node *node;                                                          \
    unsigned int hash;                                                               \
    name *key;                                                                       \
    int depth;                                                                       \
  } name##_hamt_removal_t;                                                           \
                                                                                     \
  static name##_hamt_node *name##_hamt_handle_collision_removal(                     \
      name##_hamt_removal_t *rem);                                                   \
  static name##_hamt_node *name##_hamt_handle_branch_removal(                        \
      name##_hamt_removal_t *rem);                                                   \
  static name##_hamt_node *name##_hamt_handle_leaf_removal(                          \
      name##_hamt_removal_t *rem);                                                   \
  static name##_hamt_node *name##_hamt_handle_arraynode_removal(                     \
      name##_hamt_removal_t *rem);                                                   \
                                                                                     \
  /*======= node constructors =====================*/                                \
  static name##_hamt_node *name##_hamt_create_node(                                  \
      int hash, name *key, void *value, enum NODE_TYPE type,                         \
      name##_hamt_node **children, unsigned long bitmap) {                           \
    name##_hamt_node *node;                                                          \
                                                                                     \
    if ((node = (name##_hamt_node *)malloc(sizeof(name##_hamt_node))) ==             \
        NULL) {                                                                      \
      fprintf(stderr, "failed to allocate memory for node\n");                       \
      return NULL;                                                                   \
    }                                                                                \
                                                                                     \
    node->hash = hash;                                                               \
    node->type = type;                                                               \
    node->key = key;                                                                 \
    node->value = value;                                                             \
    node->children = children;                                                       \
    node->bitmap = bitmap;                                                           \
                                                                                     \
    return node;                                                                     \
  }                                                                                  \
                                                                                     \
  static name##_hamt_node *name##_hamt_create_leaf(unsigned int hash,                \
                                                   name *key, void *value) {         \
    return name##_hamt_create_node(hash, key, value, LEAF, NULL, 0);                 \
  }                                                                                  \
                                                                                     \
  static name##_hamt_node *name##_hamt_create_collision(                             \
      unsigned int hash, name##_hamt_node **children, int bitmap) {                  \
    return name##_hamt_create_node(hash, NULL, NULL, COLLISION, children,            \
                                   bitmap);                                          \
  }                                                                                  \
                                                                                     \
  static name##_hamt_node *name##_hamt_create_branch(                                \
      unsigned int hash, name##_hamt_node **children) {                              \
    return name##_hamt_create_node(hash, NULL, NULL, BRANCH, children, 0);           \
  }                                                                                  \
                                                                                     \
  /* again, bitmap is size  */                                                       \
  static name##_hamt_node *name##_hamt_create_arraynode(                             \
      name##_hamt_node **children, unsigned int bitmap) {                            \
    return name##_hamt_create_node(0, NULL, NULL, ARRAY_NODE, children,              \
                                   bitmap);                                          \
  }                                                                                  \
                                                                                     \
  static bool name##_hamt_is_leaf(name##_hamt_node *node) {                          \
    return node != NULL && (node->type == LEAF || node->type == COLLISION);          \
  }                                                                                  \
                                                                                     \
  /*======= Allocators ==============*/                                              \
  /* Assign `n` number of children, at least `CAPACITY` in size */                   \
  static name##_hamt_node **name##_hamt_alloc_children(int size) {                   \
    name##_hamt_node **children;                                                     \
                                                                                     \
    if ((children = (name##_hamt_node **)calloc(sizeof(name##_hamt_node *),          \
                                                size)) == NULL) {                    \
      fprintf(stderr, "Failed to allocate memory for children");                     \
      return NULL;                                                                   \
    }                                                                                \
                                                                                     \
    return children;                                                                 \
  }                                                                                  \
                                                                                     \
  /*======= moving / inserting child nodes ==============*/                          \
  /**                                                                                \
   * Insert child at given position                                                  \
   */                                                                                \
  static inline void name##_hamt_insert_child(                                       \
      name##_hamt_node *parent, name##_hamt_node *child,                             \
      unsigned int position, unsigned int size) {                                    \
    name##_hamt_node *temp[sizeof(name##_hamt_node *) * size];                       \
                                                                                     \
    unsigned int i = 0, j = 0;                                                       \
                                                                                     \
    while (j < position) {                                                           \
      temp[i++] = parent->children[j++];                                             \
    }                                                                                \
    temp[i++] = child;                                                               \
    while (j < size) {                                                               \
      temp[i++] = parent->children[j++];                                             \
    }                                                                                \
    memcpy(parent->children, temp, sizeof(name##_hamt_node *) * (size + 1));         \
  }                                                                                  \
                                                                                     \
  /**                                                                                \
   * Remove child                                                                    \
   */                                                                                \
  static inline void name##_hamt_remove_child(                                       \
      name##_hamt_node *parent, unsigned int position, unsigned int size) {          \
    int arr_size = sizeof(name##_hamt_node *) * (size - 1);                          \
    name##_hamt_node **new_children = name##_hamt_alloc_children(arr_size);          \
                                                                                     \
    unsigned int i = 0, j = 0;                                                       \
                                                                                     \
    while (j < position) {                                                           \
      new_children[i++] = parent->children[j++];                                     \
    }                                                                                \
    j++;                                                                             \
    while (j < size) {                                                               \
      new_children[i++] = parent->children[j++];                                     \
    }                                                                                \
                                                                                     \
    parent->children = new_children;                                                 \
  }                                                                                  \
                                                                                     \
  /**                                                                                \
   * Replace child                                                                   \
   */                                                                                \
  static inline void name##_hamt_replace_child(name##_hamt_node *node,               \
                                               name##_hamt_node *child,              \
                                               unsigned int position) {              \
    node->children[position] = child;                                                \
  }                                                                                  \
                                                                                     \
  /**                                                                                \
   * Function is just to split out the other methods                                 \
   * This is an atempt at polymorphism                                               \
   */                                                                                \
  static name##_hamt_node *name##_hamt_insert(name##_hamt_node *node,                \
                                              unsigned int hash, name *key,          \
                                              void *value, int depth) {              \
                                                                                     \
    name##_hamt_insert_instruction_t ins = {.node = node,                            \
                                            .key = key,                              \
                                            .hash = hash,                            \
                                            .value = value,                          \
                                            .depth = depth};                         \
                                                                                     \
    switch (node->type) {                                                            \
    case LEAF:                                                                       \
      return name##_hamt_handle_leaf_insert(&ins);                                   \
    case BRANCH:                                                                     \
      return name##_hamt_handle_branch_insert(&ins);                                 \
    case COLLISION:                                                                  \
      return name##_hamt_handle_collision_insert(&ins);                              \
    case ARRAY_NODE:                                                                 \
      return name##_hamt_handle_arraynode_insert(&ins);                              \
    default:                                                                         \
      return NULL;                                                                   \
    }                                                                                \
  }                                                                                  \
                                                                                     \
  /**                                                                                \
   * If the hashes clash create a new collision node                                 \
   *                                                                                 \
   * If the partial hashes are the same recurse                                      \
   *                                                                                 \
   * Otherwise create a new Branch with the new hash                                 \
   */                                                                                \
  static inline name##_hamt_node *name##_hamt_merge_leaves(                          \
      unsigned int depth, unsigned int h1, name##_hamt_node *n1,                     \
      unsigned int h2, name##_hamt_node *n2) {                                       \
    name##_hamt_node **new_children = NULL;                                          \
                                                                                     \
    if (h1 == h2) {                                                                  \
      new_children = name##_hamt_alloc_children(MIN_COLLISION_NODE_SIZE);            \
      new_children[0] = n2;                                                          \
      new_children[1] = n1;                                                          \
      return name##_hamt_create_collision(h1, new_children, 2);                      \
    }                                                                                \
                                                                                     \
    unsigned int sub_h1 = name##_hamt_get_frag(h1, depth);                           \
    unsigned int sub_h2 = name##_hamt_get_frag(h2, depth);                           \
    unsigned int new_hash =                                                          \
        name##_hamt_get_mask(sub_h1) | name##_hamt_get_mask(sub_h2);                 \
    new_children = name##_hamt_alloc_children(MAX_BRANCH_SIZE);                      \
                                                                                     \
    if (sub_h1 == sub_h2) {                                                          \
      new_children[0] = name##_hamt_merge_leaves(depth + 1, h1, n1, h2, n2);         \
    } else if (sub_h1 < sub_h2) {                                                    \
      new_children[0] = n1;                                                          \
      new_children[1] = n2;                                                          \
    } else {                                                                         \
      new_children[0] = n2;                                                          \
      new_children[1] = n1;                                                          \
    }                                                                                \
                                                                                     \
    return name##_hamt_create_branch(new_hash, new_children);                        \
  }                                                                                  \
                                                                                     \
  /**                                                                                \
   * If what we are trying to insert matches key return a new LeafNode               \
   *                                                                                 \
   * If we got here and there is no match we need to transform the node              \
   * into a branch node using 'name##_hamt_merge_leaves'                             \
   */                                                                                \
  static inline name##_hamt_node *name##_hamt_handle_leaf_insert(                    \
      name##_hamt_insert_instruction_t *ins) {                                       \
    name##_hamt_node *new_child =                                                    \
        name##_hamt_create_leaf(ins->hash, ins->key, ins->value);                    \
    if (equals(ins->node->key, ins->key)) {                                          \
      /* if (strcmp(ins->node->key, ins->key) == 0) { */                             \
      return new_child;                                                              \
    }                                                                                \
                                                                                     \
    return name##_hamt_merge_leaves(ins->depth, ins->node->hash, ins->node,          \
                                    new_child->hash, new_child);                     \
  }                                                                                  \
                                                                                     \
  static inline name##_hamt_node *name##_hamt_expand_branch_to_array_node(           \
      int idx, name##_hamt_node *child, unsigned int bitmap,                         \
      name##_hamt_node **children) {                                                 \
    name##_hamt_node **new_children = name##_hamt_alloc_children(SIZE);              \
    unsigned int bit = bitmap;                                                       \
    unsigned int count = 0;                                                          \
                                                                                     \
    for (unsigned int i = 0; bit; ++i) {                                             \
      if (bit & 1) {                                                                 \
        new_children[i] = children[count++];                                         \
      }                                                                              \
      bit >>= 1U;                                                                    \
    }                                                                                \
                                                                                     \
    new_children[idx] = child;                                                       \
    return name##_hamt_create_arraynode(new_children, count + 1);                    \
  }                                                                                  \
                                                                                     \
  /* clang-format off */                                                           \
  /**                                                                              \
   * If there is no node at the given index insert the child and update            \
   * the hash.                                                                     \
   *                                                                               \
   * If there is no node at ^   ^^     ^^   but the size is bigger than the        \
   * maximum capacity for a Branch, then expand into an ArrayNode                  \
   *                                                                               \
   * If the child exists in the slot recurse into the tree.                        \
   */ \
  /* clang-format on */                                                              \
  static inline name##_hamt_node *name##_hamt_handle_branch_insert(                  \
      name##_hamt_insert_instruction_t *ins) {                                       \
    unsigned int frag = name##_hamt_get_frag(ins->hash, ins->depth);                 \
    unsigned int mask = name##_hamt_get_mask(frag);                                  \
    unsigned int pos = name##_hamt_get_position(ins->node->hash, frag);              \
    bool exists = ins->node->hash & mask;                                            \
                                                                                     \
    if (!exists) {                                                                   \
      unsigned int size = name##_hamt_popcount(ins->node->hash);                     \
      name##_hamt_node *new_child =                                                  \
          name##_hamt_create_leaf(ins->hash, ins->key, ins->value);                  \
                                                                                     \
      if (size >= MAX_BRANCH_SIZE) {                                                 \
        return name##_hamt_expand_branch_to_array_node(                              \
            frag, new_child, ins->node->hash, ins->node->children);                  \
      } else {                                                                       \
        name##_hamt_node *new_branch = name##_hamt_create_branch(                    \
            ins->node->hash | mask, ins->node->children);                            \
        name##_hamt_insert_child(new_branch, new_child, pos, size);                  \
                                                                                     \
        return new_branch;                                                           \
      }                                                                              \
    } else {                                                                         \
      name##_hamt_node *new_branch =                                                 \
          name##_hamt_create_branch(ins->node->hash, ins->node->children);           \
      name##_hamt_node *child = new_branch->children[pos];                           \
                                                                                     \
      /* go to next depth, inserting a branch as the child */                        \
      name##_hamt_replace_child(new_branch,                                          \
                                name##_hamt_insert(child, ins->hash, ins->key,       \
                                                   ins->value,                       \
                                                   ins->depth + 1),                  \
                                pos);                                                \
                                                                                     \
      return new_branch;                                                             \
    }                                                                                \
  }                                                                                  \
                                                                                     \
  /**                                                                                \
   * If the key string is the same as the one we are trying to                       \
   * name##_hamt_insert then replace the node. Otherwise                             \
   * name##_hamt_insert the node at the end of the collision node's                  \
   * children                                                                        \
   */                                                                                \
  static inline name##_hamt_node *name##_hamt_handle_collision_insert(               \
      name##_hamt_insert_instruction_t *ins) {                                       \
    unsigned int len = ins->node->bitmap;                                            \
    name##_hamt_node *new_child =                                                    \
        name##_hamt_create_leaf(ins->hash, ins->key, ins->value);                    \
    name##_hamt_node *collision_node = name##_hamt_create_collision(                 \
        ins->node->hash, ins->node->children, ins->node->bitmap);                    \
                                                                                     \
    if (ins->hash == ins->node->hash) {                                              \
      for (int i = 0; i < collision_node->bitmap; ++i) {                             \
        if (equals(ins->node->children[i]->key, ins->key)) {                         \
          /* if (strcmp(ins->node->children[i]->key, ins->key) == 0) { */            \
          name##_hamt_replace_child(ins->node, new_child, i);                        \
          return collision_node;                                                     \
        }                                                                            \
      }                                                                              \
                                                                                     \
      name##_hamt_insert_child(collision_node, new_child, len, len);                 \
      collision_node->bitmap++;                                                      \
      return collision_node;                                                         \
    }                                                                                \
                                                                                     \
    return name##_hamt_merge_leaves(ins->depth, ins->node->hash, ins->node,          \
                                    new_child->hash, new_child);                     \
  }                                                                                  \
                                                                                     \
  /**                                                                                \
   * If there is a child in the place where we are trying to                         \
   * name##_hamt_insert, step into the tree.                                         \
   *                                                                                 \
   * Otherwise we can create a leaf and replace the empty slot. We                   \
   * allocate 'SIZE' worth of children and fill the blank spaces with                \
   * null so it is easy to keep track off.                                           \
   *                                                                                 \
   * Could switch out to a bitmap                                                    \
   */                                                                                \
  static inline name##_hamt_node *name##_hamt_handle_arraynode_insert(               \
      name##_hamt_insert_instruction_t *ins) {                                       \
    unsigned int frag = name##_hamt_get_frag(ins->hash, ins->depth);                 \
    int size = ins->node->bitmap;                                                    \
                                                                                     \
    name##_hamt_node *child = ins->node->children[frag];                             \
    name##_hamt_node *new_child = NULL;                                              \
                                                                                     \
    if (child) {                                                                     \
      new_child = name##_hamt_insert(child, ins->hash, ins->key, ins->value,         \
                                     ins->depth + 1);                                \
    } else {                                                                         \
      new_child = name##_hamt_create_leaf(ins->hash, ins->key, ins->value);          \
    }                                                                                \
                                                                                     \
    name##_hamt_replace_child(ins->node, new_child, frag);                           \
                                                                                     \
    if (child == NULL && new_child != NULL) {                                        \
      return name##_hamt_create_arraynode(ins->node->children, size + 1);            \
    }                                                                                \
                                                                                     \
    return name##_hamt_create_arraynode(ins->node->children, size);                  \
  }                                                                                  \
                                                                                     \
  /**                                                                                \
   * Return a new node                                                               \
   */                                                                                \
  name##_hamt *name##_hamt_set(name##_hamt *hamt, name *key, void *value) {          \
    unsigned int hash = hashof(key);                                                 \
                                                                                     \
    if (hamt->root != NULL) {                                                        \
      hamt->root = name##_hamt_insert(hamt->root, hash, key, value, 0);              \
    } else {                                                                         \
      hamt->root = name##_hamt_create_leaf(hash, key, value);                        \
    }                                                                                \
                                                                                     \
    return hamt;                                                                     \
  }                                                                                  \
  /**                                                                                \
   * Wind down the tree to the leaf node using the hash.                             \
   */                                                                                \
  void *name##_hamt_get(name##_hamt *hamt, name *key) {                              \
    unsigned int hash = hashof(key);                                                 \
    name##_hamt_node *node = hamt->root;                                             \
    int depth = 0;                                                                   \
                                                                                     \
    for (;;) {                                                                       \
      if (node == NULL) {                                                            \
        return NULL;                                                                 \
      }                                                                              \
      switch (node->type) {                                                          \
      case BRANCH: {                                                                 \
        unsigned int frag = name##_hamt_get_frag(hash, depth);                       \
        unsigned int mask = name##_hamt_get_mask(frag);                              \
                                                                                     \
        if (node->hash & mask) {                                                     \
          unsigned int idx = name##_hamt_get_position(node->hash, frag);             \
          node = node->children[idx];                                                \
          depth++;                                                                   \
          continue;                                                                  \
        } else {                                                                     \
          return NULL;                                                               \
        }                                                                            \
      }                                                                              \
                                                                                     \
      case COLLISION: {                                                              \
        int len = node->bitmap;                                                      \
        for (int i = 0; i < len; ++i) {                                              \
          name##_hamt_node *child = node->children[i];                               \
          if (child != NULL &&                                                       \
              equals(child->key, key) /* strcmp(child->key, key) == 0 */)            \
            return child->value;                                                     \
        }                                                                            \
        return NULL;                                                                 \
      }                                                                              \
                                                                                     \
      case LEAF: {                                                                   \
        if (node != NULL &&                                                          \
            equals(node->key, key) /* strcmp(node->key, key) == 0 */                 \
        ) {                                                                          \
          return node->value;                                                        \
        }                                                                            \
        return NULL;                                                                 \
      }                                                                              \
                                                                                     \
      case ARRAY_NODE: {                                                             \
        node = node->children[name##_hamt_get_frag(hash, depth)];                    \
        if (node != NULL) {                                                          \
          depth++;                                                                   \
          continue;                                                                  \
        }                                                                            \
        return NULL;                                                                 \
      }                                                                              \
      }                                                                              \
    }                                                                                \
  }                                                                                  \
                                                                                     \
  /* Just to split out the functions, does nothing special */                        \
  static name##_hamt_node *name##_hamt_remove_node(                                  \
      name##_hamt_removal_t *rem) {                                                  \
    if (rem->node == NULL) {                                                         \
      return NULL;                                                                   \
    }                                                                                \
                                                                                     \
    switch (rem->node->type) {                                                       \
    case LEAF:                                                                       \
      return name##_hamt_handle_leaf_removal(rem);                                   \
    case BRANCH:                                                                     \
      return name##_hamt_handle_branch_removal(rem);                                 \
    case COLLISION:                                                                  \
      return name##_hamt_handle_collision_removal(rem);                              \
    case ARRAY_NODE:                                                                 \
      return name##_hamt_handle_arraynode_removal(rem);                              \
                                                                                     \
    default:                                                                         \
      /* NOT REACHED  */                                                             \
      return rem->node;                                                              \
    }                                                                                \
  }                                                                                  \
                                                                                     \
  /**                                                                                \
   * Removing a child from the CollisionNode or collapsing if there is               \
   * only one child left.                                                            \
   */                                                                                \
  static inline name##_hamt_node *name##_hamt_handle_collision_removal(              \
      name##_hamt_removal_t *rem) {                                                  \
    if (rem->node->hash == rem->hash) {                                              \
      for (int i = 0; i < rem->node->bitmap; ++i) {                                  \
        name##_hamt_node *child = rem->node->children[i];                            \
                                                                                     \
        if (equals(child->key, rem->key)) {                                          \
          /* if (strcmp(child->key, rem->key) == 0) { */                             \
          name##_hamt_remove_child(rem->node, i, rem->node->bitmap);                 \
          /* could free rem->node here */                                            \
          if ((rem->node->bitmap - 1) > 1) {                                         \
            return name##_hamt_create_collision(                                     \
                rem->node->hash, rem->node->children, rem->node->bitmap - 1);        \
          }                                                                          \
          /* Collapse collision node */                                              \
          return rem->node->children[0];                                             \
        }                                                                            \
      }                                                                              \
    }                                                                                \
                                                                                     \
    return rem->node;                                                                \
  }                                                                                  \
                                                                                     \
  /**                                                                                \
   * Removing an element from a branch node. Either traversing down                  \
   * the tree, collapsing the node, removing a child or a noop.                      \
   */                                                                                \
  static inline name##_hamt_node *name##_hamt_handle_branch_removal(                 \
      name##_hamt_removal_t *rem) {                                                  \
    unsigned int frag = name##_hamt_get_frag(rem->hash, rem->depth);                 \
    unsigned int mask = name##_hamt_get_mask(frag);                                  \
                                                                                     \
    name##_hamt_node *branch_node = rem->node;                                       \
    bool exists = branch_node->hash & mask;                                          \
                                                                                     \
    if (!exists) {                                                                   \
      return branch_node;                                                            \
    }                                                                                \
                                                                                     \
    unsigned int pos = name##_hamt_get_position(branch_node->hash, frag);            \
    int size = name##_hamt_popcount(branch_node->hash);                              \
    name##_hamt_node *child = branch_node->children[pos];                            \
    rem->node = child;                                                               \
    rem->depth++;                                                                    \
                                                                                     \
    name##_hamt_node *new_child = name##_hamt_remove_node(rem);                      \
                                                                                     \
    if (child == new_child) {                                                        \
      return branch_node;                                                            \
    }                                                                                \
                                                                                     \
    if (new_child == NULL) {                                                         \
      unsigned int new_hash = branch_node->hash & ~mask;                             \
      if (!new_hash) {                                                               \
        return NULL;                                                                 \
      }                                                                              \
                                                                                     \
      /* Collapse the node */                                                        \
      if (size == 2 && name##_hamt_is_leaf(branch_node->children[pos ^ 1])) {        \
        return branch_node->children[pos ^ 1];                                       \
      }                                                                              \
                                                                                     \
      name##_hamt_remove_child(branch_node, pos, size);                              \
      return name##_hamt_create_branch(new_hash, branch_node->children);             \
    }                                                                                \
                                                                                     \
    if (size == 1 && name##_hamt_is_leaf(new_child)) {                               \
      return new_child;                                                              \
    }                                                                                \
                                                                                     \
    name##_hamt_replace_child(branch_node, new_child, pos);                          \
    return name##_hamt_create_branch(branch_node->hash,                              \
                                     branch_node->children);                         \
  }                                                                                  \
                                                                                     \
  /**                                                                                \
   * Remove the node, as a modification if you passed through a free                 \
   * function from the top, you could then free your object here.                    \
   */                                                                                \
  static inline name##_hamt_node *name##_hamt_handle_leaf_removal(                   \
      name##_hamt_removal_t *rem) {                                                  \
    if (equals(rem->node->key, rem->key)) {                                          \
      /* if (strcmp(rem->node->key, rem->key) == 0) { */                             \
      /* could free rem->node here */                                                \
      return NULL;                                                                   \
    }                                                                                \
                                                                                     \
    return rem->node;                                                                \
  }                                                                                  \
                                                                                     \
  /**                                                                                \
   * Transform ArrayNode into a BranchNode. Setting each bit in the hash for         \
   * where a child is not NULL.                                                      \
   *                                                                                 \
   * We alloc MIN_ARRAY_NODE_SIZE as inorder to have got here the lower bound        \
   * limit for the ArrayNode must have been met.                                     \
   */                                                                                \
  static inline name##_hamt_node *name##_hamt_compress_array_to_branch(              \
      unsigned int idx, name##_hamt_node **children) {                               \
                                                                                     \
    name##_hamt_node **new_children =                                                \
        name##_hamt_alloc_children(MAX_BRANCH_SIZE);                                 \
    name##_hamt_node *child = NULL;                                                  \
    int j = 0;                                                                       \
    unsigned int hash = 0;                                                           \
                                                                                     \
    for (unsigned int i = 0; i < SIZE; ++i) {                                        \
      if (i != idx) {                                                                \
        child = children[i];                                                         \
        if (child != NULL) {                                                         \
          new_children[j++] = child;                                                 \
          hash |= 1 << i;                                                            \
        }                                                                            \
      }                                                                              \
    }                                                                                \
                                                                                     \
    return name##_hamt_create_branch(hash, new_children);                            \
  }                                                                                  \
                                                                                     \
  /**                                                                                \
   * Returns a new array node with the child with key `rem->key` removed             \
   * from the children                                                               \
   *                                                                                 \
   * Or if the total number of children is less than `MIN_ARRAY_NODE_SIZE`           \
   * will compress the node to a branch node and create the branch node hash         \
   */                                                                                \
  static inline name##_hamt_node *name##_hamt_handle_arraynode_removal(              \
      name##_hamt_removal_t *rem) {                                                  \
    unsigned int idx = name##_hamt_get_frag(rem->hash, rem->depth);                  \
                                                                                     \
    /* The node we are looking at */                                                 \
    name##_hamt_node *array_node = rem->node;                                        \
    /* This is nasty as the bitmap is used for different things.                     \
                         Here it is just a counter with the number of elements       \
       in the array `children` */                                                    \
                                                                                     \
    int size = array_node->bitmap;                                                   \
                                                                                     \
    name##_hamt_node *child = array_node->children[idx];                             \
    name##_hamt_node *new_child = NULL;                                              \
                                                                                     \
    if (child != NULL) {                                                             \
      rem->node = child;                                                             \
      rem->depth++;                                                                  \
      /* go 'in' to the structure */                                                 \
      new_child = name##_hamt_remove_node(rem);                                      \
    } else {                                                                         \
      new_child = NULL;                                                              \
    }                                                                                \
                                                                                     \
    if (child == new_child) {                                                        \
      return array_node;                                                             \
    }                                                                                \
                                                                                     \
    if (child != NULL && new_child == NULL) {                                        \
      if ((size - 1) <= MIN_ARRAY_NODE_SIZE) {                                       \
        return name##_hamt_compress_array_to_branch(idx,                             \
                                                    array_node->children);           \
      }                                                                              \
      name##_hamt_replace_child(array_node, NULL, idx);                              \
      return name##_hamt_create_arraynode(array_node->children,                      \
                                          array_node->bitmap - 1);                   \
    }                                                                                \
                                                                                     \
    name##_hamt_replace_child(array_node, new_child, idx);                           \
    return name##_hamt_create_arraynode(array_node->children,                        \
                                        array_node->bitmap);                         \
  }                                                                                  \
                                                                                     \
  /**                                                                                \
   * Remove a node from the tree, the delete happens on the leaf or                  \
   * collision node layer.                                                           \
   *                                                                                 \
   * I've been testing this rather horribly with a counter to ensure                 \
   * the 466550 from the test dictionary actually get removed.                       \
   */                                                                                \
  name##_hamt *name##_hamt_remove(name##_hamt *hamt, name *key) {                    \
    unsigned int hash = hashof(key);                                                 \
    name##_hamt_removal_t rem;                                                       \
    rem.hash = hash;                                                                 \
    rem.depth = 0;                                                                   \
    rem.key = key;                                                                   \
    rem.node = hamt->root;                                                           \
                                                                                     \
    if (hamt->root != NULL) {                                                        \
      hamt->root = name##_hamt_remove_node(&rem);                                    \
    }                                                                                \
                                                                                     \
    return hamt;                                                                     \
  }                                                                                  \
                                                                                     \
  /* ====== Visiting functions ====== */                                             \
  static void name##_hamt_visit_all_nodes(                                           \
      name##_hamt_node *hamt, void (*visitor)(name * key, void *value)) {            \
    if (hamt) {                                                                      \
      switch (hamt->type) {                                                          \
      case ARRAY_NODE:                                                               \
      case BRANCH: {                                                                 \
        int len = name##_hamt_popcount(hamt->bitmap);                                \
        for (int i = 0; i < len; ++i) {                                              \
          name##_hamt_node *child = hamt->children[i];                               \
          name##_hamt_visit_all_nodes(child, visitor);                               \
        }                                                                            \
        return;                                                                      \
      }                                                                              \
      case COLLISION: {                                                              \
        int len = name##_hamt_popcount(hamt->bitmap);                                \
        for (int i = 0; i < len; ++i) {                                              \
          name##_hamt_node *child = hamt->children[i];                               \
          name##_hamt_visit_all_nodes(child, visitor);                               \
        }                                                                            \
        return;                                                                      \
      }                                                                              \
      case LEAF: {                                                                   \
        visitor(hamt->key, hamt->value);                                             \
      }                                                                              \
      }                                                                              \
    }                                                                                \
  }                                                                                  \
                                                                                     \
  void name##_hamt_visit_all(name##_hamt *hamt,                                      \
                             void (*visitor)(name *, void *)) {                      \
    name##_hamt_visit_all_nodes(hamt->root, visitor);                                \
  }                                                                                  \
  /* ====== Printing functions ====== */                                             \
                                                                                     \
  /* static void name##_hamt__print_node(name *key, void *value) { */                \
  /* (void)value; */                                                                 \
  /* printf("key: %s\n", to_string(key)); */                                         \
  /* } */                                                                            \
                                                                                     \
  /* void name##_hamt_print(name##_hamt *hamt) { */                                  \
  /* name##_hamt_visit_all(hamt, name##_hamt_print_node); */                         \
  /* } */

#endif
