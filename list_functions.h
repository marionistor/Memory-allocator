/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef __BLOCKS_LIST_H__
#define __BLOCKS_LIST_H__


#include "block_meta.h"


#define ALIGNMENT 8
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))

extern struct block_meta head;

struct block_meta *find_best_block(size_t size);
void add_new_block(struct block_meta *new_block, size_t size, int status);
void split_blocks(struct block_meta *block, size_t payload_size, size_t total_size);
void coalesce(void);
struct block_meta *find_last_heap_block(void);
void expand_block(struct block_meta *block, size_t previous_size, size_t expand_size);
struct block_meta *find_block(void *ptr);

#endif
