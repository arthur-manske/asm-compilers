#ifndef DPP_LEXER_H
#define DPP_LEXER_H

#include "core/types.h"
#include <stdbool.h>
#include <stddef.h>

struct dpp_lexer {
	const char *lex_filename;
	const u8   *lex_cursor;
	const u8   *lex_marker;
	const u8   *lex_limit;
	const u8   *lex_token;
	const u8   *lex_line_ptr; /* Início da linha atual para diagnósticos */

	u32         lex_line;
	u32         lex_column;
	bool        lex_is_bol;       /* Begin of Line */
	const void *lex_origin_macro; /* Evita recursão infinita */
	bool        lex_err;          /* Error occurred */
};

void dpp_lexer_init(struct dpp_lexer *lex, const char *filename, const u8 *data, size_t len);

/* Helper para diagnósticos */
void dpp_lexer_report_error(struct dpp_lexer *lex, const char *msg);

#endif
