#include "c/sema/layout.h"
#include "core/ast/ast.h"
#include "core/sema/type.h"
#include "core/target/target.h"

void dpp_layout_compute(struct dpp_node *st_node, struct dpp_target *target)
{
	if (!st_node) return;
	struct dpp_type *st_ty = (struct dpp_type *)st_node->nod_type;
	if (!st_ty) return;

	/* If already computed, skip */
	if (st_ty->ty_size > 0) return;

	u32  offset    = 0;
	u32  max_align = 1;
	u32  max_size  = 0;
	bool is_union  = (st_node->nod_kind == NOD_UNION_DECL);

	struct dpp_node *f = st_node->nod_child;
	while (f) {
		if (f->nod_kind != NOD_VAR_DECL) {
			f = f->nod_next;
			continue;
		}

		struct dpp_type *ty = (struct dpp_type *)f->nod_type;
		if (!ty) {
			f = f->nod_next;
			continue;
		}

		/* Recursive compute ONLY if member is another struct/union BY VALUE (not pointer) */
		if (ty->ty_kind == TYPE_STRUCT || ty->ty_kind == TYPE_UNION) {
			if (ty->ty_data.ty_struct.struct_node) {
				dpp_layout_compute(ty->ty_data.ty_struct.struct_node, target);
			}
		}

		u32 size  = ty->ty_size;
		u32 align = ty->ty_align;

		/* Protection for pointers or incomplete types */
		if (ty->ty_kind == TYPE_PTR) {
			size  = 8;
			align = 8;
		} else if (align == 0) {
			if (ty->ty_kind == TYPE_LONG) {
				size  = 8;
				align = 8;
			} else {
				size  = 4;
				align = 4;
			}
		}

		/* Align offset for structs, unions stay at 0 */
		if (!is_union) {
			offset              = (offset + align - 1) & ~(align - 1);
			f->nod_member_index = offset;
			offset += size;
		} else {
			f->nod_member_index = 0;
			if (size > max_size) max_size = size;
		}

		if (align > max_align) max_align = align;
		f = f->nod_next;
	}

	u32 final_size = is_union ? max_size : offset;
	/* Final struct alignment padding */
	final_size = (final_size + max_align - 1) & ~(max_align - 1);

	st_ty->ty_size  = final_size;
	st_ty->ty_align = max_align;
}
