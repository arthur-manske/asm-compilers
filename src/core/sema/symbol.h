#ifndef DPP_SYMBOL_H
#define DPP_SYMBOL_H

#include "core/ast/ast.h"
#include "core/types.h"

struct dpp_symbol {
	const u8        *sym_name;
	size_t           sym_len;
	struct dpp_node *sym_node; /* Nó da AST que define o símbolo */

	struct dpp_symbol *sym_next; /* Próximo na bucket do hash */
};

struct dpp_scope {
	struct dpp_scope   *sco_parent;
	struct dpp_symbol **sco_buckets;
	u32                 sco_bucket_count;
};

struct dpp_symtab {
	struct dpp_scope *tab_curr;
	struct dpp_arena *tab_arena;
};

void dpp_symtab_init(struct dpp_symtab *tab, struct dpp_arena *arena);
void dpp_symtab_push(struct dpp_symtab *tab);
void dpp_symtab_pop(struct dpp_symtab *tab);

struct dpp_symbol *dpp_symtab_insert(struct dpp_symtab *tab, const u8 *name, size_t len, struct dpp_node *node);
struct dpp_symbol *dpp_symtab_lookup(struct dpp_symtab *tab, const u8 *name, size_t len);
struct dpp_symbol *dpp_symtab_lookup_local(struct dpp_symtab *tab, const u8 *name, size_t len);

#endif
