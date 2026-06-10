#include "core/arena/arena.h"
#include <stdlib.h>
#include <string.h>

void dpp_arena_init(struct dpp_arena *ar, size_t default_cap)
{
	ar->ar_head        = NULL;
	ar->ar_default_cap = default_cap ? default_cap : 4096;
}

void *dpp_arena_alloc(struct dpp_arena *ar, size_t size)
{
	size = m_align(size, 8);

	if (!ar->ar_head || ar->ar_head->ab_used + size > ar->ar_head->ab_cap) {
		size_t cap = ar->ar_default_cap;
		if (size > cap) cap = size;

		struct dpp_arena_block *block = malloc(sizeof(struct dpp_arena_block) + cap);
		if (!block) return NULL;

		block->ab_cap  = cap;
		block->ab_used = 0;
		block->ab_next = ar->ar_head;
		ar->ar_head    = block;
	}

	void *ptr = &ar->ar_head->ab_data[ar->ar_head->ab_used];
	ar->ar_head->ab_used += size;
	return ptr;
}

void dpp_arena_free(struct dpp_arena *ar)
{
	struct dpp_arena_block *curr = ar->ar_head;
	while (curr) {
		struct dpp_arena_block *next = curr->ab_next;
		free(curr);
		curr = next;
	}
	ar->ar_head = NULL;
}
