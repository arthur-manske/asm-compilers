#ifndef DPP_PREPROC_H
#define DPP_PREPROC_H

#include "core/ast/ast.h"
#include "c/lexer/lexer.h"

struct dpp_macro_param {
	u8                     *par_name;
	size_t                  par_len;
	struct dpp_macro_param *par_next;
};

struct dpp_macro {
	u8                     *mac_name;
	size_t                  mac_len;
	u8                     *mac_body;
	size_t                  mac_body_len;
	struct dpp_macro_param *mac_params;
	bool                    mac_is_function;
	bool                    mac_is_variadic;
	struct dpp_macro       *mac_next;
};

struct dpp_pp_cond {
	bool                active;
	bool                has_taken;
	struct dpp_pp_cond *next;
};

struct dpp_preproc {
	struct dpp_arena   *pp_arena;
	struct dpp_macro  **pp_macros;
	u32                 pp_macro_count;
	struct dpp_lexer   *pp_lexer_stack[32];
	s32                 pp_lexer_top;
	char              **pp_include_paths;
	u32                 pp_include_path_count;
	u32                 pp_include_path_cap;
	char              **pp_included_files;
	u32                 pp_included_count;
	u32                 pp_included_cap;
	struct dpp_pp_cond *pp_cond_stack;
	u32                 pp_cond_depth;
};

void dpp_preproc_init(struct dpp_preproc *pp, struct dpp_arena *arena);
void dpp_preproc_add_include_path(struct dpp_preproc *pp, const char *path);
void dpp_preproc_push_lexer(struct dpp_preproc *pp, struct dpp_lexer *lex);
s32  dpp_preproc_next_token(struct dpp_preproc *pp, struct dpp_lexer **out_lex);

#endif
