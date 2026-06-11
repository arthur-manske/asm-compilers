#include "c/parser/parser.h"
#include "core/ast/ast.h"
#include "c/lexer/token.h"
#include "core/diagnostic/diag.h"
#include "c/parser/declarator.h"
#include "core/sema/type.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

s32 c_parser_next_tok(struct dpp_parser *par) {
    struct dpp_preproc *pp = (struct dpp_preproc *)par->par_context;
    return dpp_preproc_next_token(pp, &par->par_curr_lex);
}

void dpp_c_parser_init(struct dpp_parser *par, struct dpp_preproc *pp) {
    dpp_parser_init(par, pp, c_parser_next_tok);
}

static bool             s_is_specifier(s32 tok);
static bool             s_is_type(struct dpp_parser *par);
struct dpp_node *dpp_parser_parse_expr(struct dpp_parser *par, int min_prec);
static struct dpp_node *s_parse_statement(struct dpp_parser *par);
static struct dpp_node *s_parse_do_while_stmt(struct dpp_parser *par);
static struct dpp_node *s_parse_compound_statement(struct dpp_parser *par);
static struct dpp_node *s_parse_declaration(struct dpp_parser *par, struct dpp_node *prefix_attrs);
static struct dpp_node *s_parse_struct_union_specifier(struct dpp_parser *par);
static struct dpp_node *s_parse_enum_specifier(struct dpp_parser *par);
static struct dpp_node *s_parse_parameter_list(struct dpp_parser *par, bool *out_is_variadic);
static void             s_parse_gcc_attribute(struct dpp_parser *par, struct dpp_node *node);
static void             s_parse_c23_attribute(struct dpp_parser *par, struct dpp_node *attr_node);
static struct dpp_node *s_parse_asm_stmt(struct dpp_parser *par);
static struct dpp_node *s_parse_initializer(struct dpp_parser *par);
static struct dpp_node *s_parse_declarator(struct dpp_parser *par, struct dpp_node *base_type_node);

static size_t s_unescape_string(u8 *dest, const u8 *src, size_t len)
{
	size_t i = 0, j = 0;
	while (i < len) {
		if (src[i] == '\\' && i + 1 < len) {
			i++;
			switch (src[i]) {
			case 'n':
				dest[j++] = '\n';
				i++;
				break;
			case 'r':
				dest[j++] = '\r';
				i++;
				break;
			case 't':
				dest[j++] = '\t';
				i++;
				break;
			case 'v':
				dest[j++] = '\v';
				i++;
				break;
			case 'f':
				dest[j++] = '\f';
				i++;
				break;
			case 'b':
				dest[j++] = '\b';
				i++;
				break;
			case 'a':
				dest[j++] = '\a';
				i++;
				break;
			case '\\':
				dest[j++] = '\\';
				i++;
				break;
			case '\"':
				dest[j++] = '\"';
				i++;
				break;
			case '\'':
				dest[j++] = '\'';
				i++;
				break;
			case 'x': {
				i++;
				u8 val = 0;
				for (int k = 0; k < 2 && i < len; k++) {
					u8 c = src[i];
					if (c >= '0' && c <= '9')
						val = (val << 4) | (c - '0');
					else if (c >= 'a' && c <= 'f')
						val = (val << 4) | (c - 'a' + 10);
					else if (c >= 'A' && c <= 'F')
						val = (val << 4) | (c - 'A' + 10);
					else
						break;
					i++;
				}
				dest[j++] = val;
				break;
			}
			default: {
				if (src[i] >= '0' && src[i] <= '7') {
					u8 val = 0;
					for (int k = 0; k < 3 && i < len; k++) {
						u8 c = src[i];
						if (c >= '0' && c <= '7')
							val = (val << 3) | (c - '0');
						else
							break;
						i++;
					}
					dest[j++] = val;
				} else
					dest[j++] = src[i++];
				break;
			}
			}
		} else
			dest[j++] = src[i++];
	}
	return j;
}

static bool s_is_specifier(s32 tok)
{
	switch (tok) {
	case TOK_INT:
	case TOK_CHAR:
	case TOK_VOID:
	case TOK_FLOAT:
	case TOK_DOUBLE:
	case TOK_CONST:
	case TOK_VOLATILE:
	case TOK_STATIC:
	case TOK_EXTERN:
	case TOK_TYPEDEF:
	case TOK_UNSIGNED:
	case TOK_SIGNED:
	case TOK_LONG:
	case TOK_SHORT:
	case TOK_STRUCT:
	case TOK_UNION:
	case TOK_ENUM:
	case TOK_INLINE:
	case TOK_RESTRICT:
	case TOK_BOOL:
	case TOK_TRUE:
	case TOK_FALSE:
	case TOK_COMPLEX:
	case TOK_ATTRIBUTE:
	case TOK_IMAGINARY:
	case TOK_ACCUM:
	case TOK_UACCUM:
	case TOK_DECIMAL_TYPE:
	case TOK_TYPEOF:
	case TOK_BUILTIN_VA_LIST:
		return true;
	default:
		return false;
	}
}

static int s_get_prec(s32 tok)
{
	switch (tok) {
	case TOK_ASSIGN:
	case TOK_ADD_ASSIGN:
	case TOK_SUB_ASSIGN:
	case TOK_MUL_ASSIGN:
	case TOK_DIV_ASSIGN:
	case TOK_MOD_ASSIGN:
	case TOK_LSHIFT_ASSIGN:
	case TOK_RSHIFT_ASSIGN:
	case TOK_AND_ASSIGN:
	case TOK_XOR_ASSIGN:
	case TOK_OR_ASSIGN:
		return 1;
	case TOK_QUERY:
		return 2;
	case TOK_OR:
		return 3;
	case TOK_AND:
		return 4;
	case TOK_PIPE:
		return 5;
	case TOK_CARET:
		return 6;
	case TOK_AMP:
		return 7;
	case TOK_EQ:
	case TOK_NE:
		return 8;
	case TOK_LT:
	case TOK_GT:
	case TOK_LE:
	case TOK_GE:
		return 9;
	case TOK_LSHIFT:
	case TOK_RSHIFT:
		return 10;
	case TOK_PLUS:
	case TOK_MINUS:
		return 11;
	case TOK_STAR:
	case TOK_DIV:
	case TOK_MOD:
		return 12;
	case TOK_DOT:
	case TOK_ARROW:
		return 15;
	case TOK_LBRACKET:
		return 15;
	case TOK_INC:
	case TOK_DEC:
		return 15;
	case TOK_LPAREN:
		return 15;
	default:
		return -1;
	}
}

static struct dpp_node *s_parse_primary_expr(struct dpp_parser *par)
{
	struct dpp_lexer *lex  = par->par_curr_lex;
	u32               line = lex->lex_line;
	u32               col  = lex->lex_column;
	s32               tok  = dpp_parser_peek(par);
	if (tok == TOK_NUMBER) {
		struct dpp_node *node          = dpp_node_new(&par->par_arena, NOD_INT_LITERAL, line, col);
		node->nod_data.nod_val.val_int = atoll((const char *)lex->lex_token);
		dpp_parser_consume(par);
		return node;
	}
	if (tok == TOK_CHAR_LITERAL) {
		struct dpp_node *node = dpp_node_new(&par->par_arena, NOD_INT_LITERAL, line, col);
		u8               val;
		s_unescape_string(&val, lex->lex_token + 1, lex->lex_cursor - lex->lex_token - 2);
		node->nod_data.nod_val.val_int = val;
		dpp_parser_consume(par);
		return node;
	}
	if (tok == TOK_TRUE || tok == TOK_FALSE) {
		struct dpp_node *node          = dpp_node_new(&par->par_arena, NOD_INT_LITERAL, line, col);
		node->nod_data.nod_val.val_int = (tok == TOK_TRUE) ? 1 : 0;
		dpp_parser_consume(par);
		return node;
	}
	if (DPP_IS_STRING_LITERAL(tok)) {
		struct dpp_node *node       = dpp_node_new(&par->par_arena, NOD_STRING_LITERAL, line, col);
        
        enum dpp_string_type str_type = STR_NORMAL;
        if (tok == TOK_STRING_UTF8) str_type = STR_UTF8;
        else if (tok == TOK_STRING_UTF16) str_type = STR_UTF16;
        else if (tok == TOK_STRING_UTF32) str_type = STR_UTF32;
        else if (tok == TOK_STRING_WIDE) str_type = STR_WIDE;
        
		size_t           total_len  = 0;
		u8              *concat_buf = NULL;
		while (DPP_IS_STRING_LITERAL(dpp_parser_peek(par))) {
			struct dpp_lexer *cur_lex   = par->par_curr_lex;
			size_t            raw_len   = cur_lex->lex_cursor - cur_lex->lex_token - 2;
			u8               *unescaped = dpp_arena_alloc(&par->par_arena, raw_len + 1);
			size_t            final_len = s_unescape_string(unescaped, cur_lex->lex_token + 1, raw_len);
			if (total_len == 0) {
				concat_buf = dpp_arena_alloc(&par->par_arena, final_len + 1);
				memcpy(concat_buf, unescaped, final_len);
				total_len = final_len;
			} else {
				u8 *new_buf = dpp_arena_alloc(&par->par_arena, total_len + final_len + 1);
				memcpy(new_buf, concat_buf, total_len);
				memcpy(new_buf + total_len, unescaped, final_len);
				total_len += final_len;
				concat_buf = new_buf;
			}
			dpp_parser_consume(par);
		}
        node->nod_data.nod_str.str_val = concat_buf;
        node->nod_data.nod_str.str_len = total_len;
        node->nod_data.nod_str.str_type = str_type;
        if (concat_buf) concat_buf[total_len] = 0;
		return node;
	}
	if (tok == TOK_IDENT) {
		struct dpp_node *node         = dpp_node_new(&par->par_arena, NOD_IDENTIFIER, line, col);
		node->nod_data.nod_id.id_name = lex->lex_token;
		node->nod_data.nod_id.id_len  = lex->lex_cursor - lex->lex_token;
		dpp_parser_consume(par);
		return node;
	}
	if (tok == TOK_LPAREN) {
		dpp_parser_consume(par);
		if (s_is_type(par)) {
			struct dpp_node *node         = dpp_node_new(&par->par_arena, NOD_CAST_EXPR, line, col);
			node->nod_data.nod_id.id_type = dpp_parser_peek(par);
			while (s_is_type(par)) {
				s32 t = dpp_parser_peek(par);
				dpp_parser_consume(par);
				if ((t == TOK_STRUCT || t == TOK_UNION || t == TOK_ENUM) && dpp_parser_peek(par) == TOK_IDENT)
					dpp_parser_consume(par);
			}
			u32 ptr_depth = 0;
			while (dpp_parser_peek(par) == TOK_STAR) {
				dpp_parser_consume(par);
				ptr_depth++;
			}
			node->nod_ptr_depth = ptr_depth;
			if (!dpp_parser_expect(par, ')')) return NULL;
			node->nod_child = dpp_parser_parse_expr(par, 14);
			return node;
		}
		if (dpp_parser_peek(par) == TOK_LBRACE) {
			struct dpp_node *node = dpp_node_new(&par->par_arena, NOD_STMT_EXPR, line, col);
			node->nod_child       = s_parse_compound_statement(par);
			if (!dpp_parser_expect(par, TOK_RPAREN)) return NULL;
			return node;
		}
		struct dpp_node *node = dpp_parser_parse_expr(par, 0);
		if (!dpp_parser_expect(par, ')')) return NULL;
		return node;
	}
	if (tok == TOK_AMP || tok == TOK_STAR || tok == TOK_BANG || tok == TOK_TILDE || tok == TOK_MINUS ||
	    tok == TOK_PLUS || tok == TOK_INC || tok == TOK_DEC) {
		dpp_parser_consume(par);
		s32              op           = tok;
		struct dpp_node *operand      = dpp_parser_parse_expr(par, 14);
		struct dpp_node *node         = dpp_node_new(&par->par_arena, NOD_UNARY_EXPR, line, col);
		node->nod_data.nod_op.op_kind = op;
		node->nod_child               = operand;
		return node;
	}
	if (tok == TOK_SIZEOF) {
		dpp_parser_consume(par);
		bool has_paren = (dpp_parser_peek(par) == '(');
		if (has_paren) dpp_parser_consume(par);
		struct dpp_node *node = dpp_node_new(&par->par_arena, NOD_SIZEOF, line, col);
		if (s_is_type(par)) {
			struct dpp_node *dummy     = dpp_node_new(&par->par_arena, NOD_VAR_DECL, line, col);
			s32              base_type = 0;
			struct dpp_node *st_node   = NULL;
			if (dpp_parser_peek(par) == TOK_STRUCT || dpp_parser_peek(par) == TOK_UNION) {
				st_node   = s_parse_struct_union_specifier(par);
				base_type = (st_node->nod_kind == NOD_STRUCT_DECL) ? TOK_STRUCT : TOK_UNION;
			} else if (dpp_parser_peek(par) == TOK_ENUM) {
				dpp_parser_consume(par);
				base_type = TOK_ENUM;
				if (dpp_parser_peek(par) == TOK_IDENT) dpp_parser_consume(par);
			} else {
				while (s_is_type(par)) {
					lex   = par->par_curr_lex;
					s32 t = dpp_parser_peek(par);
					if (t == TOK_STRUCT || t == TOK_UNION) {
						st_node = s_parse_struct_union_specifier(par);
						base_type =
							(st_node->nod_kind == NOD_STRUCT_DECL) ? TOK_STRUCT : TOK_UNION;
						continue;
					}
					if (t == TOK_IDENT) {
						struct dpp_symbol *sym =
							dpp_symtab_lookup(&par->par_symtab, lex->lex_token,
						                          lex->lex_cursor - lex->lex_token);
						if (sym && (sym->sym_node->nod_type_flags & NOD_TYPE_TYPEDEF)) {
							base_type = TOK_TYPEDEF;
							st_node   = sym->sym_node;
							dpp_parser_consume(par);
							continue;
						} else
							break;
					}
					base_type = dpp_parser_consume(par);
				}
			}
			u32 ptr_depth = 0;
			while (dpp_parser_peek(par) == TOK_STAR) {
				dpp_parser_consume(par);
				ptr_depth++;
			}
			dummy->nod_data.nod_id.id_type = base_type;
			dummy->nod_ptr_depth           = ptr_depth;
			dummy->nod_type_node           = st_node;
			node->nod_child                = dummy;
		} else
			node->nod_child = dpp_parser_parse_expr(par, 14);
		if (has_paren) dpp_parser_expect(par, ')');
		return node;
	}
	return NULL;
}

static struct dpp_node *s_parse_declarator(struct dpp_parser *par, struct dpp_node *base_type_node)
{
	struct declarator_result res = parse_declarator_full(par, NULL);
	
	struct dpp_node *head = NULL;
	if (res.name) {
		head = dpp_node_new(&par->par_arena, NOD_IDENTIFIER, par->par_curr_lex->lex_line, par->par_curr_lex->lex_column);
		head->nod_data.nod_id.id_name = res.name;
		head->nod_data.nod_id.id_len  = res.name_len;
	} else {
		head = dpp_node_new(&par->par_arena, NOD_INVALID, par->par_curr_lex->lex_line, par->par_curr_lex->lex_column);
	}

	struct dpp_type *curr = res.type;
	while (curr) {
		struct dpp_node *parent = NULL;
		if (curr->ty_kind == TYPE_PTR) {
			parent = dpp_node_new(&par->par_arena, NOD_DECLARATOR, head->nod_line, head->nod_column);
			parent->nod_data.nod_op.op_kind = dpp_token_to_op(TOK_STAR);

		} else if (curr->ty_kind == TYPE_ARRAY) {
			parent = dpp_node_new(&par->par_arena, NOD_INDEX_EXPR, head->nod_line, head->nod_column);
			parent->nod_array_size = curr->ty_data.ty_array.size;
		} else if (curr->ty_kind == TYPE_FUNCTION) {
			parent = dpp_node_new(&par->par_arena, NOD_FUNCTION_DECL, head->nod_line, head->nod_column);
			// TODO: map parameters to node children if needed, but s_parse_declaration might handle it
		}
		
		if (parent) {
			parent->nod_child = head;
			head = parent;
		}
		curr = curr->ty_next;
	}

	return head;
}

static struct dpp_node *s_parse_initializer(struct dpp_parser *par)
{
	struct dpp_lexer *lex  = par->par_curr_lex;
	u32               line = lex->lex_line;
	u32               col  = lex->lex_column;
	if (dpp_parser_peek(par) == TOK_LBRACE) {
		dpp_parser_consume(par);
		struct dpp_node  *list = dpp_node_new(&par->par_arena, NOD_INIT_LIST, line, col);
		struct dpp_node **last = &list->nod_child;
		while (dpp_parser_peek(par) != TOK_EOF && dpp_parser_peek(par) != TOK_RBRACE) {
			struct dpp_node *item = s_parse_initializer(par);
			if (item) {
				*last = item;
				while (*last) last = &(*last)->nod_next;
			} else {
				dpp_parser_consume(par);
			}

			if (dpp_parser_peek(par) == TOK_COMMA)
				dpp_parser_consume(par);
			else
				break;
		}
		dpp_parser_expect(par, TOK_RBRACE);
		return list;
	}
	if (dpp_parser_peek(par) == TOK_DOT) {
		dpp_parser_consume(par);
		struct dpp_node *node         = dpp_node_new(&par->par_arena, NOD_DESIGNATED_INIT, line, col);
		node->nod_data.nod_id.id_name = lex->lex_token;
		node->nod_data.nod_id.id_len  = lex->lex_cursor - lex->lex_token;
		dpp_parser_expect(par, TOK_IDENT);
		dpp_parser_expect(par, TOK_ASSIGN);
		node->nod_child = s_parse_initializer(par);
		return node;
	}
	return dpp_parser_parse_expr(par, 0);
}


struct dpp_node *dpp_parser_parse_expr(struct dpp_parser *par, int min_prec)
{
	struct dpp_lexer *lex = par->par_curr_lex;
	struct dpp_node  *lhs = s_parse_primary_expr(par);
	if (!lhs) return NULL;
	for (;;) {
		s32 tok = dpp_parser_peek(par);
		if (tok == TOK_DOT || tok == TOK_ARROW) {
			dpp_parser_consume(par);
			struct dpp_node *node =
				dpp_node_new(&par->par_arena, NOD_BINARY_EXPR, lhs->nod_line, lhs->nod_column);
			node->nod_data.nod_op.op_kind = tok;
			node->nod_data.nod_op.op_lhs  = lhs;
			if (dpp_parser_peek(par) != TOK_IDENT) {
				dpp_diag_error(lhs, "expected identifier after operator");
				return lhs;
			}
			struct dpp_node *member =
				dpp_node_new(&par->par_arena, NOD_IDENTIFIER, lex->lex_line, lex->lex_column);
			member->nod_data.nod_id.id_name = lex->lex_token;
			member->nod_data.nod_id.id_len  = lex->lex_cursor - lex->lex_token;
			dpp_parser_consume(par);
			node->nod_data.nod_op.op_rhs = member;
			lhs                          = node;
			continue;
		}
		if (tok == TOK_LBRACKET) {
			dpp_parser_consume(par);
			struct dpp_node *node =
				dpp_node_new(&par->par_arena, NOD_INDEX_EXPR, lhs->nod_line, lhs->nod_column);
			node->nod_data.nod_op.op_lhs = lhs;
			node->nod_data.nod_op.op_rhs = dpp_parser_parse_expr(par, 0);
			dpp_parser_expect(par, TOK_RBRACKET);
			lhs = node;
			continue;
		}
		if (tok == TOK_LPAREN) {
			dpp_parser_consume(par);
			struct dpp_node *node =
				dpp_node_new(&par->par_arena, NOD_BINARY_EXPR, lhs->nod_line, lhs->nod_column);
			node->nod_data.nod_op.op_kind = OP_CALL;

			node->nod_data.nod_op.op_lhs  = lhs;
			struct dpp_node **last        = &node->nod_data.nod_op.op_rhs;
			while (dpp_parser_peek(par) != TOK_EOF && dpp_parser_peek(par) != ')') {
				struct dpp_node *arg = dpp_parser_parse_expr(par, 0);
				if (arg) {
					*last = arg;
					last  = &arg->nod_next;
				}
				if (dpp_parser_peek(par) == TOK_COMMA)
					dpp_parser_consume(par);
				else
					break;
			}
			dpp_parser_expect(par, ')');
			lhs = node;
			continue;
		}
		if (tok == TOK_INC || tok == TOK_DEC) {
			dpp_parser_consume(par);
			struct dpp_node *node =
				dpp_node_new(&par->par_arena, NOD_UNARY_EXPR, lhs->nod_line, lhs->nod_column);
			node->nod_data.nod_op.op_kind = (tok == TOK_INC) ? dpp_token_to_op(TOK_INC) : dpp_token_to_op(TOK_DEC);
			node->nod_is_postfix          = true;

			node->nod_child               = lhs;
			lhs                           = node;
			continue;
		}
		int prec = s_get_prec(tok);
		if (prec < min_prec) break;
		dpp_parser_consume(par);
		if (tok == TOK_QUERY) {
			struct dpp_node *then_arm = dpp_parser_parse_expr(par, 0);
			dpp_parser_expect(par, ':');
			struct dpp_node *else_arm = dpp_parser_parse_expr(par, prec);
			struct dpp_node *node =
				dpp_node_new(&par->par_arena, NOD_TERNARY_EXPR, lhs->nod_line, lhs->nod_column);
			node->nod_data.nod_op.op_cond = lhs;
			node->nod_data.nod_op.op_lhs  = then_arm;
			node->nod_data.nod_op.op_rhs  = else_arm;
			lhs                           = node;
			continue;
		}
		struct dpp_node *rhs = dpp_parser_parse_expr(par, (tok == TOK_ASSIGN) ? prec : prec + 1);
		struct dpp_node *new_lhs =
			dpp_node_new(&par->par_arena, NOD_BINARY_EXPR, lhs->nod_line, lhs->nod_column);
		new_lhs->nod_data.nod_op.op_kind = dpp_token_to_op(tok);
		new_lhs->nod_data.nod_op.op_lhs  = lhs;
		new_lhs->nod_data.nod_op.op_rhs  = rhs;
		lhs                              = new_lhs;
	}
	return lhs;
}

static struct dpp_node *s_parse_do_while_stmt(struct dpp_parser *par)
{
	struct dpp_lexer *lex  = par->par_curr_lex;
	u32               line = lex->lex_line;
	u32               col  = lex->lex_column;
	dpp_parser_consume(par);
	struct dpp_node *body = s_parse_statement(par);
	if (!dpp_parser_expect(par, TOK_WHILE)) return NULL;
	if (!dpp_parser_expect(par, TOK_LPAREN)) return NULL;
	struct dpp_node *cond = dpp_parser_parse_expr(par, 0);
	if (!dpp_parser_expect(par, TOK_RPAREN)) return NULL;
	if (!dpp_parser_expect(par, TOK_SEMICOLON)) return NULL;
	struct dpp_node *node = dpp_node_new(&par->par_arena, NOD_DO_WHILE_STMT, line, col);
	node->nod_child       = body;
	body->nod_next        = cond;
	return node;
}

static struct dpp_node *s_parse_statement(struct dpp_parser *par)
{
	struct dpp_lexer *lex  = par->par_curr_lex;
	u32               line = lex->lex_line;
	u32               col  = lex->lex_column;
	s32               tok  = dpp_parser_peek(par);
	if (tok == TOK_ASM) {
        dpp_parser_consume(par);
        return s_parse_asm_stmt(par);
    }
	if (tok == TOK_IDENT) {
		const u8 *name = lex->lex_token;
		size_t    len  = lex->lex_cursor - lex->lex_token;
		dpp_parser_consume(par);
		if (dpp_parser_peek(par) == TOK_COLON) {
			dpp_parser_consume(par);
			struct dpp_node *node         = dpp_node_new(&par->par_arena, NOD_LABEL_STMT, line, col);
			node->nod_data.nod_id.id_name = name;
			node->nod_data.nod_id.id_len  = len;
			node->nod_child               = s_parse_statement(par);
			return node;
		}
		struct dpp_node *id_node         = dpp_node_new(&par->par_arena, NOD_IDENTIFIER, line, col);
		id_node->nod_data.nod_id.id_name = name;
		id_node->nod_data.nod_id.id_len  = len;
		struct dpp_node *node            = dpp_node_new(&par->par_arena, NOD_EXPR_STMT, line, col);
		struct dpp_node *lhs             = id_node;
		for (;;) {
			s32 t = dpp_parser_peek(par);
			if (t == TOK_DOT || t == TOK_ARROW) {
				dpp_parser_consume(par);
				struct dpp_node *n =
					dpp_node_new(&par->par_arena, NOD_BINARY_EXPR, lhs->nod_line, lhs->nod_column);
				n->nod_data.nod_op.op_kind = t;
				n->nod_data.nod_op.op_lhs  = lhs;
				struct dpp_node *member =
					dpp_node_new(&par->par_arena, NOD_IDENTIFIER, par->par_curr_lex->lex_line,
				                     par->par_curr_lex->lex_column);
				member->nod_data.nod_id.id_name = par->par_curr_lex->lex_token;
				member->nod_data.nod_id.id_len =
					par->par_curr_lex->lex_cursor - par->par_curr_lex->lex_token;
				dpp_parser_consume(par);
				n->nod_data.nod_op.op_rhs = member;
				lhs                       = n;
				continue;
			}
			if (t == TOK_LPAREN) {
				dpp_parser_consume(par);
				struct dpp_node *n =
					dpp_node_new(&par->par_arena, NOD_BINARY_EXPR, lhs->nod_line, lhs->nod_column);
				n->nod_data.nod_op.op_kind = OP_CALL;
				n->nod_data.nod_op.op_lhs  = lhs;
				struct dpp_node **arg_last = &n->nod_data.nod_op.op_rhs;
				while (dpp_parser_peek(par) != TOK_EOF && dpp_parser_peek(par) != ')') {
					struct dpp_node *arg = dpp_parser_parse_expr(par, 0);
					if (arg) {
						*arg_last = arg;
						arg_last  = &arg->nod_next;
					}
					if (dpp_parser_peek(par) == TOK_COMMA)
						dpp_parser_consume(par);
					else
						break;
				}
				dpp_parser_expect(par, ')');
				lhs = n;
				continue;
			}
			if (t == TOK_LBRACKET) {
				dpp_parser_consume(par);
				struct dpp_node *n =
					dpp_node_new(&par->par_arena, NOD_INDEX_EXPR, lhs->nod_line, lhs->nod_column);
				n->nod_data.nod_op.op_lhs = lhs;
				n->nod_data.nod_op.op_rhs = dpp_parser_parse_expr(par, 0);
				dpp_parser_expect(par, TOK_RBRACKET);
				lhs = n;
				continue;
			}
			if (t == TOK_INC || t == TOK_DEC) {
			        dpp_parser_consume(par);
			        struct dpp_node *n =
			                dpp_node_new(&par->par_arena, NOD_UNARY_EXPR, lhs->nod_line, lhs->nod_column);
			        n->nod_data.nod_op.op_kind = dpp_token_to_op(t);
			        n->nod_is_postfix          = true;
			        n->nod_child               = lhs;
			        lhs                        = n;
			        continue;
			}

			int prec = s_get_prec(t);
			if (prec < 0) break;
			dpp_parser_consume(par);

			struct dpp_node *rhs = dpp_parser_parse_expr(par, (t == TOK_ASSIGN) ? prec : prec + 1);
			struct dpp_node *new_lhs =
				dpp_node_new(&par->par_arena, NOD_BINARY_EXPR, lhs->nod_line, lhs->nod_column);
			new_lhs->nod_data.nod_op.op_kind = t;
			new_lhs->nod_data.nod_op.op_lhs  = lhs;
			new_lhs->nod_data.nod_op.op_rhs  = rhs;
			lhs                              = new_lhs;
		}
		node->nod_child = lhs;
		if (!dpp_parser_expect(par, TOK_SEMICOLON)) return NULL;
		return node;
	}
	if (tok == TOK_LBRACE) return s_parse_compound_statement(par);
	if (tok == TOK_RETURN) {
		dpp_parser_consume(par);
		struct dpp_node *node = dpp_node_new(&par->par_arena, NOD_RETURN_STMT, line, col);
		if (dpp_parser_peek(par) != TOK_SEMICOLON) node->nod_child = dpp_parser_parse_expr(par, 0);
		if (!dpp_parser_expect(par, TOK_SEMICOLON)) return NULL;
		return node;
	}
	if (tok == TOK_GOTO) {
		dpp_parser_consume(par);
		struct dpp_node *node = dpp_node_new(&par->par_arena, NOD_GOTO_STMT, line, col);
		if (dpp_parser_peek(par) != TOK_IDENT) {
			dpp_diag_error(node, "expected identifier after 'goto'");
		}
		node->nod_data.nod_id.id_name = lex->lex_token;
		node->nod_data.nod_id.id_len  = lex->lex_cursor - lex->lex_token;
		dpp_parser_consume(par);
		if (!dpp_parser_expect(par, TOK_SEMICOLON)) return NULL;
		return node;
	}
	if (tok == TOK_IF) {
		dpp_parser_consume(par);
		dpp_parser_expect(par, TOK_LPAREN);
		struct dpp_node *cond = dpp_parser_parse_expr(par, 0);
		dpp_parser_expect(par, TOK_RPAREN);
		struct dpp_node *then_stmt = s_parse_statement(par);
		struct dpp_node *node      = dpp_node_new(&par->par_arena, NOD_IF_STMT, line, col);
		node->nod_child            = cond;
		cond->nod_next             = then_stmt;
		if (dpp_parser_peek(par) == TOK_ELSE) {
			dpp_parser_consume(par);
			then_stmt->nod_next = s_parse_statement(par);
		}
		return node;
	}
	if (tok == TOK_DO) return s_parse_do_while_stmt(par);
	if (tok == TOK_WHILE) {
		dpp_parser_consume(par);
		dpp_parser_expect(par, TOK_LPAREN);
		struct dpp_node *cond = dpp_parser_parse_expr(par, 0);
		dpp_parser_expect(par, TOK_RPAREN);
		struct dpp_node *body = s_parse_statement(par);
		struct dpp_node *node = dpp_node_new(&par->par_arena, NOD_WHILE_STMT, line, col);
		node->nod_child       = cond;
		cond->nod_next        = body;
		return node;
	}
	if (tok == TOK_FOR) {
		dpp_parser_consume(par);
		dpp_parser_expect(par, TOK_LPAREN);
		struct dpp_node *node = dpp_node_new(&par->par_arena, NOD_FOR_STMT, line, col);
		if (s_is_type(par))
			node->nod_child = s_parse_declaration(par, NULL);
		else {
			if (dpp_parser_peek(par) != TOK_SEMICOLON) node->nod_child = dpp_parser_parse_expr(par, 0);
			if (!dpp_parser_expect(par, TOK_SEMICOLON)) return NULL;
		}
		struct dpp_node *cond = NULL;
		if (dpp_parser_peek(par) != TOK_SEMICOLON) cond = dpp_parser_parse_expr(par, 0);
		if (!dpp_parser_expect(par, TOK_SEMICOLON)) return NULL;
		struct dpp_node *inc = NULL;
		if (dpp_parser_peek(par) != ')') inc = dpp_parser_parse_expr(par, 0);
		dpp_parser_expect(par, ')');
		struct dpp_node *body = s_parse_statement(par);
		struct dpp_node **chain_last = &node->nod_child;
		if (node->nod_child) chain_last = &node->nod_child->nod_next;
		if (cond) {
		    *chain_last = cond;
		    chain_last = &cond->nod_next;
		}
		if (inc) {
		    *chain_last = inc;
		    chain_last = &inc->nod_next;
		}
		if (body) {
		    *chain_last = body;
		}
		return node;

	}
	if (tok == TOK_SWITCH) {
		dpp_parser_consume(par);
		dpp_parser_expect(par, TOK_LPAREN);
		struct dpp_node *cond = dpp_parser_parse_expr(par, 0);
		dpp_parser_expect(par, TOK_RPAREN);
		struct dpp_node *body = s_parse_statement(par);
		struct dpp_node *node = dpp_node_new(&par->par_arena, NOD_SWITCH_STMT, line, col);
		node->nod_child       = cond;
		cond->nod_next        = body;
		return node;
	}
	if (tok == TOK_CASE) {
		dpp_parser_consume(par);
		struct dpp_node *val = dpp_parser_parse_expr(par, 0);
		dpp_parser_expect(par, TOK_COLON);
		struct dpp_node *node = dpp_node_new(&par->par_arena, NOD_CASE_STMT, line, col);
		node->nod_child       = val;
		return node;
	}
	if (tok == TOK_DEFAULT) {
		dpp_parser_consume(par);
		dpp_parser_expect(par, TOK_COLON);
		return dpp_node_new(&par->par_arena, NOD_DEFAULT_STMT, line, col);
	}
	if (tok == TOK_ASM) return s_parse_asm_stmt(par);
	if (tok == TOK_BREAK) {
		dpp_parser_consume(par);
		if (!dpp_parser_expect(par, TOK_SEMICOLON)) return NULL;
		return dpp_node_new(&par->par_arena, NOD_BREAK_STMT, line, col);
	}
	if (tok == TOK_CONTINUE) {
		dpp_parser_consume(par);
		if (!dpp_parser_expect(par, TOK_SEMICOLON)) return NULL;
		return dpp_node_new(&par->par_arena, NOD_CONTINUE_STMT, line, col);
	}
	if (tok == TOK_SEMICOLON) {
		dpp_parser_consume(par);
		return dpp_node_new(&par->par_arena, NOD_INVALID, line, col);
	}
	struct dpp_node *node = dpp_node_new(&par->par_arena, NOD_EXPR_STMT, line, col);
	node->nod_child       = dpp_parser_parse_expr(par, 0);
	if (!dpp_parser_expect(par, TOK_SEMICOLON)) return NULL;
	return node;
}

static struct dpp_node *s_parse_compound_statement(struct dpp_parser *par)
{
	struct dpp_lexer *lex  = par->par_curr_lex;
	u32               line = lex->lex_line;
	u32               col  = lex->lex_column;
	dpp_parser_expect(par, TOK_LBRACE);
	struct dpp_node  *node = dpp_node_new(&par->par_arena, NOD_COMPOUND_STMT, line, col);
	struct dpp_node **last = &node->nod_child;
	while (dpp_parser_peek(par) != TOK_EOF && dpp_parser_peek(par) != TOK_RBRACE) {
		struct dpp_node *item = NULL;
		if (dpp_parser_peek(par) == TOK_ATTR_OPEN) {
			struct dpp_node *dummy = dpp_node_new(&par->par_arena, NOD_INVALID, 0, 0);
			s_parse_c23_attribute(par, dummy);
			item = s_parse_declaration(par, dummy);
		} else if (s_is_type(par))
			item = s_parse_declaration(par, NULL);
		else
			item = s_parse_statement(par);
		if (item) {
			*last = item;
			while (*last) last = &(*last)->nod_next;
		} else {
			dpp_parser_consume(par);
		}
	}
	dpp_parser_expect(par, TOK_RBRACE);
	return node;
}

static void s_parse_gcc_attribute(struct dpp_parser *par, struct dpp_node *node)
{
	dpp_parser_expect(par, TOK_ATTRIBUTE);
	dpp_parser_expect(par, '(');
	dpp_parser_expect(par, '(');
	while (dpp_parser_peek(par) != TOK_EOF && dpp_parser_peek(par) != ')') {
		s32 tok = dpp_parser_peek(par);
		if (tok == TOK_COMMA) {
			dpp_parser_consume(par);
			continue;
		}

		struct dpp_lexer *lex  = par->par_curr_lex;
		const u8         *attr = lex->lex_token;
		size_t            len  = lex->lex_cursor - lex->lex_token;

		dpp_parser_consume(par);

		if (tok == TOK_IDENT) {
            struct dpp_node *attr_node = dpp_node_new(&par->par_arena, NOD_ATTRIBUTE, lex->lex_line, lex->lex_column);
            attr_node->nod_data.nod_attr.attr_name = attr;
            attr_node->nod_data.nod_attr.attr_len = len;
            attr_node->nod_data.nod_attr.attr_args = NULL;
            
            // Add to node
            if (!node->nod_attrs) node->nod_attrs = attr_node;
            else {
                struct dpp_node *curr = node->nod_attrs;
                while (curr->nod_next) curr = curr->nod_next;
                curr->nod_next = attr_node;
            }
		}

		if (dpp_parser_peek(par) == '(') {
			dpp_parser_consume(par);
			int depth = 1;
			while (depth > 0 && dpp_parser_peek(par) != TOK_EOF) {
				s32 t = dpp_parser_consume(par);
				if (t == '(')
					depth++;
				else if (t == ')')
					depth--;
			}
		}
	}
	dpp_parser_expect(par, ')');
	dpp_parser_expect(par, ')');
}
static void s_parse_c23_attribute(struct dpp_parser *par, struct dpp_node *decl_node)
{
	dpp_parser_expect(par, TOK_ATTR_OPEN);
	while (dpp_parser_peek(par) != TOK_EOF && dpp_parser_peek(par) != TOK_ATTR_CLOSE) {
		if (dpp_parser_peek(par) == TOK_IDENT) {
			struct dpp_lexer *lex  = par->par_curr_lex;
			const u8         *name = lex->lex_token;
			size_t            len  = lex->lex_cursor - lex->lex_token;
            dpp_parser_consume(par);
            
            struct dpp_node *attr = dpp_node_new(&par->par_arena, NOD_ATTRIBUTE, lex->lex_line, lex->lex_column);
            attr->nod_data.nod_attr.attr_name = name;
            attr->nod_data.nod_attr.attr_len = len;
            attr->nod_data.nod_attr.attr_args = NULL;
            
            // Parse arguments if present
            if (dpp_parser_peek(par) == '(') {
                dpp_parser_consume(par);
                while (dpp_parser_peek(par) != TOK_EOF && dpp_parser_peek(par) != ')') {
                    dpp_parser_consume(par);
                }
                dpp_parser_expect(par, ')');
            }
            
            // Add to decl_node's attribute list
            if (!decl_node->nod_attrs) decl_node->nod_attrs = attr;
            else {
                struct dpp_node *curr = decl_node->nod_attrs;
                while (curr->nod_next) curr = curr->nod_next;
                curr->nod_next = attr;
            }
		} else {
            // Unexpected token in attribute list, skip
            dpp_parser_consume(par);
        }
    }
	dpp_parser_expect(par, TOK_ATTR_CLOSE);
}

static struct dpp_node *s_parse_asm_stmt(struct dpp_parser *par)
{
	struct dpp_lexer *lex  = par->par_curr_lex;
	u32               line = lex->lex_line;
	u32               col  = lex->lex_column;
	dpp_parser_expect(par, TOK_LPAREN);
	struct dpp_node  *node = dpp_node_new(&par->par_arena, NOD_ASM_STMT, line, col);
    
    // Template string
	if (DPP_IS_STRING_LITERAL(dpp_parser_peek(par))) {
		node->nod_child = dpp_node_new(&par->par_arena, NOD_STRING_LITERAL, line, col);
        node->nod_child->nod_data.nod_id.id_name = par->par_curr_lex->lex_token;
        node->nod_child->nod_data.nod_id.id_len = par->par_curr_lex->lex_cursor - par->par_curr_lex->lex_token;
		dpp_parser_consume(par);
	}

	node->nod_data.nod_asm.asm_outputs = NULL;
	node->nod_data.nod_asm.asm_inputs = NULL;
	node->nod_data.nod_asm.asm_clobbers = NULL;
	struct dpp_node **last_outputs = &node->nod_data.nod_asm.asm_outputs;
	struct dpp_node **last_inputs = &node->nod_data.nod_asm.asm_inputs;
	struct dpp_node **last_clobbers = &node->nod_data.nod_asm.asm_clobbers;
    
	int section = 0; // 0: template, 1: outputs, 2: inputs, 3: clobbers
	while (dpp_parser_peek(par) != TOK_RPAREN && dpp_parser_peek(par) != TOK_EOF) {
        if (dpp_parser_peek(par) == TOK_COLON) {
            dpp_parser_consume(par);
            section++;
            continue;
        }

        if (section == 1 || section == 2) {
            // Expect constraint(expr)
            if (DPP_IS_STRING_LITERAL(dpp_parser_peek(par))) {
                struct dpp_node *constraint = dpp_node_new(&par->par_arena, NOD_STRING_LITERAL, line, col);
                constraint->nod_data.nod_id.id_name = par->par_curr_lex->lex_token;
                constraint->nod_data.nod_id.id_len = par->par_curr_lex->lex_cursor - par->par_curr_lex->lex_token;
                dpp_parser_consume(par);

                dpp_parser_expect(par, TOK_LPAREN);
                struct dpp_node *expr = dpp_parser_parse_expr(par, 0);
                if (expr->nod_kind == NOD_IDENTIFIER) {
                    struct dpp_symbol *sym = dpp_symtab_lookup(&par->par_symtab, expr->nod_data.nod_id.id_name, expr->nod_data.nod_id.id_len);
                    if (sym) expr->nod_ref = sym->sym_node;
                }
                dpp_parser_expect(par, TOK_RPAREN);

                constraint->nod_next = expr;
                if (section == 1) {
                    *last_outputs = constraint;
                    last_outputs = &expr->nod_next;
                } else {
                    *last_inputs = constraint;
                    last_inputs = &expr->nod_next;
                }
            }
        } else if (section == 3) {
            // Expect clobber string
            if (DPP_IS_STRING_LITERAL(dpp_parser_peek(par))) {
                struct dpp_node *clobber = dpp_node_new(&par->par_arena, NOD_STRING_LITERAL, line, col);
                clobber->nod_data.nod_id.id_name = par->par_curr_lex->lex_token;
                clobber->nod_data.nod_id.id_len = par->par_curr_lex->lex_cursor - par->par_curr_lex->lex_token;
                dpp_parser_consume(par);
                
                *last_clobbers = clobber;
                last_clobbers = &clobber->nod_next;
            }
        }
        
        if (dpp_parser_peek(par) == TOK_COMMA) {
            dpp_parser_consume(par);
        } else if (dpp_parser_peek(par) != TOK_COLON && dpp_parser_peek(par) != TOK_RPAREN) {
            dpp_parser_consume(par);
        }
	}
	dpp_parser_expect(par, TOK_RPAREN);
	dpp_parser_expect(par, TOK_SEMICOLON);
	return node;
}

static struct dpp_node *s_parse_struct_union_specifier(struct dpp_parser *par)
{
	dpp_parser_peek(par);
	struct dpp_lexer *lex  = par->par_curr_lex;
	u32               line = lex->lex_line;
	u32               col  = lex->lex_column;

	s32              kind = dpp_parser_consume(par);
	struct dpp_node *node =
		dpp_node_new(&par->par_arena, (kind == TOK_STRUCT) ? NOD_STRUCT_DECL : NOD_UNION_DECL, line, col);
	if (dpp_parser_peek(par) == TOK_IDENT) {
		node->nod_data.nod_id.id_name = lex->lex_token;
		node->nod_data.nod_id.id_len  = lex->lex_cursor - lex->lex_token;
		struct dpp_symbol *sym        = dpp_symtab_lookup(&par->par_symtab, node->nod_data.nod_id.id_name,
		                                                  node->nod_data.nod_id.id_len);
		if (sym && (sym->sym_node->nod_kind == NOD_STRUCT_DECL || sym->sym_node->nod_kind == NOD_UNION_DECL))
			node = sym->sym_node;
		else
			dpp_symtab_insert(&par->par_symtab, node->nod_data.nod_id.id_name, node->nod_data.nod_id.id_len,
			                  node);
		dpp_parser_consume(par);
	}
	if (dpp_parser_peek(par) == TOK_LBRACE) {
		dpp_parser_consume(par);
		struct dpp_node **last_field = &node->nod_child;
		while (dpp_parser_peek(par) != TOK_EOF && dpp_parser_peek(par) != TOK_RBRACE) {
			struct dpp_node *field = s_parse_declaration(par, NULL);
			if (field) {
				*last_field = field;
				while (*last_field) last_field = &(*last_field)->nod_next;
			} else {
				dpp_parser_consume(par);
			}
		}
		dpp_parser_expect(par, TOK_RBRACE);
	}
	return node;
}

static struct dpp_type *s_base_type_from_tokens(struct dpp_parser *par, s32 last_type, struct dpp_node *st_node)
{
	struct dpp_type *ty = NULL;
	switch (last_type) {
	case TOK_VOID:
		ty = dpp_type_new(&par->par_arena, TYPE_VOID);
		break;
	case TOK_CHAR:
		ty = dpp_type_new(&par->par_arena, TYPE_CHAR);
		break;
	case TOK_INT:
		ty = dpp_type_new(&par->par_arena, TYPE_INT);
		break;
	case TOK_LONG:
		ty = dpp_type_new(&par->par_arena, TYPE_LONG);
		break;
	case TOK_FLOAT:
		ty = dpp_type_new(&par->par_arena, TYPE_FLOAT);
		break;
	case TOK_DOUBLE:
		ty = dpp_type_new(&par->par_arena, TYPE_DOUBLE);
		break;
	case TOK_BOOL:
		ty = dpp_type_new(&par->par_arena, TYPE_BOOL);
		break;
	case TOK_STRUCT:
	case TOK_UNION:
		ty                               = dpp_type_new(&par->par_arena, (last_type == TOK_STRUCT) ? TYPE_STRUCT : TYPE_UNION);
		ty->ty_data.ty_struct.struct_node = st_node;
		break;
	case TOK_ENUM:
		ty = dpp_type_new(&par->par_arena, TYPE_INT);
		break;
	case TOK_BUILTIN_VA_LIST:
		ty = dpp_type_ptr(&par->par_arena, dpp_type_new(&par->par_arena, TYPE_VOID));
		break;
	case TOK_TYPEDEF:
		ty = st_node ? (struct dpp_type *)st_node->nod_type : dpp_type_new(&par->par_arena, TYPE_INT);
		break;
	default:
		ty = dpp_type_new(&par->par_arena, TYPE_INT);
		break;
	}
	return ty;
}

static struct dpp_node *s_parse_declaration(struct dpp_parser *par, struct dpp_node *prefix_attrs)
{
	s32 start_tok = dpp_parser_peek(par);
	if (!s_is_specifier(start_tok) && start_tok != TOK_IDENT) return NULL;
	struct dpp_lexer *lex       = par->par_curr_lex;
	u32               line      = lex->lex_line;
	u32               col       = lex->lex_column;
	struct dpp_node  *spec_attr = dpp_node_new(&par->par_arena, NOD_INVALID, line, col);
	if (prefix_attrs) {
		spec_attr->nod_attr_flags = prefix_attrs->nod_attr_flags;
		spec_attr->nod_attrs      = prefix_attrs->nod_attrs;
	}
	struct dpp_node *st_node   = NULL;
	s32              last_type = 0;
	u32              storage   = 0;
		while (s_is_type(par)) {
		lex     = par->par_curr_lex;
		s32 tok = dpp_parser_peek(par);
		if (tok == TOK_IDENT) {
			size_t len = lex->lex_cursor - lex->lex_token;
			if (len == 13 && memcmp(lex->lex_token, "__extension__", 13) == 0) {
				dpp_parser_consume(par);
				continue;
			}
			if (len == 8 && memcmp(lex->lex_token, "__inline", 8) == 0) {
				dpp_parser_consume(par);
				continue;
			}
			if (len == 10 && memcmp(lex->lex_token, "__inline__", 10) == 0) {
				dpp_parser_consume(par);
				continue;
			}
			if (len == 10 && memcmp(lex->lex_token, "__restrict", 10) == 0) {
				dpp_parser_consume(par);
				continue;
			}
			if (len == 9 && memcmp(lex->lex_token, "FY_EXPORT", 9) == 0) {
				dpp_parser_consume(par);
				continue;
			}
			struct dpp_symbol *sym = dpp_symtab_lookup(&par->par_symtab, lex->lex_token, len);
			if (sym && (sym->sym_node->nod_type_flags & NOD_TYPE_TYPEDEF)) {
				last_type = TOK_TYPEDEF;
				st_node   = sym->sym_node;
				dpp_parser_consume(par);
			} else {
				break;
			}
			continue;
		}
		if (tok == TOK_ATTRIBUTE)
			s_parse_gcc_attribute(par, spec_attr);
		else if (tok == TOK_ATTR_OPEN)
			s_parse_c23_attribute(par, spec_attr);
		else if (tok == TOK_STRUCT || tok == TOK_UNION) {
			st_node = s_parse_struct_union_specifier(par);
			st_node->nod_attr_flags |= spec_attr->nod_attr_flags;
			last_type = tok;
		} else if (tok == TOK_ENUM) {
			st_node = s_parse_enum_specifier(par);
			last_type = tok;
		}
		else {
			s32 tok_type = dpp_parser_consume(par);
			if (tok_type == TOK_UNSIGNED)
				spec_attr->nod_type_flags |= NOD_TYPE_UNSIGNED;
			else if (tok_type == TOK_SIGNED)
				spec_attr->nod_type_flags |= NOD_TYPE_SIGNED;
			else if (tok_type == TOK_CONST)
				spec_attr->nod_type_flags |= NOD_TYPE_CONST;
			else if (tok_type == TOK_VOLATILE)
				spec_attr->nod_type_flags |= NOD_TYPE_VOLATILE;
			else if (tok_type == TOK_RESTRICT)
				spec_attr->nod_type_flags |= NOD_TYPE_RESTRICT;
			else if (tok_type == TOK_STATIC)
				storage |= NOD_STORAGE_STATIC;
			else if (tok_type == TOK_EXTERN)
				storage |= NOD_STORAGE_EXTERN;
			else if (tok_type == TOK_LONG)
				spec_attr->nod_type_flags |= NOD_TYPE_LONG;
			else if (tok_type == TOK_SHORT)
				spec_attr->nod_type_flags |= NOD_TYPE_SHORT;
			else if (tok_type == TOK_TYPEDEF)
				spec_attr->nod_type_flags |= NOD_TYPE_TYPEDEF;
			else if (tok_type == TOK_TYPEOF) {
				dpp_parser_expect(par, TOK_LPAREN);
				struct dpp_node *typeof_node =
					dpp_node_new(&par->par_arena, NOD_TYPEOF, lex->lex_line, lex->lex_column);
				typeof_node->nod_child = dpp_parser_parse_expr(par, 0);
				dpp_parser_expect(par, TOK_RPAREN);
				spec_attr->nod_child = typeof_node;
				last_type            = TOK_TYPEOF;
			} else
				last_type = tok_type;
		}
	}
	if (dpp_parser_peek(par) == TOK_SEMICOLON) {
		dpp_parser_consume(par);
		if (st_node && st_node->nod_next != NULL) {
			return st_node;
		}
		return st_node;
	}
	struct dpp_node *head = NULL;
	struct dpp_node *tail = NULL;
	
	struct dpp_type *base_type = s_base_type_from_tokens(par, last_type, st_node);

	while (dpp_parser_peek(par) != TOK_EOF) {
		lex                 = par->par_curr_lex;
		u32 d_line          = lex->lex_line;
		u32 d_col           = lex->lex_column;
		
		struct declarator_result res = parse_declarator_full(par, base_type);
		
		struct dpp_node *decl = dpp_node_new(
			&par->par_arena, (spec_attr->nod_type_flags & NOD_TYPE_TYPEDEF) ? NOD_TYPEDEF : NOD_VAR_DECL,
			d_line, d_col);
		decl->nod_attr_flags          = spec_attr->nod_attr_flags;
		decl->nod_attrs               = spec_attr->nod_attrs;
		decl->nod_type_flags          = spec_attr->nod_type_flags;
		decl->nod_storage             = storage;
		decl->nod_data.nod_id.id_name = res.name;
		decl->nod_data.nod_id.id_len  = res.name_len;
		decl->nod_type                = res.type;

		if (st_node) {
			if (st_node->nod_kind == NOD_STRUCT_DECL)
				decl->nod_data.nod_id.id_type = dpp_token_to_type(TOK_STRUCT);
			else if (st_node->nod_kind == NOD_UNION_DECL)
				decl->nod_data.nod_id.id_type = dpp_token_to_type(TOK_UNION);
			else if (st_node->nod_kind == NOD_ENUM_DECL)
				decl->nod_data.nod_id.id_type = dpp_token_to_type(TOK_ENUM);
			else if (st_node->nod_kind == NOD_TYPEDEF)
				decl->nod_data.nod_id.id_type = dpp_token_to_type(TOK_TYPEDEF);
			decl->nod_type_node = st_node;
		} else
			decl->nod_data.nod_id.id_type = last_type;

		if (res.type && res.type->ty_kind == TYPE_FUNCTION) {
			decl->nod_kind        = NOD_FUNCTION_DECL;
			decl->nod_is_variadic = res.type->ty_data.ty_function.is_variadic;
			decl->nod_child       = res.type->ty_data.ty_function.param_nodes;
		}

		if (decl->nod_data.nod_id.id_name) {
			dpp_symtab_insert(&par->par_symtab, decl->nod_data.nod_id.id_name, decl->nod_data.nod_id.id_len, decl);
		}

		if (decl->nod_kind == NOD_FUNCTION_DECL) {
			while (dpp_parser_peek(par) == TOK_ATTRIBUTE)
				s_parse_gcc_attribute(par, spec_attr);
			while (dpp_parser_peek(par) == TOK_ASM) {
				dpp_parser_consume(par);
				if (dpp_parser_peek(par) == TOK_VOLATILE) dpp_parser_consume(par);
				if (dpp_parser_peek(par) == TOK_LPAREN) {
					dpp_parser_consume(par);
					while (DPP_IS_STRING_LITERAL(dpp_parser_peek(par))) dpp_parser_consume(par);
					dpp_parser_expect(par, ')');
				}
			}
			if (dpp_parser_peek(par) == TOK_LBRACE) {
				dpp_symtab_push(&par->par_symtab);
				
				/* Insert parameters into function scope */
				struct dpp_node *p = decl->nod_child;
				while (p) {
					if (p->nod_data.nod_id.id_name) {
						dpp_symtab_insert(&par->par_symtab, p->nod_data.nod_id.id_name, p->nod_data.nod_id.id_len, p);
					}
					p = p->nod_next;
				}

				struct dpp_node  *body       = s_parse_compound_statement(par);
				struct dpp_node **last_child = &decl->nod_child;
				while (*last_child) last_child = &(*last_child)->nod_next;
				*last_child = body;
				dpp_symtab_pop(&par->par_symtab);
				if (!head)
					head = decl;
				else
					tail->nod_next = decl;
				return head;
			}
		} else if (dpp_parser_peek(par) == TOK_ASSIGN) {
			dpp_parser_consume(par);
			decl->nod_child = s_parse_initializer(par);
		}
		
		if (!head)
			head = decl;
		else
			tail->nod_next = decl;
		tail = decl;
		
		while (dpp_parser_peek(par) == TOK_ATTRIBUTE || dpp_parser_peek(par) == TOK_ASM) {
			if (dpp_parser_peek(par) == TOK_ATTRIBUTE)
				s_parse_gcc_attribute(par, spec_attr);
			else {
				dpp_parser_consume(par);
				if (dpp_parser_peek(par) == TOK_VOLATILE) dpp_parser_consume(par);
				if (dpp_parser_peek(par) == TOK_LPAREN) {
					dpp_parser_consume(par);
					while (DPP_IS_STRING_LITERAL(dpp_parser_peek(par))) dpp_parser_consume(par);
					dpp_parser_expect(par, ')');
				}
			}
		}
		if (dpp_parser_peek(par) == TOK_COMMA)
			dpp_parser_consume(par);
		else
			break;
	}
	while (dpp_parser_peek(par) == TOK_ATTRIBUTE || dpp_parser_peek(par) == TOK_ASM) {
		if (dpp_parser_peek(par) == TOK_ATTRIBUTE)
			s_parse_gcc_attribute(par, spec_attr);
		else {
			dpp_parser_consume(par);
			if (dpp_parser_peek(par) == TOK_VOLATILE) dpp_parser_consume(par);
			if (dpp_parser_peek(par) == TOK_LPAREN) {
				dpp_parser_consume(par);
				while (dpp_parser_peek(par) == TOK_STRING) dpp_parser_consume(par);
				dpp_parser_expect(par, ')');
			}
		}
	}
	if (dpp_parser_peek(par) == TOK_SEMICOLON) dpp_parser_consume(par);
	return head;
}

static struct dpp_node *s_parse_parameter_list(struct dpp_parser *par, bool *out_is_variadic)
{
	struct dpp_lexer *lex  = par->par_curr_lex;
	struct dpp_node  *head = NULL;
	struct dpp_node **last = &head;
	while (dpp_parser_peek(par) != TOK_EOF && dpp_parser_peek(par) != ')') {
		lex      = par->par_curr_lex;
		u32 line = lex->lex_line;
		u32 col  = lex->lex_column;
		if (dpp_parser_peek(par) == TOK_ELLIPSIS) {
			dpp_parser_consume(par);
			*out_is_variadic = true;
			break;
		}
		if (dpp_parser_peek(par) == TOK_ATTRIBUTE) {
			struct dpp_node *dummy = dpp_node_new(&par->par_arena, NOD_INVALID, 0, 0);
			s_parse_gcc_attribute(par, dummy);
			continue;
		}

		/* Allow attributes to appear before types */
		while (dpp_parser_peek(par) == TOK_ATTRIBUTE) {
			struct dpp_node *dummy = dpp_node_new(&par->par_arena, NOD_INVALID, 0, 0);
			s_parse_gcc_attribute(par, dummy);
		}

		/* If we don't recognize a type, consume the token anyway and keep parsing the parameter */
		if (!s_is_type(par)) {
		}
		struct dpp_node *param_attrs = dpp_node_new(&par->par_arena, NOD_INVALID, 0, 0);
		s32              base_type   = TOK_INT;
		struct dpp_node *st_node     = NULL;

		if (dpp_parser_peek(par) == TOK_STRUCT || dpp_parser_peek(par) == TOK_UNION) {
			st_node   = s_parse_struct_union_specifier(par);
			base_type = (st_node->nod_kind == NOD_STRUCT_DECL) ? TOK_STRUCT : TOK_UNION;
		} else if (dpp_parser_peek(par) == TOK_ENUM) {
			dpp_parser_consume(par);
			base_type = TOK_ENUM;
			if (dpp_parser_peek(par) == TOK_IDENT) dpp_parser_consume(par);
		} else if (s_is_type(par)) {
			while (s_is_type(par)) {
				lex   = par->par_curr_lex;
				s32 t = dpp_parser_peek(par);
				if (t == TOK_ATTR_OPEN) {
					s_parse_c23_attribute(par, param_attrs);
					continue;
				}
				if (t == TOK_STRUCT || t == TOK_UNION) {
					st_node   = s_parse_struct_union_specifier(par);
					base_type = (st_node->nod_kind == NOD_STRUCT_DECL) ? TOK_STRUCT : TOK_UNION;
					continue;
				}
				if (t == TOK_IDENT) {
					struct dpp_symbol *sym = dpp_symtab_lookup(&par->par_symtab, lex->lex_token,
					                                           lex->lex_cursor - lex->lex_token);
					if (sym && (sym->sym_node->nod_type_flags & NOD_TYPE_TYPEDEF)) {
						base_type = TOK_TYPEDEF;
						st_node   = sym->sym_node;
						dpp_parser_consume(par);
						continue;
					} else
						break;
				}
				s32 qt = dpp_parser_consume(par);
				if (qt == TOK_CONST || qt == TOK_VOLATILE || qt == TOK_RESTRICT) {
					param_attrs->nod_type_flags |= (qt == TOK_CONST)      ? NOD_TYPE_CONST
					                               : (qt == TOK_VOLATILE) ? NOD_TYPE_VOLATILE
					                                                      : NOD_TYPE_RESTRICT;
				} else {
					base_type = qt;
				}
			}
		} else {
			/* Consume generic identifier as part of parameter name if type not found */
			if (dpp_parser_peek(par) == TOK_IDENT) dpp_parser_consume(par);
		}
		u32 ptr_depth = 0;
		while (dpp_parser_peek(par) == TOK_STAR || dpp_parser_peek(par) == TOK_CONST || dpp_parser_peek(par) == TOK_VOLATILE ||
		       dpp_parser_peek(par) == TOK_RESTRICT || dpp_parser_peek(par) == TOK_ATTRIBUTE) {
			if (dpp_parser_peek(par) == TOK_STAR) {
				dpp_parser_consume(par);
				ptr_depth++;
			} else if (dpp_parser_peek(par) == TOK_ATTRIBUTE) {
				s_parse_gcc_attribute(par, param_attrs);
			} else {
				dpp_parser_consume(par);
			}
		}
		if (dpp_parser_peek(par) == TOK_ATTR_OPEN) s_parse_c23_attribute(par, param_attrs);
		struct dpp_node *param         = dpp_node_new(&par->par_arena, NOD_PARAM_DECL, line, col);
		param->nod_data.nod_id.id_type = base_type;
		param->nod_ptr_depth           = ptr_depth;
		param->nod_type_node           = st_node;
		param->nod_attr_flags          = param_attrs->nod_attr_flags;
		param->nod_attrs               = param_attrs->nod_attrs;
		if (dpp_parser_peek(par) == TOK_IDENT) {
			param->nod_data.nod_id.id_name = lex->lex_token;
			param->nod_data.nod_id.id_len  = lex->lex_cursor - lex->lex_token;
			dpp_symtab_insert(&par->par_symtab, param->nod_data.nod_id.id_name,
			                  param->nod_data.nod_id.id_len, param);
			dpp_parser_consume(par);
			if (dpp_parser_peek(par) == TOK_ATTR_OPEN) s_parse_c23_attribute(par, param);
		}
		*last = param;
		last  = &param->nod_next;
		if (dpp_parser_peek(par) == TOK_COMMA)
			dpp_parser_consume(par);
		else
			break;
	}
	return head;
}

static struct dpp_node *s_parse_enum_specifier(struct dpp_parser *par)
{
	struct dpp_lexer *lex  = par->par_curr_lex;
	u32               line = lex->lex_line;
	u32               col  = lex->lex_column;
	dpp_parser_expect(par, TOK_ENUM);
	struct dpp_node *node = dpp_node_new(&par->par_arena, NOD_ENUM_DECL, line, col);
	if (dpp_parser_peek(par) == TOK_IDENT) {
		node->nod_data.nod_id.id_name = lex->lex_token;
		node->nod_data.nod_id.id_len  = lex->lex_cursor - lex->lex_token;
		dpp_parser_consume(par);
	}
	if (dpp_parser_peek(par) == TOK_LBRACE) {
		dpp_parser_consume(par);
		struct dpp_node **last = &node->nod_child;
		while (dpp_parser_peek(par) != TOK_EOF && dpp_parser_peek(par) != TOK_RBRACE) {
			if (dpp_parser_peek(par) == TOK_IDENT) {
				struct dpp_node *constant =
					dpp_node_new(&par->par_arena, NOD_ENUM_CONST, lex->lex_line, lex->lex_column);
				constant->nod_data.nod_id.id_name = lex->lex_token;
				constant->nod_data.nod_id.id_len  = lex->lex_cursor - lex->lex_token;
				dpp_symtab_insert(&par->par_symtab, constant->nod_data.nod_id.id_name,
				                  constant->nod_data.nod_id.id_len, constant);
				dpp_parser_consume(par);
				if (dpp_parser_peek(par) == TOK_ASSIGN) {
					dpp_parser_consume(par);
					dpp_parser_parse_expr(par, 0);
				}
				*last = constant;
				last  = &constant->nod_next;
			}
			if (dpp_parser_peek(par) == TOK_COMMA)
				dpp_parser_consume(par);
			else if (dpp_parser_peek(par) == TOK_COLON || dpp_parser_peek(par) == TOK_RPAREN)
				break;
			}
			}
			dpp_parser_expect(par, TOK_RPAREN);
			dpp_parser_expect(par, TOK_SEMICOLON);
			return node;
			}

static bool s_is_type(struct dpp_parser *par)
{
	s32 tok = dpp_parser_peek(par);
	if (s_is_specifier(tok)) return true;
	if (tok == TOK_IDENT) {
		struct dpp_lexer *lex = par->par_curr_lex;
		size_t            len = lex->lex_cursor - lex->lex_token;
		if (len == 13 && memcmp(lex->lex_token, "__extension__", 13) == 0) return true;
		if (len == 8 && memcmp(lex->lex_token, "__inline", 8) == 0) return true;
		if (len == 10 && memcmp(lex->lex_token, "__inline__", 10) == 0) return true;
		if (len == 10 && memcmp(lex->lex_token, "__restrict", 10) == 0) return true;
		if (len == 9 && memcmp(lex->lex_token, "FY_EXPORT", 9) == 0) return true;
		struct dpp_symbol *sym        = dpp_symtab_lookup(&par->par_symtab, lex->lex_token, len);
		bool               is_typedef = (sym && (sym->sym_node->nod_type_flags & NOD_TYPE_TYPEDEF));
		if (!is_typedef) {
		}
		return is_typedef;
	}
	if (tok == TOK_ATTR_OPEN) return true;
	return false;
}


struct dpp_node *dpp_parser_parse(struct dpp_parser *par)
{
    struct dpp_node *root = dpp_node_new(&par->par_arena, NOD_TRANSLATION_UNIT, 0, 0);
    while (dpp_parser_peek(par) != TOK_EOF) {
        struct dpp_node *decl = s_parse_declaration(par, NULL);
        if (decl) {
            dpp_node_add_child(root, decl);
        } else {
            dpp_parser_consume(par);
        }
    }
    return root;
}
