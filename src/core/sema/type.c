#include "core/sema/type.h"
#include "core/arena/arena.h"
#include <string.h>

static void s_set_default_size_align(struct dpp_type *ty)
{
	switch (ty->ty_kind) {
	case TYPE_CHAR:   ty->ty_size = 1; ty->ty_align = 1; break;
	case TYPE_BOOL:   ty->ty_size = 1; ty->ty_align = 1; break;
	case TYPE_INT:    ty->ty_size = 4; ty->ty_align = 4; break;
	case TYPE_LONG:   ty->ty_size = 8; ty->ty_align = 8; break;
	case TYPE_FLOAT:    ty->ty_size = 4;  ty->ty_align = 4;  break;
	case TYPE_FLOAT16:  ty->ty_size = 2;  ty->ty_align = 2;  break;
	case TYPE_FLOAT32:  ty->ty_size = 4;  ty->ty_align = 4;  break;
	case TYPE_FLOAT64:  ty->ty_size = 8;  ty->ty_align = 8;  break;
	case TYPE_FLOAT128: ty->ty_size = 16; ty->ty_align = 16; break;
	case TYPE_FLOAT32X: ty->ty_size = 8;  ty->ty_align = 8;  break;
	case TYPE_FLOAT64X: ty->ty_size = 16; ty->ty_align = 16; break;
	case TYPE_FLOAT128X:ty->ty_size = 16; ty->ty_align = 16; break;
	case TYPE_DOUBLE:   ty->ty_size = 8;  ty->ty_align = 8;  break;
	case TYPE_VOID:   ty->ty_size = 1; ty->ty_align = 1; break;
	default: break;
	}
}

struct dpp_type *dpp_type_new(struct dpp_arena *arena, enum dpp_type_kind kind)
{
	struct dpp_type *ty = dpp_arena_alloc(arena, sizeof(struct dpp_type));
	memset(ty, 0, sizeof(*ty));
	ty->ty_kind = kind;
	s_set_default_size_align(ty);
	return ty;
}

struct dpp_type *dpp_type_ptr(struct dpp_arena *arena, struct dpp_type *next)
{
	struct dpp_type *ty = dpp_type_new(arena, TYPE_PTR);
	ty->ty_next         = next;
	ty->ty_size         = 8;
	ty->ty_align        = 8;
	return ty;
}

struct dpp_type *dpp_type_array(struct dpp_arena *arena, struct dpp_type *next, u32 size)
{
	struct dpp_type *ty          = dpp_type_new(arena, TYPE_ARRAY);
	ty->ty_next                  = next;
	ty->ty_data.ty_array.size    = size;
	ty->ty_size                  = (next ? next->ty_size : 4) * size;
	ty->ty_align                 = next ? next->ty_align : 4;
	return ty;
}

struct dpp_type *dpp_type_function(struct dpp_arena *arena, struct dpp_type *ret_type, struct dpp_type **params, struct dpp_node *param_nodes, u32 num_params, bool is_variadic)
{
	struct dpp_type *ty = dpp_type_new(arena, TYPE_FUNCTION);
	ty->ty_next         = ret_type;
	ty->ty_data.ty_function.params      = params;
	ty->ty_data.ty_function.param_nodes = param_nodes;
	ty->ty_data.ty_function.num_params  = num_params;
	ty->ty_data.ty_function.is_variadic = is_variadic;
	ty->ty_size                         = 8;
	ty->ty_align                        = 8;
	return ty;
}

struct dpp_type *dpp_type_decimal(struct dpp_arena *arena, u32 bits)
{
	struct dpp_type *ty     = dpp_type_new(arena, TYPE_DECIMAL);
	ty->ty_data.ty_ext.bits = bits;
	ty->ty_size             = bits / 8;
	if (ty->ty_size == 0) ty->ty_size = 1;
	ty->ty_align = ty->ty_size;
	return ty;
}

struct dpp_type *dpp_type_xint(struct dpp_arena *arena, u32 bits, bool is_signed)
{
	struct dpp_type *ty          = dpp_type_new(arena, TYPE_EXT_INT);
	ty->ty_data.ty_ext.bits      = bits;
	ty->ty_data.ty_ext.is_signed = is_signed;
	ty->ty_size                  = (bits + 7) / 8;
	ty->ty_align                 = (bits > 32) ? 8 : 4;
	return ty;
}

bool dpp_types_are_compatible(struct dpp_type *t1, struct dpp_type *t2)
{
	if (!t1 || !t2) return false;
	if (t1->ty_kind != t2->ty_kind) return false;

	if (!dpp_types_are_compatible(t1->ty_next, t2->ty_next)) return false;

	switch (t1->ty_kind) {
	case TYPE_ARRAY:
		return t1->ty_data.ty_array.size == t2->ty_data.ty_array.size;
	case TYPE_FUNCTION:
		if (t1->ty_data.ty_function.num_params != t2->ty_data.ty_function.num_params) return false;
		for (u32 i = 0; i < t1->ty_data.ty_function.num_params; i++) {
			if (!dpp_types_are_compatible(t1->ty_data.ty_function.params[i], t2->ty_data.ty_function.params[i])) return false;
		}
		return true;
	case TYPE_EXT_INT:
		return t1->ty_data.ty_ext.bits == t2->ty_data.ty_ext.bits &&
		       t1->ty_data.ty_ext.is_signed == t2->ty_data.ty_ext.is_signed;
	default:
		return true;
	}
}

void dpp_type_set_base(struct dpp_type *chain, struct dpp_type *base) {
    if (!chain) return;
    struct dpp_type *curr = chain;
    while (curr->ty_next != NULL) {
        curr = curr->ty_next;
    }
    curr->ty_next = base;
}
