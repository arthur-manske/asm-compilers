#define _POSIX_C_SOURCE 200809L
#include "c/preproc/preproc.h"
#include "c/lexer/lexer.h"
#include "c/lexer/token.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern s32 dpp_lexer_next(struct dpp_lexer *lex);

static u32 s_hash(const u8 *name, size_t len)
{
	u32 h = 5381;
	for (size_t i = 0; i < len; i++) h = ((h << 5) + h) + name[i];
	return h;
}

void dpp_preproc_init(struct dpp_preproc *pp, struct dpp_arena *arena)
{
	memset(pp, 0, sizeof(*pp));
	pp->pp_arena            = arena;
	pp->pp_macros           = (struct dpp_macro **)calloc(1024, sizeof(struct dpp_macro *));
	pp->pp_macro_count      = 1024;
	pp->pp_lexer_top        = -1;
	pp->pp_include_path_cap = 16;
	pp->pp_include_paths    = (char **)malloc(sizeof(char *) * pp->pp_include_path_cap);
	pp->pp_included_cap     = 64;
	pp->pp_included_files   = (char **)malloc(sizeof(char *) * pp->pp_included_cap);
	pp->pp_included_count   = 0;
	pp->pp_cond_depth       = 0;

	/* Built-in macros */
	{
		struct dpp_macro *m = dpp_arena_alloc(pp->pp_arena, sizeof(struct dpp_macro));
		memset(m, 0, sizeof(*m));
		m->mac_name        = (u8 *)"FY_EXPORT";
		m->mac_len         = 9;
		m->mac_is_function = false;
		m->mac_body        = (u8 *)"";
		m->mac_body_len    = 0;
		u32 h              = s_hash(m->mac_name, m->mac_len) % 1024;
		pp->pp_macros[h]   = m;
	}

#define s_add_macro(n, b) do { \
	struct dpp_macro *m_ = dpp_arena_alloc(pp->pp_arena, sizeof(struct dpp_macro)); \
	memset(m_, 0, sizeof(*m_)); \
	m_->mac_name = (u8 *)(n); \
	m_->mac_len = strlen(n); \
	m_->mac_body = (u8 *)(b); \
	m_->mac_body_len = strlen(b); \
	u32 h_ = s_hash(m_->mac_name, m_->mac_len) % 1024; \
	m_->mac_next = pp->pp_macros[h_]; \
	pp->pp_macros[h_] = m_; \
} while(0)

	s_add_macro("__GNUC__", "4");
	s_add_macro("__GNUC_MINOR__", "2");
	s_add_macro("__GNUC_PATCHLEVEL__", "1");
	s_add_macro("__clang__", "1");
	s_add_macro("__clang_major__", "19");
	s_add_macro("__clang_minor__", "1");
	s_add_macro("__clang_patchlevel__", "0");
	s_add_macro("__linux__", "1");
	s_add_macro("__linux", "1");
	s_add_macro("linux", "1");
	s_add_macro("__unix__", "1");
	s_add_macro("__unix", "1");
	s_add_macro("unix", "1");
	s_add_macro("__ELF__", "1");
	s_add_macro("__x86_64__", "1");
	s_add_macro("__x86_64", "1");
	s_add_macro("__amd64__", "1");
	s_add_macro("__amd64", "1");
	s_add_macro("__LP64__", "1");
	s_add_macro("_LP64", "1");
	s_add_macro("__SIZEOF_INT__", "4");
	s_add_macro("__SIZEOF_LONG__", "8");
	s_add_macro("__SIZEOF_POINTER__", "8");
	s_add_macro("__SIZEOF_SIZE_T__", "8");
	s_add_macro("__CHAR_BIT__", "8");
	s_add_macro("__INT_MAX__", "2147483647");
	s_add_macro("__LONG_MAX__", "9223372036854775807L");
	s_add_macro("__SIZE_MAX__", "18446744073709551615UL");
	s_add_macro("__SCHAR_MAX__", "127");
	s_add_macro("__SHRT_MAX__", "32767");
	s_add_macro("__INT_MIN__", "(-__INT_MAX__ - 1)");
	s_add_macro("__LONG_MIN__", "(-__LONG_MAX__ - 1L)");
	s_add_macro("__STDC__", "1");
	s_add_macro("__STDC_HOSTED__", "1");
	s_add_macro("__STDC_VERSION__", "199901L");
	s_add_macro("__STDC_IEC_559__", "1");
	s_add_macro("__STDC_ISO_10646__", "201505L");
	s_add_macro("__restrict", "");
	s_add_macro("__restrict__", "");
	s_add_macro("__inline", "inline");
	s_add_macro("__inline__", "inline");
	s_add_macro("__volatile", "volatile");
	s_add_macro("__volatile__", "volatile");
	s_add_macro("__const", "const");
	s_add_macro("__const__", "const");
	s_add_macro("__extension__", "");
	s_add_macro("__builtin_va_list", "void*");

#undef s_add_macro
}

void dpp_preproc_add_include_path(struct dpp_preproc *pp, const char *path)
{
	if (pp->pp_include_path_count >= pp->pp_include_path_cap) {
		pp->pp_include_path_cap *= 2;
		pp->pp_include_paths = (char **)realloc(pp->pp_include_paths, sizeof(char *) * pp->pp_include_path_cap);
	}
	pp->pp_include_paths[pp->pp_include_path_count++] = strdup(path);
}

void dpp_preproc_push_lexer(struct dpp_preproc *pp, struct dpp_lexer *lex)
{
	if (pp->pp_lexer_top >= 31) {
		fprintf(stderr, "FATAL: Preprocessor lexer stack overflow\n");
		exit(1);
	}
	pp->pp_lexer_stack[++pp->pp_lexer_top] = lex;
}

static bool s_is_skipping(struct dpp_preproc *pp)
{
	struct dpp_pp_cond *curr = pp->pp_cond_stack;
	while (curr) {
		if (!curr->active) return true;
		curr = curr->next;
	}
	return false;
}

static const u8 *s_skip_ws_c(const u8 *p, const u8 *end)
{
	while (p < end) {
		if (*p == ' ' || *p == '\t') {
			p++;
			continue;
		}
		if (p + 1 < end && p[0] == '/' && p[1] == '/') return end;
		if (p + 1 < end && p[0] == '/' && p[1] == '*') {
			p += 2;
			while (p < end) {
				if (p + 1 < end && p[0] == '*' && p[1] == '/') {
					p += 2;
					break;
				}
				p++;
			}
			continue;
		}
		break;
	}
	return p;
}

static bool s_eval_primary(struct dpp_preproc *pp, const u8 **ppos, const u8 *end);
static bool s_eval_unary(struct dpp_preproc *pp, const u8 **ppos, const u8 *end);
static bool s_eval_and(struct dpp_preproc *pp, const u8 **ppos, const u8 *end);

static bool s_eval_or(struct dpp_preproc *pp, const u8 **ppos, const u8 *end)
{
	bool      left = s_eval_and(pp, ppos, end);
	const u8 *p    = s_skip_ws_c(*ppos, end);
	while (p + 1 < end && p[0] == '|' && p[1] == '|') {
		p += 2;
		*ppos      = p;
		bool right = s_eval_and(pp, ppos, end);
		left       = left || right;
		p          = s_skip_ws_c(*ppos, end);
	}
	return left;
}

static bool s_eval_and(struct dpp_preproc *pp, const u8 **ppos, const u8 *end)
{
	bool      left = s_eval_unary(pp, ppos, end);
	const u8 *p    = s_skip_ws_c(*ppos, end);
	while (p + 1 < end && p[0] == '&' && p[1] == '&') {
		p += 2;
		*ppos      = p;
		bool right = s_eval_unary(pp, ppos, end);
		left       = left && right;
		p          = s_skip_ws_c(*ppos, end);
	}
	return left;
}

static bool s_eval_unary(struct dpp_preproc *pp, const u8 **ppos, const u8 *end)
{
	const u8 *p = s_skip_ws_c(*ppos, end);
	if (p < end && *p == '!') {
		p++;
		bool val = s_eval_unary(pp, &p, end);
		*ppos    = p;
		return !val;
	}
	return s_eval_primary(pp, ppos, end);
}

static bool s_eval_primary(struct dpp_preproc *pp, const u8 **ppos, const u8 *end)
{
	const u8 *p = s_skip_ws_c(*ppos, end);
	if (p >= end) return false;

	if (*p == '(') {
		p++;
		bool val = s_eval_or(pp, &p, end);
		p        = s_skip_ws_c(p, end);
		if (p < end && *p == ')') p++;
		*ppos = p;
		return val;
	}

	if (*p >= '0' && *p <= '9') {
		char *ep = NULL;
		long  n  = strtol((const char *)p, &ep, 0);
		if (ep > (const char *)p) {
			*ppos = (const u8 *)ep;
			return n != 0;
		}
	}

	if (end - p >= 7 && memcmp(p, "defined", 7) == 0) {
		p += 7;
		p          = s_skip_ws_c(p, end);
		bool paren = (p < end && *p == '(');
		if (paren) p++;
		p                  = s_skip_ws_c(p, end);
		const u8 *id_start = p;
		while (p < end &&
		       ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_'))
			p++;
		size_t id_len = p - id_start;
		if (paren) {
			p = s_skip_ws_c(p, end);
			if (p < end && *p == ')') p++;
		}
		*ppos = p;
		if (id_len > 0) {
			u32               h = s_hash(id_start, id_len) % pp->pp_macro_count;
			struct dpp_macro *m = pp->pp_macros[h];
			while (m) {
				if (m->mac_len == id_len && memcmp(m->mac_name, id_start, id_len) == 0) return true;
				m = m->mac_next;
			}
		}
		return false;
	}

	if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || *p == '_') {
		const u8 *id_start = p;
		while (p < end &&
		       ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_'))
			p++;
		size_t id_len       = p - id_start;
		*ppos               = p;
		u32               h = s_hash(id_start, id_len) % pp->pp_macro_count;
		struct dpp_macro *m = pp->pp_macros[h];
		while (m) {
			if (m->mac_len == id_len && memcmp(m->mac_name, id_start, id_len) == 0) {
					if (!m->mac_is_function) {
						/* Evaluate object-like macro body as integer */
						char *ep = NULL;
						long n   = strtol((const char *)m->mac_body, &ep, 0);
						(void)ep;
						return n != 0;
					} else {
						return true;
					}
			}
			m = m->mac_next;
		}
		return false;
	}

	*ppos = p;
	return false;
}

static bool s_eval_if_expr(struct dpp_preproc *pp, struct dpp_lexer *lex)
{
	const u8 *p   = lex->lex_cursor;
	const u8 *end = p;
	while (*end && *end != '\n') end++;
	const u8 *pos = p;
	return s_eval_or(pp, &pos, end);
}

static void s_handle_include(struct dpp_preproc *pp, struct dpp_lexer *lex)
{
	s32 tok = dpp_lexer_next(lex);
	if (tok != TOK_STRING && tok != '<') return;

	char filename[256];
	if (tok == TOK_STRING) {
		snprintf(filename, sizeof(filename), "%.*s", (int)(lex->lex_cursor - lex->lex_token - 2),
		         lex->lex_token + 1);
	} else {
		const u8 *start = lex->lex_cursor;
		while (*lex->lex_cursor && *lex->lex_cursor != '>' && *lex->lex_cursor != '\n') lex->lex_cursor++;
		snprintf(filename, sizeof(filename), "%.*s", (int)(lex->lex_cursor - start), start);
		if (*lex->lex_cursor == '>') lex->lex_cursor++;
	}

	char fullpath[PATH_MAX];
	bool found = false;
	if (access(filename, F_OK) == 0) {
		realpath(filename, fullpath);
		found = true;
	}
	if (!found) {
		for (u32 i = 0; i < pp->pp_include_path_count; i++) {
			char temp[PATH_MAX];
			snprintf(temp, sizeof(temp), "%s/%s", pp->pp_include_paths[i], filename);
			if (access(temp, F_OK) == 0) {
				realpath(temp, fullpath);
				found = true;
				break;
			}
		}
	}

	for (u32 i = 0; i < pp->pp_included_count; i++) {
		if (strcmp(pp->pp_included_files[i], fullpath) == 0) return;
	}

	if (found) {
		if (pp->pp_included_count >= pp->pp_included_cap) {
			pp->pp_included_cap *= 2;
			pp->pp_included_files = realloc(pp->pp_included_files, sizeof(char *) * pp->pp_included_cap);
		}
		pp->pp_included_files[pp->pp_included_count++] = strdup(fullpath);
		FILE *f = fopen(fullpath, "rb");
		if (f) {
			fseek(f, 0, SEEK_END);
			size_t size = ftell(f);
			fseek(f, 0, SEEK_SET);
			u8 *data = (u8 *)malloc(size + 1);
			fread(data, 1, size, f);
			data[size] = 0;
			fclose(f);
			struct dpp_lexer *new_lex =
				(struct dpp_lexer *)dpp_arena_alloc(pp->pp_arena, sizeof(struct dpp_lexer));
			dpp_lexer_init(new_lex, strdup(fullpath), data, size);
			dpp_preproc_push_lexer(pp, new_lex);
		}
	}
}

static void s_handle_define(struct dpp_preproc *pp, struct dpp_lexer *lex)
{
	s32 tok = dpp_lexer_next(lex);
	if (tok != TOK_IDENT) return;

	struct dpp_macro *m = dpp_arena_alloc(pp->pp_arena, sizeof(struct dpp_macro));
	memset(m, 0, sizeof(*m));
	m->mac_len = lex->lex_cursor - lex->lex_token;
	u8 *mname  = dpp_arena_alloc(pp->pp_arena, m->mac_len);
	memcpy(mname, lex->lex_token, m->mac_len);
	m->mac_name        = mname;
	m->mac_is_function = (*lex->lex_cursor == '(');

	if (m->mac_is_function) {
		lex->lex_cursor++;
		struct dpp_macro_param **last_p = &m->mac_params;
		while (true) {
			tok = dpp_lexer_next(lex);
			if (tok == TOK_IDENT) {
				struct dpp_macro_param *p =
					dpp_arena_alloc(pp->pp_arena, sizeof(struct dpp_macro_param));
				p->par_len = lex->lex_cursor - lex->lex_token;
				u8 *pname  = dpp_arena_alloc(pp->pp_arena, p->par_len);
				memcpy(pname, lex->lex_token, p->par_len);
				p->par_name = pname;
				p->par_next = NULL;
				*last_p     = p;
				last_p      = &p->par_next;
			} else if (tok == TOK_ELLIPSIS) {
				m->mac_is_variadic = true;
			} else if (tok == ')')
				break;

			if (*lex->lex_cursor == ',')
				lex->lex_cursor++;
			else if (*lex->lex_cursor == ')') {
				lex->lex_cursor++;
				break;
			}
		}
	}

	while (*lex->lex_cursor == ' ' || *lex->lex_cursor == '\t') lex->lex_cursor++;
	const u8 *body_start = lex->lex_cursor;
	while (*lex->lex_cursor && *lex->lex_cursor != '\n') {
		if (*lex->lex_cursor == '\\') {
			lex->lex_cursor++;
			if (*lex->lex_cursor == '\n') {
				lex->lex_line++;
			}
		}
		if (*lex->lex_cursor == '\\' &&
		    (lex->lex_cursor[1] == '\n' || (lex->lex_cursor[1] == '\r' && lex->lex_cursor[2] == '\n'))) {
			if (lex->lex_cursor[1] == '\r')
				lex->lex_cursor += 3;
			else
				lex->lex_cursor += 2;
			lex->lex_line++;
			continue;
		}
		lex->lex_cursor++;
	}
	m->mac_body_len = lex->lex_cursor - body_start;
	u8 *mbody       = dpp_arena_alloc(pp->pp_arena, m->mac_body_len + 1);
	memcpy(mbody, body_start, m->mac_body_len);
	mbody[m->mac_body_len] = 0;
	m->mac_body            = mbody;

	u32 h            = s_hash(m->mac_name, m->mac_len) % pp->pp_macro_count;
	m->mac_next      = pp->pp_macros[h];
	pp->pp_macros[h] = m;
}

static void s_handle_directive(struct dpp_preproc *pp, struct dpp_lexer *lex)
{
	s32 tok = dpp_lexer_next(lex);
	if (tok == TOK_NUMBER) {
		lex->lex_line = atoi((const char *)lex->lex_token);
		tok           = dpp_lexer_next(lex);
		if (tok == TOK_STRING) {
			size_t flen  = lex->lex_cursor - lex->lex_token - 2;
			char  *fname = malloc(flen + 1);
			memcpy(fname, lex->lex_token + 1, flen);
			fname[flen]       = 0;
			lex->lex_filename = fname;
		}
		goto end_line;
	}

	if (tok == TOK_IF) {
		bool                cond = s_eval_if_expr(pp, lex);
		{
			const u8 *p = lex->lex_cursor;
			while (*p == ' ' || *p == '\t') p++;
			fprintf(stderr, "DBG_IF %s:%u cond=%d expr=%.20s\n",
				lex->lex_filename ? lex->lex_filename : "?", lex->lex_line, cond, (const char *)p);
		}
		struct dpp_pp_cond *c    = dpp_arena_alloc(pp->pp_arena, sizeof(struct dpp_pp_cond));
		c->active                = cond;
		c->has_taken             = cond;
		c->next                  = pp->pp_cond_stack;
		pp->pp_cond_stack        = c;
		pp->pp_cond_depth++;
		goto end_line;
	}

	if (tok == TOK_ELSE) {
		if (pp->pp_cond_stack) {
			pp->pp_cond_stack->active    = !pp->pp_cond_stack->has_taken;
			pp->pp_cond_stack->has_taken = true;
		}
		goto end_line;
	}

	if (tok != TOK_IDENT) goto end_line;

	size_t    len  = lex->lex_cursor - lex->lex_token;
	const u8 *name = lex->lex_token;

	if (len == 7 && memcmp(name, "include", 7) == 0) {
		if (!s_is_skipping(pp)) s_handle_include(pp, lex);
	} else if (len == 6 && memcmp(name, "define", 6) == 0) {
		if (!s_is_skipping(pp)) {
			s_handle_define(pp, lex);
		}
	} else if (len == 6 && memcmp(name, "pragma", 6) == 0) {
		tok = dpp_lexer_next(lex);
		if (tok == TOK_IDENT) {
			size_t plen = lex->lex_cursor - lex->lex_token;
			if (plen == 4 && memcmp(lex->lex_token, "once", 4) == 0) {
				const char *fname = lex->lex_filename ? lex->lex_filename : "";
				for (u32 i = 0; i < pp->pp_included_count; i++) {
					if (strcmp(pp->pp_included_files[i], fname) == 0) goto end_line;
				}
				if (pp->pp_included_count >= pp->pp_included_cap) {
					pp->pp_included_cap *= 2;
					pp->pp_included_files =
						realloc(pp->pp_included_files, sizeof(char *) * pp->pp_included_cap);
				}
				pp->pp_included_files[pp->pp_included_count++] = strdup(fname);
			}
		}
	} else if (len == 6 && memcmp(name, "ifndef", 6) == 0 || len == 5 && memcmp(name, "ifdef", 5) == 0) {
		bool is_ifndef = (len == 6);
		tok            = dpp_lexer_next(lex);
		if (tok != TOK_IDENT) goto end_line;
		bool              defined = false;
		u32               h = s_hash(lex->lex_token, lex->lex_cursor - lex->lex_token) % pp->pp_macro_count;
		struct dpp_macro *m = pp->pp_macros[h];
		while (m) {
			if (m->mac_len == (size_t)(lex->lex_cursor - lex->lex_token) &&
			    memcmp(m->mac_name, lex->lex_token, m->mac_len) == 0) {
				defined = true;
				break;
			}
			m = m->mac_next;
		}
		struct dpp_pp_cond *c = dpp_arena_alloc(pp->pp_arena, sizeof(struct dpp_pp_cond));
		c->active             = is_ifndef ? !defined : defined;
		c->has_taken          = c->active;
		c->next               = pp->pp_cond_stack;
		pp->pp_cond_stack     = c;
		pp->pp_cond_depth++;
	} else if (len == 4 && memcmp(name, "elif", 4) == 0) {
		if (pp->pp_cond_stack) {
			struct dpp_pp_cond *c = pp->pp_cond_stack;
			if (c->has_taken) {
				c->active = false;
			} else {
				c->active    = s_eval_if_expr(pp, lex);
				c->has_taken = c->active;
			}
		}
	} else if (len == 4 && memcmp(name, "else", 4) == 0) {
		if (pp->pp_cond_stack) {
			pp->pp_cond_stack->active    = !pp->pp_cond_stack->has_taken;
			pp->pp_cond_stack->has_taken = true;
		}
	} else if (len == 5 && memcmp(name, "endif", 5) == 0) {
		if (pp->pp_cond_stack) {
			pp->pp_cond_stack = pp->pp_cond_stack->next;
			pp->pp_cond_depth--;
		}
	}

end_line:
	while (*lex->lex_cursor && *lex->lex_cursor != '\n') {
		if (*lex->lex_cursor == '\\') {
			lex->lex_cursor++;
			if (*lex->lex_cursor == '\n') {
				lex->lex_line++;
			}
		}
		if (*lex->lex_cursor == '\\' &&
		    (lex->lex_cursor[1] == '\n' || (lex->lex_cursor[1] == '\r' && lex->lex_cursor[2] == '\n'))) {
			if (lex->lex_cursor[1] == '\r')
				lex->lex_cursor += 3;
			else
				lex->lex_cursor += 2;
			lex->lex_line++;
			continue;
		}
		lex->lex_cursor++;
	}
	if (*lex->lex_cursor == '\n') {
		lex->lex_cursor++;
		lex->lex_line++;
	};
}

static u8 *s_expand_function_macro(struct dpp_preproc *pp, struct dpp_macro *m, struct dpp_lexer *lex, size_t *out_len)
{
    while (*lex->lex_cursor == ' ' || *lex->lex_cursor == '\t' || *lex->lex_cursor == '\n') {
        if (*lex->lex_cursor == '\n') lex->lex_line++;
        lex->lex_cursor++;
    }

	if (*lex->lex_cursor != '(') return NULL;
	lex->lex_cursor++;

	char *args[64];
	int   arg_count = 0;
	while (arg_count < 64) {
		while (*lex->lex_cursor == ' ' || *lex->lex_cursor == '\t' || *lex->lex_cursor == '\n')
			lex->lex_cursor++;
		const u8 *start       = lex->lex_cursor;
		int       paren_depth = 0;
		while (*lex->lex_cursor && (paren_depth > 0 || (*lex->lex_cursor != ',' && *lex->lex_cursor != ')'))) {
			if (*lex->lex_cursor == '(') paren_depth++;
			if (*lex->lex_cursor == ')') paren_depth--;
			lex->lex_cursor++;
		}
		size_t alen     = lex->lex_cursor - start;
		args[arg_count] = malloc(alen + 1);
		memcpy(args[arg_count], start, alen);
		args[arg_count][alen] = 0;
		arg_count++;
		if (*lex->lex_cursor == ',')
			lex->lex_cursor++;
		else if (*lex->lex_cursor == ')') {
			lex->lex_cursor++;
			break;
		} else
			break;
	}

	size_t result_cap = 4096;
	u8    *result     = malloc(result_cap);
	size_t res_len    = 0;

#define ENSURE_CAP(need)                                                                                               \
	do {                                                                                                           \
		while (res_len + (need) >= result_cap) {                                                               \
			result_cap *= 2;                                                                               \
			result = realloc(result, result_cap);                                                          \
		}                                                                                                      \
	} while (0)

	const u8 *p = m->mac_body;
	while (*p) {
		bool replaced = false;
		if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || *p == '_') {
			const u8 *id_start = p;
			while ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') ||
			       *p == '_')
				p++;
			size_t id_len = p - id_start;

			struct dpp_macro_param *param = m->mac_params;
			int                     idx   = 0;
			while (param) {
				if (param->par_len == id_len && memcmp(param->par_name, id_start, id_len) == 0) {
					if (idx < arg_count) {
						size_t alen = strlen(args[idx]);
						ENSURE_CAP(alen);
						memcpy(result + res_len, args[idx], alen);
						res_len += alen;
						replaced = true;
						break;
					}
				}
				param = param->par_next;
				idx++;
			}
			if (!replaced && m->mac_is_variadic && id_len == 11 &&
			    memcmp(id_start, "__VA_ARGS__", 11) == 0) {
				for (int i = idx; i < arg_count; i++) {
					size_t alen = strlen(args[i]);
					ENSURE_CAP(alen + 2);
					memcpy(result + res_len, args[i], alen);
					res_len += alen;
					if (i < arg_count - 1) {
						result[res_len++] = ',';
						result[res_len++] = ' ';
					}
				}
				replaced = true;
			}
			if (!replaced) {
				ENSURE_CAP(id_len);
				memcpy(result + res_len, id_start, id_len);
				res_len += id_len;
			}
		} else if (*p == '#') {
			/* Stringification: #param → "arg" */
			const u8 *after = p + 1;
			while (*after == ' ' || *after == '\t') after++;
			if ((*after >= 'a' && *after <= 'z') || (*after >= 'A' && *after <= 'Z') || *after == '_') {
				const u8 *id_start = after;
				while ((*after >= 'a' && *after <= 'z') || (*after >= 'A' && *after <= 'Z') ||
				       (*after >= '0' && *after <= '9') || *after == '_')
					after++;
				size_t                  id_len = after - id_start;
				struct dpp_macro_param *param  = m->mac_params;
				int                     idx    = 0;
				while (param) {
					if (param->par_len == id_len && memcmp(param->par_name, id_start, id_len) == 0) {
						if (idx < arg_count) {
							size_t alen = strlen(args[idx]);
							ENSURE_CAP(alen + 3);
							result[res_len++] = '"';
							memcpy(result + res_len, args[idx], alen);
							res_len += alen;
							result[res_len++] = '"';
							replaced = true;
							p        = after;
							break;
						}
					}
					param = param->par_next;
					idx++;
				}
			}
			if (!replaced) {
				ENSURE_CAP(1);
				result[res_len++] = *p++;
			}
		} else {
			ENSURE_CAP(1);
			result[res_len++] = *p++;
		}
	}
	ENSURE_CAP(1);
	result[res_len] = 0;
#undef ENSURE_CAP
	for (int i = 0; i < arg_count; i++) free(args[i]);
	*out_len = res_len;
	return result;
}

s32 dpp_preproc_next_token(struct dpp_preproc *pp, struct dpp_lexer **out_lex)
{
	for (;;) {
		if (pp->pp_lexer_top < 0) return TOK_EOF;
		struct dpp_lexer *lex = pp->pp_lexer_stack[pp->pp_lexer_top];
		s32               tok = dpp_lexer_next(lex);
		if (tok == TOK_EOF) {
			pp->pp_lexer_top--;
			continue;
		}

		if (tok == TOK_PP_HASH) {
			s_handle_directive(pp, lex);
			lex->lex_is_bol = true;
			continue;
		}

		if (s_is_skipping(pp)) {
			while (*lex->lex_cursor && *lex->lex_cursor != '\n') lex->lex_cursor++;
			if (*lex->lex_cursor == '\n') lex->lex_line++;
			lex->lex_is_bol = true;
			continue;
		}

		if (tok == TOK_IDENT) {
			u32 h = s_hash(lex->lex_token, lex->lex_cursor - lex->lex_token) % pp->pp_macro_count;
			struct dpp_macro *m = pp->pp_macros[h];
			while (m) {
				if (m->mac_len == (size_t)(lex->lex_cursor - lex->lex_token) &&
				    memcmp(m->mac_name, lex->lex_token, m->mac_len) == 0) {
					bool recursive = false;
					for (int i = pp->pp_lexer_top; i >= 0; i--)
						if (pp->pp_lexer_stack[i]->lex_origin_macro == m) {
							recursive = true;
							break;
						}
					if (recursive) break;

					if (!m->mac_is_function) {
						struct dpp_lexer *body_lex = (struct dpp_lexer *)dpp_arena_alloc(
							pp->pp_arena, sizeof(struct dpp_lexer));
						dpp_lexer_init(body_lex, "<macro>", m->mac_body, m->mac_body_len);
						body_lex->lex_origin_macro = m;
						dpp_preproc_push_lexer(pp, body_lex);
						goto next_token;
					}
					size_t elen     = 0;
					u8    *expanded = s_expand_function_macro(pp, m, lex, &elen);
					if (expanded) {
						struct dpp_lexer *body_lex =
							(struct dpp_lexer *)dpp_arena_alloc(
								pp->pp_arena, sizeof(struct dpp_lexer));
						dpp_lexer_init(body_lex, "<macro>", expanded, elen);
						body_lex->lex_origin_macro = m;
						dpp_preproc_push_lexer(pp, body_lex);
						goto next_token;
					}
				}
				m = m->mac_next;
			}
		}

		if (out_lex) *out_lex = lex;
		return tok;
	next_token:;
	}
}
