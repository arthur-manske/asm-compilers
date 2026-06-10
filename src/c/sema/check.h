#ifndef DPP_SEMA_CHECK_H
#define DPP_SEMA_CHECK_H

#include "core/ast/ast.h"
#include "core/sema/symbol.h"
#include "core/target/target.h"

void dpp_check_top_level(struct dpp_node *root);
void dpp_analyze_types(struct dpp_node *node, struct dpp_symtab *tab, struct dpp_arena *arena,
                       struct dpp_target *target);
void dpp_eval_array_sizes(struct dpp_node *node);

#endif
