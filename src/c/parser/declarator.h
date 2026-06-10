#ifndef DPP_DECLARATOR_PARSER_H
#define DPP_DECLARATOR_PARSER_H

#include "c/parser/parser.h"
#include "core/sema/type.h"

struct declarator_result {
    struct dpp_type *type;
    const u8        *name;
    size_t           name_len;
};

// Ponto de entrada
struct declarator_result parse_declarator_full(struct dpp_parser *par, struct dpp_type *base_type);
struct dpp_type* parse_declarator_full_inner(struct dpp_parser *par, struct dpp_type *base_type, const u8 **name, size_t *len, bool *out_is_tight);

// Funções para teste unitário (expostas apenas para o harness)
struct dpp_type* parse_declarator_recursive(struct dpp_parser *par, struct dpp_type *base);
struct dpp_type* parse_direct_declarator(struct dpp_parser *par, struct dpp_type *base, const u8 **name, size_t *len);
struct dpp_type* parse_postfix_declarator(struct dpp_parser *par, struct dpp_type *base, bool is_tight_ptr);

#endif
