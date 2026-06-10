#include "core/parser/parser.h"
#include <stdio.h>
#include <stdlib.h>

void dpp_parser_init(struct dpp_parser *par, void *context, s32 (*next_tok)(struct dpp_parser *))
{
    par->par_context = context;
    par->par_next_tok = next_tok;
    dpp_arena_init(&par->par_arena, 1024 * 1024);
    dpp_symtab_init(&par->par_symtab, &par->par_arena);
    par->par_curr_tok = par->par_next_tok(par);
    par->par_in_function = false;
}

void dpp_parser_free(struct dpp_parser *par)
{
    dpp_arena_free(&par->par_arena);
}

s32 dpp_parser_peek(struct dpp_parser *par)
{
    return par->par_curr_tok;
}

s32 dpp_parser_consume(struct dpp_parser *par)
{
    s32 kind          = par->par_curr_tok;
    par->par_curr_tok = par->par_next_tok(par);
    return kind;
}

static u32 s_expect_id = 0;

bool dpp_parser_expect(struct dpp_parser *par, s32 kind)
{
    u32 id = ++s_expect_id;
    if (dpp_parser_peek(par) == kind) {
        dpp_parser_consume(par);
        return true;
    } else {
        struct dpp_lexer *l = par->par_curr_lex;
        s32 got = dpp_parser_peek(par);
        const u8 *tok_text = l ? l->lex_token : (const u8*)"?";
        size_t tok_len = l ? (size_t)(l->lex_cursor - l->lex_token) : 0;
        if (tok_len > 40) tok_len = 40;
        fprintf(stderr, "EXPECT_FAIL#%u tok=%d '%.*s' expected=%d at %s:%d\n",
                id, got, (int)tok_len, tok_text, kind,
                l ? l->lex_filename : "?", l ? l->lex_line : 0);
        /* Generic error reporting without C-specific knowledge */
        char buf[512];
        snprintf(buf, sizeof(buf), "unexpected token (got=%d '%.*s', expected=%d)",
                 got, (int)tok_len, tok_text, kind);
        dpp_lexer_report_error(par->par_curr_lex, buf);
        return false;
    }
}
