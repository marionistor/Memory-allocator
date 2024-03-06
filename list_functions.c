// SPDX-License-Identifier: BSD-3-Clause

#include "list_functions.h"
#include <unistd.h>
#include <stdlib.h>

// santinela listei
struct block_meta head = {0, 3, &head, &head};

// functie pentru gasirea blocului cu dimensiune minima, cel putin cat dimensiunea data
struct block_meta *find_best_block(size_t size)
{
	struct block_meta *blk = head.next;
	struct block_meta *best_blk = NULL;
	size_t best_size = __SIZE_MAX__;

	while (blk != &head) {
		if (blk->status == STATUS_FREE && blk->size >= size && blk->size < best_size) {
			best_size = blk->size;
			best_blk = blk;
		}
		blk = blk->next;
	}

	return best_blk;
}

// functie pentru adaugarea unui bloc nou
void add_new_block(struct block_meta *new_block, size_t size, int status)
{
	new_block->size = size;
	new_block->status = status;
	new_block->next = &head;
	new_block->prev = head.prev;
	head.prev->next = new_block;
	head.prev = new_block;
}

// functie pentru efectuarea operatiei de split
void split_blocks(struct block_meta *block, size_t payload_size, size_t total_size)
{
	struct block_meta *new_block =
	(struct block_meta *) ((char *) block + ALIGN(sizeof(struct block_meta)) + payload_size);

	block->size = payload_size;

	new_block->size = total_size - payload_size - ALIGN(sizeof(struct block_meta));
	new_block->status = STATUS_FREE;

	new_block->prev = block;
	new_block->next = block->next;
	block->next->prev = new_block;
	block->next = new_block;
}

// functie pentru unirea blocurilor libere adiacente
void coalesce(void)
{
	struct block_meta *first_block = head.next;
	struct block_meta *second_block = first_block->next;

	while (second_block != &head) {
		if (first_block->status == STATUS_FREE && second_block->status == STATUS_FREE) {
			first_block->size += ALIGN(sizeof(struct block_meta)) + second_block->size;
			// unesc blocurile si verific in continuare daca mai exista blocuri libere adiacente
			first_block->next = second_block->next;
			second_block->next->prev = first_block;
			second_block = first_block->next;
		} else {
			first_block = first_block->next;
			second_block = second_block->next;
		}
	}
}

// functie pentru gasirea ultimului bloc de pe heap
struct block_meta *find_last_heap_block(void)
{
	struct block_meta *blk = head.prev;

	while (blk != &head) {
		if (blk->status == STATUS_FREE || blk->status == STATUS_ALLOC)
			return blk;

		blk = blk->prev;
	}

	return NULL;
}

// functie pentru expandarea ultimului bloc de pe heap
void expand_block(struct block_meta *block, size_t previous_size, size_t expand_size)
{
	void *ret = sbrk(expand_size - previous_size);

	DIE(ret == (void *) -1, "sbrk failed");
	block->size = expand_size;
	block->status = STATUS_ALLOC;
}

// functie pentru gasirea blocului care contine adresa ptr
struct block_meta *find_block(void *ptr)
{
	struct block_meta *blk = head.next;

	while (blk != &head) {
		if ((char *) blk + ALIGN(sizeof(struct block_meta)) == (char *) ptr)
			return blk;

		blk = blk->next;
	}

	return NULL;
}
