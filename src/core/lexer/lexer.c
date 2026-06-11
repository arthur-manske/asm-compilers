#include "core/lexer/lexer.h"
#include "core/logger/logger.h"
#include <stdio.h>
#include <string.h>

void
dpp_lexer_init(struct dpp_lexer *lex, const char *filename, const u8 *data, size_t len)
{
    lex->lex_filename = filename;
    lex->lex_cursor   = data;
    lex->lex_token    = data;
    lex->lex_limit    = data + len;
    lex->lex_marker   = NULL;
    lex->lex_line_ptr = data;
    lex->lex_line     = 1;
    lex->lex_column = 1;
    lex->lex_is_bol = true;
    lex->lex_origin_macro = NULL;
    lex->lex_err = false;
}

extern void dpp_diag_report_error(void);

void
dpp_lexer_report_error(struct dpp_lexer *lex, const char *msg)
{
    lex->lex_err = true;
    dpp_diag_report_error();
    clog_source src = {
        .file = lex->lex_filename,
        .line = lex->lex_line,
        .col = lex->lex_column
    };
    clog_error(src, "%s", msg);
}
