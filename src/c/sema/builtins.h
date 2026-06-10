#ifndef DPP_BUILTINS_H
#define DPP_BUILTINS_H

#include "core/ast/ast.h"
#include "core/sema/symbol.h"

void dpp_builtins_init(struct dpp_symtab *tab, struct dpp_arena *arena);

#endif
