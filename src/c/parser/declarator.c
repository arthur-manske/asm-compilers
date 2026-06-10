#include "c/parser/declarator.h"
#include "c/parser/parser.h"
#include "c/lexer/token.h"
#include "core/ast/ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward declarations
struct dpp_type *parse_declarator_internal(struct dpp_parser *par, struct dpp_type *base, const u8 **name, size_t *len);

struct dpp_type *parse_type_spec(struct dpp_parser *par)
{
    struct dpp_type *res = NULL;
    while (true) {
        s32 tok = dpp_parser_peek(par);
        if (tok == TOK_INT) {
            dpp_parser_consume(par);
            if (!res) res = dpp_type_new(&par->par_arena, TYPE_INT);
        } else if (tok == TOK_VOID) {
            dpp_parser_consume(par);
            if (!res) res = dpp_type_new(&par->par_arena, TYPE_VOID);
        } else if (tok == TOK_CHAR) {
            dpp_parser_consume(par);
            if (!res) res = dpp_type_new(&par->par_arena, TYPE_CHAR);
        } else if (tok == TOK_DOUBLE) {
            dpp_parser_consume(par);
            if (!res) res = dpp_type_new(&par->par_arena, TYPE_DOUBLE);
        } else if (tok == TOK_FLOAT) {
            dpp_parser_consume(par);
            if (!res) res = dpp_type_new(&par->par_arena, TYPE_FLOAT);
        } else if (tok == TOK_LONG) {
            dpp_parser_consume(par);
            if (!res) res = dpp_type_new(&par->par_arena, TYPE_LONG);
        } else if (tok == TOK_SHORT) {
            dpp_parser_consume(par);
            if (!res) res = dpp_type_new(&par->par_arena, TYPE_INT);
        } else if (tok == TOK_IDENT) {
            // Check if it's a typedef
            struct dpp_symbol *sym = dpp_symtab_lookup(&par->par_symtab, par->par_curr_lex->lex_token,
                                                        par->par_curr_lex->lex_cursor - par->par_curr_lex->lex_token);
            if (sym && (sym->sym_node->nod_type_flags & NOD_TYPE_TYPEDEF)) {
                dpp_parser_consume(par);
                if (!res) res = dpp_type_new(&par->par_arena, TYPE_INT); // Representing typedef as INT for now
            } else {
                break;
            }
        } else if (tok == TOK_CONST || tok == TOK_VOLATILE || tok == TOK_RESTRICT) {
            dpp_parser_consume(par);
            if (res) {
                if (tok == TOK_CONST) res->ty_flags |= TY_CONST;
                else if (tok == TOK_VOLATILE) res->ty_flags |= TY_VOLATILE;
                else if (tok == TOK_RESTRICT) res->ty_flags |= TY_RESTRICT;
            }
        } else {
            break;
        }
    }
    return res;
}

void parse_function_params(struct dpp_parser *par, struct dpp_type ***out_params, struct dpp_node **out_param_nodes, u32 *out_count, bool *is_variadic)
{
    u32               cap    = 4;
    u32               count  = 0;
    *is_variadic = false;
    struct dpp_type **params = dpp_arena_alloc(&par->par_arena, sizeof(struct dpp_type *) * cap);
    struct dpp_node  *head   = NULL;
    struct dpp_node **last   = &head;

    if (dpp_parser_peek(par) != TOK_RPAREN) {
        while (dpp_parser_peek(par) != TOK_RPAREN && dpp_parser_peek(par) != TOK_EOF) {
            if (dpp_parser_peek(par) == TOK_ELLIPSIS) {
                *is_variadic = true;
                dpp_parser_consume(par);
                break;
            }
            if (count >= cap) {
                cap *= 2;
                struct dpp_type **new_params =
                    dpp_arena_alloc(&par->par_arena, sizeof(struct dpp_type *) * cap);
                memcpy(new_params, params, sizeof(struct dpp_type *) * count);
                params = new_params;
            }

            struct dpp_type *param_base = parse_type_spec(par);
            if (param_base) {
                struct declarator_result decl_res = parse_declarator_full(par, param_base);
                params[count++] = decl_res.type;
                
                struct dpp_node *pnode = dpp_node_new(&par->par_arena, NOD_PARAM_DECL, 0, 0);
                pnode->nod_type = decl_res.type;
                pnode->nod_data.nod_id.id_name = decl_res.name;
                pnode->nod_data.nod_id.id_len  = decl_res.name_len;
                
                *last = pnode;
                last = &pnode->nod_next;
            } else {
                if (dpp_parser_peek(par) == TOK_IDENT) dpp_parser_consume(par);
            }

            if (dpp_parser_peek(par) == TOK_COMMA) {
                dpp_parser_consume(par);
                if (dpp_parser_peek(par) == TOK_ELLIPSIS) {
                    *is_variadic = true;
                    dpp_parser_consume(par);
                    break;
                }
            } else {
                break;
            }
        }
    }

    *out_params      = params;
    *out_param_nodes = head;
    *out_count       = count;
}

struct dpp_type *parse_pointer(struct dpp_parser *par, struct dpp_type *inner)
{
    if (dpp_parser_peek(par) == TOK_STAR) {
        dpp_parser_consume(par);
        struct dpp_type *ptr = dpp_type_ptr(&par->par_arena, NULL);
        while (dpp_parser_peek(par) == TOK_CONST || dpp_parser_peek(par) == TOK_VOLATILE ||
               dpp_parser_peek(par) == TOK_RESTRICT) {
            s32 q = dpp_parser_peek(par);
            dpp_parser_consume(par);
            if (q == TOK_CONST) ptr->ty_flags |= TY_CONST;
            else if (q == TOK_RESTRICT) ptr->ty_flags |= TY_RESTRICT;
        }
        struct dpp_type *res = parse_pointer(par, inner);
        dpp_type_set_base(ptr, res);
        return ptr;
    }
    return inner;
}

struct dpp_type *parse_direct_declarator(struct dpp_parser *par, struct dpp_type *inner, const u8 **name, size_t *len)
{
    struct dpp_type *res = NULL;
    if (dpp_parser_peek(par) == TOK_LPAREN) {
        dpp_parser_consume(par);
        res = parse_declarator_internal(par, NULL, name, len);
        if (!dpp_parser_expect(par, TOK_RPAREN)) return NULL;
    } else if (dpp_parser_peek(par) == TOK_IDENT) {
        if (name) *name = par->par_curr_lex->lex_token;
        if (len)  *len  = par->par_curr_lex->lex_cursor - par->par_curr_lex->lex_token;
        dpp_parser_consume(par);
        res = NULL; // No modifiers yet
    }

    // Parse postfixes and wrap them around 'inner'?
    while (true) {
        if (dpp_parser_peek(par) == TOK_LBRACKET) {
            dpp_parser_consume(par);
            u32              size      = 0;
            struct dpp_node *size_expr = NULL;
            if (dpp_parser_peek(par) != TOK_RBRACKET) {
                size_expr = dpp_parser_parse_expr(par, 0);
                if (size_expr && size_expr->nod_kind == NOD_INT_LITERAL)
                    size = (u32)size_expr->nod_data.nod_val.val_int;
            }
            if (!dpp_parser_expect(par, TOK_RBRACKET)) return NULL;
            struct dpp_type *arr = dpp_type_array(&par->par_arena, NULL, size);
            if (size_expr && size_expr->nod_kind != NOD_INT_LITERAL)
                arr->ty_data.ty_array.node = size_expr;
            if (res) {
                dpp_type_set_base(res, arr);
            } else {
                res = arr;
            }
        } else if (dpp_parser_peek(par) == TOK_LPAREN) {
            dpp_parser_consume(par);
            u32               num_params  = 0;
            struct dpp_type **params      = NULL;
            struct dpp_node  *param_nodes = NULL;
            bool              is_variadic = false;
            if (dpp_parser_peek(par) != TOK_RPAREN) {
                parse_function_params(par, &params, &param_nodes, &num_params, &is_variadic);
            }
            if (!dpp_parser_expect(par, TOK_RPAREN)) return NULL;
            struct dpp_type *func = dpp_type_function(&par->par_arena, NULL, params, param_nodes, num_params, is_variadic);
            if (res) {
                dpp_type_set_base(res, func);
            } else {
                res = func;
            }
        } else {
            break;
        }
    }

    if (res) {
        dpp_type_set_base(res, inner);
        return res;
    }
    return inner;
}

struct dpp_type *parse_declarator_internal(struct dpp_parser *par, struct dpp_type *base, const u8 **name, size_t *len)
{
    struct dpp_type *ptr = parse_pointer(par, NULL);
    struct dpp_type *full = parse_direct_declarator(par, ptr, name, len);
    if (!full) return NULL;
    if (base) {
        dpp_type_set_base(full, base);
    }
    return full;
}

struct declarator_result parse_declarator_full(struct dpp_parser *par, struct dpp_type *base_type)
{
    struct declarator_result res = {0};
    res.type = parse_declarator_internal(par, base_type, &res.name, &res.name_len);
    return res;
}
