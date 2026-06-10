#ifndef DPP_ARENA_H
#define DPP_ARENA_H

#include "core/types.h"

struct dpp_arena_block {
	struct dpp_arena_block *ab_next;
	size_t                  ab_used;
	size_t                  ab_cap;
	u8                      ab_data[];
};

struct dpp_arena {
	struct dpp_arena_block *ar_head;
	size_t                  ar_default_cap;
};

void  dpp_arena_init(struct dpp_arena *ar, size_t default_cap);
void *dpp_arena_alloc(struct dpp_arena *ar, size_t size);
void  dpp_arena_free(struct dpp_arena *ar);

#endif
