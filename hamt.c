/* hamt -- A Hash Array Mapped Trie implementation.
 *
 * Version 1.0 May 2021
 *
 * Copyright (c) 2021, James Barford-Evans
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "hamt.h"

#define BITS     5
#define SIZE     32
#define MASK     31
#define CAPACITY 6

enum NODE_TYPE {
	LEAF,
	BRANCH,
	COLLISON
};

typedef struct hamt_node_t {
	enum NODE_TYPE type;
	unsigned int hash;
	unsigned long bitmap;
	char *key;
	void *value;
	struct hamt_node_t **children;
} hamt_node_t;

typedef struct insert_instruction_t {
	hamt_node_t *node;
	unsigned int hash;
	char *key;
	void *value;
	int depth;
} insert_instruction_t;

static hamt_node_t *handle_collision_insert(insert_instruction_t *ins);
static hamt_node_t *handle_branch_insert(insert_instruction_t *ins);
static hamt_node_t *handle_leaf_insert(insert_instruction_t *ins);

static void resize_children(hamt_node_t *node, unsigned int size);

// hamt node creation methods
static hamt_node_t *create_node(int hash, char *key, void *value,
		enum NODE_TYPE type, hamt_node_t **children, unsigned long bitmap) {
	hamt_node_t *node;

	if ((node = (hamt_node_t *)malloc(sizeof(hamt_node_t))) == NULL) {
		fprintf(stderr, "failed to allocate memory for node\n");
		return NULL;
	}

	node->hash     = hash;
	node->type     = type;
	node->key      = key;
	node->value    = value;
	node->children = children;
	node->bitmap   = bitmap;

	return node;
}

hamt_node_t *create_hamt() {
	return create_node(0, "", NULL, BRANCH, NULL, 0);
}

static hamt_node_t *create_leaf(unsigned int hash, char *key, void *value) {
	return create_node(hash, key, value, LEAF, NULL, 0);
}

static hamt_node_t *create_collision(unsigned int hash, hamt_node_t **children,
		unsigned long bitmap) {
	return create_node(hash, NULL, NULL, COLLISON, children, bitmap);
}

static hamt_node_t *create_branch(unsigned int hash, hamt_node_t **children,
		unsigned long bitmap) {
	return create_node(hash, NULL, NULL, BRANCH, children, bitmap);
}


/*======= hashing ======================*/
/**
 * From Ideal hash trees Phil Bagley, page 3
 * https://lampwww.epfl.ch/papers/idealhashtrees.pdf
 * Count number of bits in a number
 */
static const unsigned int SK5 = 0x55555555;
static const unsigned int SK3 = 0x33333333;
static const unsigned int SKF0 = 0xF0F0F0F;

static inline int popcount(unsigned long bits) {
	bits -= ((bits >> 1ul) & SK5);
	bits = (bits & SK3)  + ((bits >> 2ul) & SK3);
	bits = (bits & SKF0) + ((bits >> 4ul) & SKF0);
	bits += bits >> 8ul;
	return (bits + (bits >> 16ul)) & 0x3F;
}

/**
 * convert a string to a 32bit unsigned integer
 */
static unsigned int get_hash(char *str) {
	unsigned int hash = 0;	
	char *ptr = str;

	while (*ptr != '\0') {
		hash = ((hash << BITS) - hash) + *(ptr++);
	}

	return hash;
}

static unsigned int get_mask(unsigned int frag) {
	return 1 << frag;
}

/* take 5 bits of the hash */
static unsigned int get_frag(unsigned int hash, int depth) {
	return ((unsigned int)hash >> (BITS * depth)) & MASK;
}

/**
 * Get the position in the array where the child is located
 *
 */
static unsigned int get_position(unsigned int hash, unsigned int frag) {
	return popcount(hash & (get_mask(frag) - 1));
}

/**
 * Set bitmap to 1 if there are no children or shift the bit along
 * 
 * e.g (from left to right)
 * 00000000
 * add child:
 * 00000001
 * 
 * 00000001
 * add another child:
 * 00000011
 */
static void update_bitmap(hamt_node_t *node) {	
	if (node->bitmap == 0) node->bitmap = 1;
	else node->bitmap |= node->bitmap << 1ul;
}

static void replace_child(hamt_node_t *node, unsigned int position,
		hamt_node_t *child) {
	node->children[position] = child;
}

static hamt_node_t **alloc_children(int size) {
	hamt_node_t **children;

	if ((children = (hamt_node_t **)malloc(sizeof(hamt_node_t *) * size)) == NULL) {
		fprintf(stderr, "Failed to allocate memory for children");
		return NULL;
	}

	return children;
}	

/**
 * If the slot is occupied in the bitmap move all the children over to
 * allow this child in
 * 
 * Example:
 * We want to slot in child to position 2 and for simplicity
 * it's key is "c".
 * 
 * initial:
 * [a,b,d,f,g]
 *
 * after moving
 * [a,b, ,d,f,g]
 * 
 * insert
 * [a,b,c,d,f,g]
 */
static void insert_child(hamt_node_t *parent, hamt_node_t *child,
		unsigned int position, unsigned int size) {
	resize_children(parent, size);

	hamt_node_t *temp[sizeof(hamt_node_t *) * size];

	unsigned int i = 0, j = 0;

	while (j < position) {
		temp[i++] = parent->children[j++];
	}
	temp[i++] = child;
	while (j < size) {
		temp[i++] = parent->children[j++];
	}

	memcpy(parent->children, temp, sizeof(hamt_node_t *) * (size + 1));
}

/**
 * Assign more space for children, assigns `CAPACITY` * size of children.
 * Or a noop if there is enough space
 *
 * TODO: this is over allocating
 */
static void resize_children(hamt_node_t *node, unsigned int size) {
	if (size % CAPACITY == 0) {
		unsigned int new_size = sizeof(hamt_node_t) * (CAPACITY + size);
		hamt_node_t **children = NULL;

		if (node->children == NULL) {
			children = alloc_children(CAPACITY);
		} else {
			children = (hamt_node_t **)realloc(node->children, new_size);
		}

		node->children = children;
	}
}

static bool exact_str_match(char *str1, char *str2) {
	return strcmp(str1, str2) == 0;
}

void visit_all(hamt_node_t *hamt, void(*visitor)(char *key, void *value)) {
	if (hamt) {
		switch (hamt->type) {
			case BRANCH: {
				int len = popcount(hamt->bitmap);
				for (int i = 0; i < len; ++i) {
					hamt_node_t *child = hamt->children[i];
					visit_all(child, visitor);
				}
				return;
			}
			case COLLISON: {
				int len = popcount(hamt->bitmap);
				for (int i = 0; i < len; ++i) {
					hamt_node_t *child = hamt->children[i];
					visit_all(child, visitor);
				}
				return;
			}
			case LEAF: {
				visitor(hamt->key, hamt->value);
			}
		}
	}
}

static void print_node(char *key, void *value) {
	(void)value;
	printf("key: %s\n", key);
}

void print_hamt(hamt_node_t *hamt) {
	visit_all(hamt, print_node);
}

/**
 * Node insertion 
 */
static hamt_node_t *insert(hamt_node_t *node, unsigned int hash, char *key,
		void *value, int depth) {
	
	insert_instruction_t ins = {
		.node  = node,
		.key   = key,
		.hash  = hash,
		.value = value,
		.depth = depth
	};

	switch (node->type) {
		case LEAF:     return handle_leaf_insert(&ins);
		case BRANCH:   return handle_branch_insert(&ins);
		case COLLISON: return handle_collision_insert(&ins);
		default:
			return NULL;
	}
}

static hamt_node_t *handle_leaf_insert(insert_instruction_t *ins) {
	if (ins->node->hash == ins->hash) {
		if (exact_str_match(ins->node->key, ins->key)) {
			return create_leaf(ins->hash, ins->key, ins->value);
		}

		unsigned long new_bitmap = 3;

		hamt_node_t *collision_node = create_collision(ins->hash,
			alloc_children(CAPACITY), new_bitmap);

		if (collision_node->children == NULL) return NULL;

		collision_node->children[0] = ins->node;
		collision_node->children[1] = create_leaf(ins->hash, ins->key, ins->value);

		return collision_node;
	}

	unsigned int prev_frag = get_frag(ins->node->hash, ins->depth);
	unsigned int prev_mask = get_mask(prev_frag);

	unsigned int frag = get_frag(ins->hash, ins->depth);
	unsigned int mask = get_mask(frag);

	if (prev_frag == frag) {
		hamt_node_t *new_branch = create_branch(mask, alloc_children(CAPACITY), 0);

		new_branch->children[0] = insert(
			insert(
				create_branch(0, alloc_children(CAPACITY), 0),
					ins->hash, ins->key, ins->value, ins->depth + 1
			),
			ins->node->hash, ins->node->key, ins->node->value, ins->depth + 1
		);

		return new_branch;
	}

	hamt_node_t *child = create_leaf(ins->hash, ins->key, ins->value);
	hamt_node_t *new_branch = create_branch(mask | prev_mask, alloc_children(CAPACITY), 0);

	if (new_branch->children == NULL) return NULL;
	if (prev_frag < frag) {
		new_branch->children[0] = ins->node;
		new_branch->children[1] = child;
	} else {
		new_branch->children[0] = child;
		new_branch->children[1] = ins->node;
	}

	return new_branch;
}

static hamt_node_t *handle_branch_insert(insert_instruction_t *ins) {
	unsigned int frag = get_frag(ins->hash, ins->depth);
	unsigned int mask = get_mask(frag);
	unsigned int pos = get_position(ins->node->hash, frag);

	// Branch is full
	if (ins->node->hash & mask) {
		hamt_node_t *new_branch = create_branch(ins->node->hash, ins->node->children, 0);
		hamt_node_t *child = new_branch->children[pos];

		// go to next depth, inserting a branch as the child
		replace_child(new_branch, pos, insert(child, ins->hash, ins->key, ins->value,
					ins->depth + 1));

		return new_branch;
	} else {
		hamt_node_t *new_branch = create_branch(ins->node->hash | mask, ins->node->children, 0);
		hamt_node_t *new_child = create_leaf(ins->hash, ins->key, ins->value);

		unsigned int size = popcount(ins->node->hash);
		insert_child(new_branch, new_child, pos, size);

		return new_branch;
	}
}

static hamt_node_t *handle_collision_insert(insert_instruction_t *ins) {
	unsigned int len = popcount(ins->node->bitmap);	
	hamt_node_t *new_child = create_leaf(ins->hash, ins->key, ins->value);
	hamt_node_t *collision_node = create_collision(ins->node->hash,
			ins->node->children, ins->node->bitmap);

	for (unsigned int i = 0; i < len; ++i) {	
		if (exact_str_match(ins->node->children[i]->key, ins->key)) {
			replace_child(ins->node, i, new_child);
			return collision_node;
		}
	}

	insert_child(collision_node, new_child, len, len);
	update_bitmap(collision_node);
	return collision_node;
}

/**
 * Return a new node 
 */
hamt_node_t *hamt_set(hamt_node_t *hamt, char *key, void *value) {
	unsigned int hash = get_hash(key);	

	return insert(hamt, hash, key, value, 0);
}

void *hamt_get(hamt_node_t *hamt, char *key) {
	unsigned int hash = get_hash(key);
	hamt_node_t *node = hamt;
	int depth = -1;

	for (;;) {
		++depth;
		if (node == NULL) {
			return NULL;
		}
		switch (node->type) {
			case BRANCH: {
				unsigned int frag = get_frag(hash, depth);
				unsigned int mask = get_mask(frag);

				if (node->hash & mask) {
					unsigned int idx = get_position(node->hash, frag);
					node = node->children[idx];
					continue;
				} else {
					return NULL;
				}
			}

			case COLLISON: {
				int len = popcount(node->bitmap);
				for (int i = 0; i < len; ++i) {
					hamt_node_t *child = node->children[i];
					if (child != NULL && exact_str_match(child->key, key))
						return child->value;
				}	
				return NULL;
			}
	
			case LEAF: {
				if (node != NULL && exact_str_match(node->key, key)) {
					return node->value;
				}
				return NULL;
			}
		}	
	}
}
