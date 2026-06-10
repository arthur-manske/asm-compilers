#ifndef DPP_CG_LLVM_H
#define DPP_CG_LLVM_H

#include "core/ast/ast.h"
#include "core/sema/symbol.h"
#include <llvm-c/Analysis.h>
#include <llvm-c/BitWriter.h>
#include <llvm-c/Core.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>

struct dpp_codegen {
	LLVMContextRef     context;
	LLVMModuleRef      module;
	LLVMBuilderRef     builder;
	struct dpp_symtab *symtab;
	struct dpp_arena  *arena;
	struct dpp_node   *current_func;
};

void dpp_codegen_init(struct dpp_codegen *cg, const char *module_name, struct dpp_symtab *tab, struct dpp_arena *arena);

void dpp_codegen_free(struct dpp_codegen *cg);
void dpp_codegen_emit(struct dpp_codegen *cg, struct dpp_node *root, const char *out_file);

#endif
