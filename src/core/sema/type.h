#ifndef DPP_TYPE_H
#define DPP_TYPE_H

#include "core/types.h"
#include "core/arena/arena.h"

// dpp_type_kind is now defined in core/types.h

#define TY_CONST          (1u << 0)
#define TY_VOLATILE       (1u << 1)
#define TY_RESTRICT       (1u << 2)
#define TY_PROVEN_NONNULL (1u << 3)
#define TY_SUSPECT        (1u << 4)
#define TY_OWNER          (1u << 5)

struct dpp_type {
	enum dpp_type_kind ty_kind;
	u32                ty_size;
	u32                ty_align;
	u32                ty_flags;
	const u8          *ty_resource_pool;
	size_t             ty_pool_len;
	void              *ty_backend_type;

	/* Pointer to next modifier in the declarator chain (e.g. PTR -> ARRAY -> FUNC) */
	struct dpp_type   *ty_next; 

	union {
		struct {
			struct dpp_node *struct_node;
		} ty_struct;

		struct {
			u32  bits;
			bool is_signed;
		} ty_ext;

		struct {
			u32 size;
			struct dpp_node *node;  /* parsed size expression (constant-folded later) */
		} ty_array;

		struct {
			struct dpp_type **params;
			struct dpp_node  *param_nodes;
			u32               num_params;
			bool              is_variadic;
		} ty_function;
	} ty_data;
};

/* Sistema de Tipos: Cria instâncias de tipos na Arena */
struct dpp_type *dpp_type_new(struct dpp_arena *arena, enum dpp_type_kind kind);
struct dpp_type *dpp_type_ptr(struct dpp_arena *arena, struct dpp_type *next);
struct dpp_type *dpp_type_array(struct dpp_arena *arena, struct dpp_type *next, u32 size);
struct dpp_type *dpp_type_function(struct dpp_arena *arena, struct dpp_type *ret_type, struct dpp_type **params, struct dpp_node *param_nodes, u32 num_params, bool is_variadic);

struct dpp_type *dpp_type_decimal(struct dpp_arena *arena, u32 bits);
struct dpp_type *dpp_type_xint(struct dpp_arena *arena, u32 bits, bool is_signed);
bool             dpp_types_are_compatible(struct dpp_type *t1, struct dpp_type *t2);
void             dpp_type_set_base(struct dpp_type *chain, struct dpp_type *base);

#endif
