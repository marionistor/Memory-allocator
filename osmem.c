// SPDX-License-Identifier: BSD-3-Clause

#include "osmem.h"
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include "list_functions.h"


#define MMAP_THRESHOLD (128 * 1024)

// variabila globala pentru a verifica daca s-a facut prealocare
size_t heap_prealloc;

void *os_malloc(size_t size)
{
	/* TODO: Implement os_malloc */

	if (size == 0)
		return NULL;

	// dimensiunea blocului de memorie
	size_t block_size = ALIGN(size) + ALIGN(sizeof(struct block_meta));

	// pentru dimensiuni mai mari decat MMAP_THRESHOLD se foloseste mmap
	if (block_size > MMAP_THRESHOLD) {
		struct block_meta *new_block =
		(struct block_meta *) mmap(NULL, block_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

		DIE((void *) new_block == (void *) -1, "mmap fialed");
		add_new_block(new_block, ALIGN(size), STATUS_MAPPED);
		return (char *) new_block + ALIGN(sizeof(struct block_meta));
	}

	// verificare prealocare
	if (!heap_prealloc) {
		struct block_meta *new_block = (struct block_meta *) sbrk(MMAP_THRESHOLD);

		DIE((void *) new_block == (void *) -1, "sbrk failed");
		heap_prealloc = 1;
		add_new_block(new_block, MMAP_THRESHOLD - ALIGN(sizeof(struct block_meta)), STATUS_ALLOC);

		// verificare split dupa prealocare
		if (MMAP_THRESHOLD - block_size >= ALIGN(sizeof(struct block_meta)) + ALIGNMENT)
			split_blocks(new_block, ALIGN(size), MMAP_THRESHOLD - ALIGN(sizeof(struct block_meta)));

		return (char *) new_block + ALIGN(sizeof(struct block_meta));
	}

	// unesc toate blocurile cu STATUS_FREE adiacente
	coalesce();
	struct block_meta *blk = find_best_block(ALIGN(size));

	// nu a fost gasit niciun bloc potrivit
	if (blk == NULL) {
		struct block_meta *last_heap_blk = find_last_heap_block();

		// verific daca ultimul bloc de pe heap are STATUS_FREE pentru a face expand
		if (last_heap_blk != NULL && last_heap_blk->status == STATUS_FREE) {
			expand_block(last_heap_blk, last_heap_blk->size, ALIGN(size));
			return (char *) last_heap_blk + ALIGN(sizeof(struct block_meta));
		}

		struct block_meta *new_block = (struct block_meta *) sbrk(block_size);

		DIE((void *) new_block == (void *) -1, "sbrk failed");
		add_new_block(new_block, ALIGN(size), STATUS_ALLOC);
		return (char *) new_block + ALIGN(sizeof(struct block_meta));
	}

	// a fost gasit un bloc care va avea STATUS_ALLOC
	blk->status = STATUS_ALLOC;

	// verificare split pentru blocul de memorie gasit
	if (blk->size - ALIGN(size) >= ALIGN(sizeof(struct block_meta)) + ALIGNMENT)
		split_blocks(blk, ALIGN(size), blk->size);

	return (char *) blk + ALIGN(sizeof(struct block_meta));
}

void os_free(void *ptr)
{
	/* TODO: Implement os_free */

	if (ptr == NULL)
		return;

	struct block_meta *blk = head.next;

	while (blk != &head) {
		if ((char *) blk + ALIGN(sizeof(struct block_meta)) == (char *) ptr) {
			if (blk->status == STATUS_ALLOC) {
				blk->status = STATUS_FREE;
			} else {
				blk->prev->next = blk->next;
				blk->next->prev = blk->prev;
				int ret = munmap(blk, blk->size + ALIGN(sizeof(struct block_meta)));

				DIE(ret == -1, "munmap failed");
			}
			return;
		}
		blk = blk->next;
	}
}

void *os_calloc(size_t nmemb, size_t size)
{
	/* TODO: Implement os_calloc */

	if (nmemb == 0 || size == 0)
		return NULL;

	// dimensiunea blocului
	size_t block_size = ALIGN(nmemb * size) + ALIGN(sizeof(struct block_meta));

	// daca dimensiunea blocului e mai mare decat page size se foloseste mmap
	if ((long) block_size > sysconf(_SC_PAGESIZE)) {
		struct block_meta *new_block =
		(struct block_meta *) mmap(NULL, block_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

		DIE((void *) new_block == (void *) -1, "mmap failed");
		add_new_block(new_block, ALIGN(nmemb * size), STATUS_MAPPED);
		memset((char *) new_block + ALIGN(sizeof(struct block_meta)), 0, nmemb * size);
		return (char *) new_block + ALIGN(sizeof(struct block_meta));
	}

	// verificare prealocare
	if (!heap_prealloc) {
		struct block_meta *new_block = (struct block_meta *) sbrk(MMAP_THRESHOLD);

		DIE((void *) new_block == (void *) -1, "sbrk failed");
		heap_prealloc = 1;
		add_new_block(new_block, MMAP_THRESHOLD - ALIGN(sizeof(struct block_meta)), STATUS_ALLOC);

		// verificare split dupa prealocare
		if (MMAP_THRESHOLD - block_size >= ALIGN(sizeof(struct block_meta)) + ALIGNMENT)
			split_blocks(new_block, ALIGN(nmemb * size), MMAP_THRESHOLD - ALIGN(sizeof(struct block_meta)));

		memset((char *) new_block + ALIGN(sizeof(struct block_meta)), 0,  nmemb * size);
		return (char *) new_block + ALIGN(sizeof(struct block_meta));
	}

	// unesc toate blocurile libere adiacente
	coalesce();
	struct block_meta *blk = find_best_block(ALIGN(nmemb * size));

	// nu a fost gasit niciun bloc potrivit
	if (blk == NULL) {
		struct block_meta *last_heap_blk = find_last_heap_block();

		// verific daca ultimul bloc de pe heap are STATUS_FREE pentru a putea face expand
		if (last_heap_blk != NULL && last_heap_blk->status == STATUS_FREE) {
			expand_block(last_heap_blk, last_heap_blk->size, ALIGN(nmemb * size));
			memset((char *) last_heap_blk + ALIGN(sizeof(struct block_meta)), 0, nmemb * size);
			return (char *) last_heap_blk + ALIGN(sizeof(struct block_meta));
		}

		struct block_meta *new_block = (struct block_meta *) sbrk(block_size);

		DIE((void *) new_block == (void *) -1, "sbrk failed");
		add_new_block(new_block, ALIGN(nmemb * size), STATUS_ALLOC);
		memset((char *) new_block + ALIGN(sizeof(struct block_meta)), 0, nmemb * size);
		return (char *) new_block + ALIGN(sizeof(struct block_meta));
	}

	// a fost gasit un bloc care va avea STATUS_ALLOC
	blk->status = STATUS_ALLOC;

	// verificare split pentru blocul gasit
	if (blk->size - ALIGN(nmemb * size) >= ALIGN(sizeof(struct block_meta)) + ALIGNMENT)
		split_blocks(blk, ALIGN(nmemb * size), blk->size);

	memset((char *) blk + ALIGN(sizeof(struct block_meta)), 0, nmemb * size);
	return (char *) blk + ALIGN(sizeof(struct block_meta));
}

void *os_realloc(void *ptr, size_t size)
{
	/* TODO: Implement os_realloc */

	if (ptr == NULL)
		return os_malloc(size);

	if (size == 0) {
		os_free(ptr);
		return NULL;
	}

	// caut adresa in lista de blocuri de memorie
	struct block_meta *blk = find_block(ptr);

	if (blk == NULL)
		return NULL;

	if (blk->status == STATUS_FREE)
		return NULL;

	// daca realloc primeste ca parametru aceeasi dimensiune ca a blocului se va returna adresa primita ca parametru
	if (ALIGN(size) == blk->size)
		return ptr;

	// dimensiunea blocului
	size_t block_size = ALIGN(size) + ALIGN(sizeof(struct block_meta));

	/* daca blocul are STATUS_MAPPED sau dimensiunea blocului e mai mare decat MMAP_THRESHOLD se va aloca un bloc nou
	 * unde se va copia continutul
	 */
	if (blk->status == STATUS_MAPPED || block_size > MMAP_THRESHOLD) {
		void *new_block = os_malloc(size);

		if (ALIGN(size) > blk->size)
			// daca size este mai mare decat size-ul blocului copiez tot continutul blocului
			memmove(new_block, (char *) blk + ALIGN(sizeof(struct block_meta)), blk->size);
		else
			// daca size este mai mic decat size-ul blocului copiez doar size bytes din continutul blocului
			memmove(new_block, (char *) blk + ALIGN(sizeof(struct block_meta)), ALIGN(size));

		os_free((char *) blk + ALIGN(sizeof(struct block_meta)));
		return new_block;
	}

	// size-ul dat ca parametru este mai mic decat size-ul blocului
	if (ALIGN(size) < blk->size) {
		// se face split daca se poate
		if (blk->size - ALIGN(size) >= ALIGN(sizeof(struct block_meta)) + ALIGNMENT)
			split_blocks(blk, ALIGN(size), blk->size);

		return (char *) blk + ALIGN(sizeof(struct block_meta));
	}

	struct block_meta *last_heap_blk = find_last_heap_block();

	// daca blocul este ultimul bloc de pe heap se face expand
	if (blk == last_heap_blk) {
		expand_block(blk, blk->size, ALIGN(size));
		return (char *) blk + ALIGN(sizeof(struct block_meta));
	}

	// unesc toate blocurile cu STATUS_FREE adiacente
	coalesce();

	// daca urmatorul bloc are STATUS_FREE il unesc cu blocul curent
	if (blk->next->status == STATUS_FREE) {
		struct block_meta *temp = blk->next;

		blk->size += ALIGN(sizeof(struct block_meta)) + temp->size;
		blk->next = temp->next;
		temp->next->prev = blk;

		// daca dimensiunea blocurilor unite este cel putin cat size se poate returna blocul curent
		if (blk->size >= ALIGN(size)) {
			// se face split daca se poate
			if (blk->size - ALIGN(size) >= ALIGN(sizeof(struct block_meta)) + ALIGNMENT)
				split_blocks(blk, ALIGN(size), blk->size);

			return (char *) blk + ALIGN(sizeof(struct block_meta));
		}
	}

	// apelare malloc pentru cautarea unui bloc liber potrivit sau crearea unui bloc nou unde se va muta continutul
	void *new_block = os_malloc(size);

	memmove(new_block, (char *) blk + ALIGN(sizeof(struct block_meta)), blk->size);
	os_free((char *) blk + ALIGN(sizeof(struct block_meta)));
	return new_block;
}
