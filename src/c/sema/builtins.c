#include "c/sema/builtins.h"
#include "core/sema/type.h"
#include <string.h>

void dpp_builtins_init(struct dpp_symtab *tab, struct dpp_arena *arena)
{
#define INJECT_BUILTIN(name, ret_kind, ...)                                                                            \
	{                                                                                                              \
		struct dpp_node *decl         = dpp_node_new(arena, NOD_FUNCTION_DECL, 0, 0);                          \
		decl->nod_data.nod_id.id_name = (const u8 *)strdup(name);                                              \
		decl->nod_data.nod_id.id_len  = strlen(name);                                                          \
		decl->nod_storage             = NOD_STORAGE_EXTERN;                                                    \
		decl->nod_type                = dpp_type_new(arena, ret_kind);                                         \
		/* Parameters could be expanded here if needed, keeping it simple for now */                           \
		dpp_symtab_insert(tab, decl->nod_data.nod_id.id_name, decl->nod_data.nod_id.id_len, decl);             \
	}

#include "builtins_def.inc"

	/* Manual injection of standard variadic tools as aliases for robustness */
	INJECT_BUILTIN("va_start", TYPE_VOID, TYPE_PTR, TYPE_INT)
	INJECT_BUILTIN("va_end", TYPE_VOID, TYPE_PTR)
	INJECT_BUILTIN("va_arg", TYPE_INT, TYPE_PTR, TYPE_INT)
	INJECT_BUILTIN("va_copy", TYPE_VOID, TYPE_PTR, TYPE_PTR)

#undef INJECT_BUILTIN
}
