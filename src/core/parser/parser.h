#ifndef DPP_CORE_PARSER_H
#define DPP_CORE_PARSER_H

#include "core/arena/arena.h"
#include "core/lexer/lexer.h"
#include "core/sema/symbol.h"
#include <stdbool.h>

struct dpp_parser;

struct dpp_parser {
    struct dpp_arena    par_arena;
    struct dpp_symtab   par_symtab;
    s32                 par_curr_tok;
    struct dpp_lexer   *par_curr_lex;
    bool                par_in_function;
    void               *par_context; /* Generic language-specific context */
    s32               (*par_next_tok)(struct dpp_parser *par);
};

void dpp_parser_init(struct dpp_parser *par, void *context, s32 (*next_tok)(struct dpp_parser *));
void dpp_parser_free(struct dpp_parser *par);

s32  dpp_parser_peek(struct dpp_parser *par);
s32  dpp_parser_consume(struct dpp_parser *par);
bool dpp_parser_expect(struct dpp_parser *par, s32 kind);

#endif
