#ifndef DPP_AST_STREAM_H
#define DPP_AST_STREAM_H

#include "core/ast/ast.h"
#include "core/target/target.h"
#include "core/types.h"
#include <stdio.h>

/* Magic: "DACH" (D++ AST Chunked) */
#define DACH_MAGIC 0x48434144

enum dach_chunk_type {
	DACH_CHUNK_HEADER  = 0x01,
	DACH_CHUNK_SOURCE  = 0x10,
	DACH_CHUNK_AST     = 0x20,
	DACH_CHUNK_STRINGS = 0x30,
	DACH_CHUNK_SYMTAB  = 0x40,
	DACH_CHUNK_ABI     = 0x50,
	DACH_CHUNK_END     = 0xFF
};

struct dach_node_entry {
	u16 kind;
	u64 flags;
	u32 line;
	u32 col;
	u32 size;
	u32 align;
	u32 offset;
	s32 child_idx;
	s32 next_idx;
	u32 ptr_depth;
	u32 array_size;
	u32 type_flags;
	u32 type_bits;
	u64 data;
	f64 data_float;
};

struct dach_symbol_entry {
	u32  name_len;
	s32  node_idx;
	char name[];
};

/* Exporta/Importa a AST e Symtab */
struct dpp_symtab;
void dpp_ast_export(struct dpp_node *root, struct dpp_symtab *tab, struct dpp_target *target, const char *filename,
                    FILE *out);
struct dpp_node *dpp_ast_import(FILE *in, struct dpp_arena *arena, struct dpp_symtab *out_tab,
                                struct dpp_target *out_target);

#endif
