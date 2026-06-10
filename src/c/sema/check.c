#include "c/sema/check.h"
#include "c/lexer/token.h"
#include "core/diagnostic/diag.h"
#include "c/sema/layout.h"
#include "core/sema/symbol.h"
#include "core/sema/type.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static struct dpp_node *g_current_func = NULL;
static u32              g_counter      = 0;

static struct dpp_type *s_resolve_type(struct dpp_node *node, struct dpp_symtab *tab, struct dpp_arena *arena)
{
	if (!node) return dpp_type_new(arena, TYPE_INT);
	if (node->nod_type) return (struct dpp_type *)node->nod_type;

	if (node->nod_type_node && node->nod_type_node->nod_kind != NOD_STRUCT_DECL &&
	    node->nod_type_node->nod_kind != NOD_UNION_DECL && node->nod_type_node->nod_kind != NOD_ENUM_DECL) {
		struct dpp_type *base = s_resolve_type(node->nod_type_node, tab, arena);
		for (u32 i = 0; i < node->nod_ptr_depth; i++) base = dpp_type_ptr(arena, base);
		return base;
	}

	s32              base_tok = node->nod_data.nod_id.id_type;
	struct dpp_type *ty;

	switch (base_tok) {
	case TOK_VOID:
		ty = dpp_type_new(arena, TYPE_VOID);
		break;
	case TOK_CHAR:
		ty = dpp_type_new(arena, TYPE_CHAR);
		break;
	case TOK_INT:
		ty = dpp_type_new(arena, TYPE_INT);
		break;
	case TOK_LONG:
		ty = dpp_type_new(arena, TYPE_LONG);
		break;
	case TOK_FLOAT:
		ty = dpp_type_new(arena, TYPE_FLOAT);
		break;
	case TYPE_DOUBLE:
		ty = dpp_type_new(arena, TYPE_DOUBLE);
		break;
	case TOK_BOOL:
		ty = dpp_type_new(arena, TYPE_BOOL);
		break;
	case TOK_TYPEDEF: {
		if (node->nod_type_node) return s_resolve_type(node->nod_type_node, tab, arena);
		ty = dpp_type_new(arena, TYPE_INT);
		break;
	}
	case TOK_STRUCT:
	case TOK_UNION: {
		ty = dpp_type_new(arena, (base_tok == TOK_STRUCT) ? TYPE_STRUCT : TYPE_UNION);
		if (node->nod_type_node)
			ty->ty_data.ty_struct.struct_node = node->nod_type_node;
		else {
			struct dpp_symbol *sym =
				dpp_symtab_lookup(tab, node->nod_data.nod_id.id_name,
			                          node->nod_data.nod_id.id_name ? node->nod_data.nod_id.id_len : 0);
			if (sym && sym->sym_node) ty->ty_data.ty_struct.struct_node = sym->sym_node;
		}
		break;
	}
	case TOK_ENUM:
		ty = dpp_type_new(arena, TYPE_INT);
		break;
	case TOK_BUILTIN_VA_LIST:
		ty = dpp_type_ptr(arena, dpp_type_new(arena, TYPE_VOID));
		break;
	default:
		ty = dpp_type_new(arena, TYPE_INT);
		break;
	}

	for (u32 i = 0; i < node->nod_ptr_depth; i++) ty = dpp_type_ptr(arena, ty);
	if (node->nod_array_size > 0) {
		ty = dpp_type_array(arena, ty, node->nod_array_size);
	}
	return ty;
}

static struct dpp_node *s_inject_lazy_builtin(struct dpp_symtab *tab, struct dpp_arena *arena, const u8 *name,
                                              size_t len)
{
	if ((len == 8 && memcmp(name, "va_start", 8) == 0) ||
	    (len == 18 && memcmp(name, "__builtin_va_start", 18) == 0) ||
	    (len == 6 && memcmp(name, "va_end", 6) == 0) || (len == 16 && memcmp(name, "__builtin_va_end", 16) == 0) ||
	    (len == 14 && memcmp(name, "__builtin_expect", 14) == 0)) {
		struct dpp_node *decl         = dpp_node_new(arena, NOD_FUNCTION_DECL, 0, 0);
		decl->nod_data.nod_id.id_name = (const u8 *)strdup((const char *)name);
		decl->nod_data.nod_id.id_len  = len;
		decl->nod_storage             = NOD_STORAGE_EXTERN;
		decl->nod_type                = dpp_type_new(arena, TYPE_VOID);
		decl->nod_is_variadic         = true;
		dpp_symtab_insert(tab, decl->nod_data.nod_id.id_name, decl->nod_data.nod_id.id_len, decl);
		return decl;
	}
	return NULL;
}

static struct dpp_node *s_resolve_tracking_macro(struct dpp_node *node, struct dpp_arena *arena)
{
	const u8 *name = node->nod_data.nod_id.id_name;
	size_t    len  = node->nod_data.nod_id.id_len;

	if (len == 8 && memcmp(name, "__LINE__", 8) == 0) {
		node->nod_kind                 = NOD_INT_LITERAL;
		node->nod_data.nod_val.val_int = node->nod_line;
		node->nod_type                 = dpp_type_new(arena, TYPE_INT);
		return node;
	}
	if (len == 8 && memcmp(name, "__FILE__", 8) == 0) {
		node->nod_kind                = NOD_STRING_LITERAL;
		node->nod_data.nod_id.id_name = (const u8 *)"<dynamic>";
		node->nod_data.nod_id.id_len  = 9;
		node->nod_type                = dpp_type_ptr(arena, dpp_type_new(arena, TYPE_CHAR));
		return node;
	}
	if (len == 13 && memcmp(name, "__FILE_NAME__", 13) == 0) {
		node->nod_kind                = NOD_STRING_LITERAL;
		node->nod_data.nod_id.id_name = (const u8 *)"<dynamic>";
		node->nod_data.nod_id.id_len  = 9;
		node->nod_type                = dpp_type_ptr(arena, dpp_type_new(arena, TYPE_CHAR));
		return node;
	}
	if (len == 8 && memcmp(name, "__func__", 8) == 0 || (len == 12 && memcmp(name, "__FUNCTION__", 12) == 0)) {
		node->nod_kind = NOD_STRING_LITERAL;
		if (g_current_func) {
			node->nod_data.nod_id.id_name = g_current_func->nod_data.nod_id.id_name;
			node->nod_data.nod_id.id_len  = g_current_func->nod_data.nod_id.id_len;
		} else {
			node->nod_data.nod_id.id_name = (const u8 *)"<global>";
			node->nod_data.nod_id.id_len  = 8;
		}
		node->nod_type = dpp_type_ptr(arena, dpp_type_new(arena, TYPE_CHAR));
		return node;
	}
	if (len == 11 && memcmp(name, "__COUNTER__", 11) == 0) {
		node->nod_kind                 = NOD_INT_LITERAL;
		node->nod_data.nod_val.val_int = g_counter++;
		node->nod_type                 = dpp_type_new(arena, TYPE_INT);
		return node;
	}
	/* Fixed macros */
	if (len == 8 && memcmp(name, "__DATE__", 8) == 0) {
		node->nod_kind                = NOD_STRING_LITERAL;
		node->nod_data.nod_id.id_name = (const u8 *)"Jun 02 2026";
		node->nod_data.nod_id.id_len  = 11;
		node->nod_type                = dpp_type_ptr(arena, dpp_type_new(arena, TYPE_CHAR));
		return node;
	}
	if (len == 8 && memcmp(name, "__TIME__", 8) == 0) {
		node->nod_kind                = NOD_STRING_LITERAL;
		node->nod_data.nod_id.id_name = (const u8 *)"18:30:00";
		node->nod_data.nod_id.id_len  = 8;
		node->nod_type                = dpp_type_ptr(arena, dpp_type_new(arena, TYPE_CHAR));
		return node;
	}
	if (len == 16 && memcmp(name, "__STDC_VERSION__", 16) == 0) {
		node->nod_kind                 = NOD_INT_LITERAL;
		node->nod_data.nod_val.val_int = 202311L;
		node->nod_type                 = dpp_type_new(arena, TYPE_LONG);
		return node;
	}
	if ((len == 7 && memcmp(name, "__clang__", 7) == 0) || (len == 8 && memcmp(name, "__GNUC__", 8) == 0) ||
	    (len == 7 && memcmp(name, "__dpp__", 7) == 0)) {
		node->nod_kind                 = NOD_INT_LITERAL;
		node->nod_data.nod_val.val_int = 1;
		node->nod_type                 = dpp_type_new(arena, TYPE_INT);
		return node;
	}

	return NULL;
}

static struct dpp_node *s_resolve_builtin_math_macro(struct dpp_node *node, struct dpp_arena *arena)
{
	struct dpp_node *lhs = node->nod_data.nod_op.op_lhs;
	if (!lhs || lhs->nod_kind != NOD_IDENTIFIER) return NULL;
	const u8 *name = lhs->nod_data.nod_id.id_name;
	size_t    len  = lhs->nod_data.nod_id.id_len;

	if (len == 7 && memcmp(name, "m_align", 7) == 0) {
		struct dpp_node *x = node->nod_data.nod_op.op_rhs;
		struct dpp_node *a = x ? x->nod_next : NULL;
		if (!x || !a) return NULL;
		struct dpp_node *plus         = dpp_node_new(arena, NOD_BINARY_EXPR, node->nod_line, node->nod_column);
		plus->nod_data.nod_op.op_kind = '+';
		plus->nod_data.nod_op.op_lhs  = x;
		plus->nod_data.nod_op.op_rhs  = a;
		struct dpp_node *one          = dpp_node_new(arena, NOD_INT_LITERAL, node->nod_line, node->nod_column);
		one->nod_data.nod_val.val_int = 1;
		one->nod_type                 = dpp_type_new(arena, TYPE_INT);
		struct dpp_node *sub          = dpp_node_new(arena, NOD_BINARY_EXPR, node->nod_line, node->nod_column);
		sub->nod_data.nod_op.op_kind  = '-';
		sub->nod_data.nod_op.op_lhs   = plus;
		sub->nod_data.nod_op.op_rhs   = one;
		struct dpp_node *sub2         = dpp_node_new(arena, NOD_BINARY_EXPR, node->nod_line, node->nod_column);
		sub2->nod_data.nod_op.op_kind = '-';
		sub2->nod_data.nod_op.op_lhs  = a;
		sub2->nod_data.nod_op.op_rhs  = one;
		struct dpp_node *not_node     = dpp_node_new(arena, NOD_UNARY_EXPR, node->nod_line, node->nod_column);
		not_node->nod_data.nod_op.op_kind = dpp_token_to_op(TOK_TILDE);
		not_node->nod_child               = sub2;
		node->nod_kind                    = NOD_BINARY_EXPR;
		node->nod_data.nod_op.op_kind     = dpp_token_to_op(TOK_AMP);
		node->nod_data.nod_op.op_lhs      = sub;
		node->nod_data.nod_op.op_rhs      = not_node;
		node->nod_type                    = dpp_type_new(arena, TYPE_LONG);
		return node;
	}
	return NULL;
}

void dpp_check_top_level(struct dpp_node *root)
{
	struct dpp_node *curr = root->nod_child;
	while (curr) {
		if (curr->nod_kind == NOD_FUNCTION_DECL) {
		}
		curr = curr->nod_next;
	}
}

void dpp_analyze_types(struct dpp_node *node, struct dpp_symtab *tab, struct dpp_arena *arena,
                       struct dpp_target *target)
{
	if (!node) return;
	switch (node->nod_kind) {
	case NOD_TRANSLATION_UNIT: {
		struct dpp_node *curr = node->nod_child;
		while (curr) {
			dpp_analyze_types(curr, tab, arena, target);
			curr = curr->nod_next;
		}
		break;
	}
	case NOD_FUNCTION_DECL: {
		struct dpp_node *old_func = g_current_func;
		g_current_func            = node;
		dpp_symtab_push(tab);
		node->nod_type     = s_resolve_type(node, tab, arena);
		struct dpp_node *p = node->nod_child;
		while (p && p->nod_kind == NOD_PARAM_DECL) {
			p->nod_type = s_resolve_type(p, tab, arena);
			dpp_symtab_insert(tab, p->nod_data.nod_id.id_name, p->nod_data.nod_id.id_len, p);
			p = p->nod_next;
		}
		if (p && p->nod_kind == NOD_COMPOUND_STMT) dpp_analyze_types(p, tab, arena, target);
		dpp_symtab_pop(tab);
		g_current_func = old_func;
		break;
	}
	case NOD_VAR_DECL: {
		node->nod_type = s_resolve_type(node, tab, arena);
		if (node->nod_child) dpp_analyze_types(node->nod_child, tab, arena, target);
		dpp_symtab_insert(tab, node->nod_data.nod_id.id_name, node->nod_data.nod_id.id_len, node);
		break;
	}
	case NOD_ENUM_DECL: {
		struct dpp_node *c = node->nod_child;
		while (c) {
			if (c->nod_kind == NOD_ENUM_CONST) {
				c->nod_type = dpp_type_new(arena, TYPE_INT);
				dpp_symtab_insert(tab, c->nod_data.nod_id.id_name, c->nod_data.nod_id.id_len, c);
			}
			c = c->nod_next;
		}
		break;
	}
	case NOD_STRUCT_DECL:
	case NOD_UNION_DECL: {
		struct dpp_type *st_ty = dpp_type_new(arena, (node->nod_kind == NOD_STRUCT_DECL) ? TYPE_STRUCT : TYPE_UNION);
		st_ty->ty_data.ty_struct.struct_node = node;
		node->nod_type = st_ty;
		dpp_symtab_insert(tab, node->nod_data.nod_id.id_name,
		                  node->nod_data.nod_id.id_name ? node->nod_data.nod_id.id_len : 0, node);
		struct dpp_node *f = node->nod_child;
		while (f) {
			f->nod_type = s_resolve_type(f, tab, arena);
			if (f->nod_kind == NOD_STRUCT_DECL || f->nod_kind == NOD_UNION_DECL)
				dpp_analyze_types(f, tab, arena, target);
			f = f->nod_next;
		}
		break;
	}
	case NOD_IDENTIFIER: {
		struct dpp_symbol *sym =
			dpp_symtab_lookup(tab, node->nod_data.nod_id.id_name, node->nod_data.nod_id.id_len);
		if (!sym) {
			if (node->nod_data.nod_id.id_len == 4 &&
			    memcmp(node->nod_data.nod_id.id_name, "NULL", 4) == 0) {
				node->nod_type = dpp_type_ptr(arena, dpp_type_new(arena, TYPE_VOID));
				return;
			}
			if (s_resolve_tracking_macro(node, arena)) return;
			struct dpp_node *builtin = s_inject_lazy_builtin(tab, arena, node->nod_data.nod_id.id_name,
			                                                 node->nod_data.nod_id.id_len);
			if (builtin) {
				sym = dpp_symtab_lookup(tab, node->nod_data.nod_id.id_name,
				                        node->nod_data.nod_id.id_len);
			}
		}
		if (sym && sym->sym_node) {
			node->nod_type = sym->sym_node->nod_type;
			node->nod_ref  = sym->sym_node;
		} else {
			dpp_diag_error(node, "use of undeclared identifier '%.*s'", (int)node->nod_data.nod_id.id_len,
			               node->nod_data.nod_id.id_name);
		}
		break;
	}
	case NOD_INT_LITERAL:
		node->nod_type = dpp_type_new(arena, TYPE_INT);
		break;
	case NOD_STRING_LITERAL:
		node->nod_type = dpp_type_ptr(arena, dpp_type_new(arena, TYPE_CHAR));
		break;
	case NOD_BINARY_EXPR: {
		dpp_analyze_types(node->nod_data.nod_op.op_lhs, tab, arena, target);
		s32 op = node->nod_data.nod_op.op_kind;
		if (op == OP_CALL) {
			if (s_resolve_builtin_math_macro(node, arena)) {
				dpp_analyze_types(node, tab, arena, target);
				return;
			}
			struct dpp_node *arg = node->nod_data.nod_op.op_rhs;
			while (arg) {
				dpp_analyze_types(arg, tab, arena, target);
				arg = arg->nod_next;
			}
			if (node->nod_data.nod_op.op_lhs->nod_type)
				node->nod_type = node->nod_data.nod_op.op_lhs->nod_type;
			else
				node->nod_type = dpp_type_new(arena, TYPE_INT);
		} else if (op == TOK_DOT || op == TOK_ARROW) {
			struct dpp_type *lhs_ty = (struct dpp_type *)node->nod_data.nod_op.op_lhs->nod_type;
			if (op == TOK_ARROW && lhs_ty && lhs_ty->ty_kind == TYPE_PTR)
				lhs_ty = lhs_ty->ty_next;
			if (lhs_ty && (lhs_ty->ty_kind == TYPE_STRUCT || lhs_ty->ty_kind == TYPE_UNION)) {
				struct dpp_node *st_decl = lhs_ty->ty_data.ty_struct.struct_node;
				if (st_decl) {
					struct dpp_node *member_id = node->nod_data.nod_op.op_rhs;
					struct dpp_node *f         = st_decl->nod_child;
					u32              idx       = 0;
					while (f) {
						if (f->nod_kind == NOD_VAR_DECL) {
							if (f->nod_data.nod_id.id_len ==
							            member_id->nod_data.nod_id.id_len &&
							    memcmp(f->nod_data.nod_id.id_name,
							           member_id->nod_data.nod_id.id_name,
							           f->nod_data.nod_id.id_len) == 0) {
								node->nod_type         = f->nod_type;
								node->nod_member_index = idx;
								member_id->nod_type    = f->nod_type;
								member_id->nod_ref     = f;
								break;
							}
							idx++;
						}
						f = f->nod_next;
					}
				}
			}
			if (!node->nod_type) node->nod_type = dpp_type_new(arena, TYPE_INT);
		} else {
			dpp_analyze_types(node->nod_data.nod_op.op_rhs, tab, arena, target);
			if (op == OP_EQ || op == OP_NE || op == OP_LT || op == OP_GT || op == OP_LE ||
				op == OP_GE || op == OP_LOGICAL_AND || op == OP_LOGICAL_OR) {
					node->nod_type = dpp_type_new(arena, TYPE_BOOL);
			} else if (op == OP_ADD || op == OP_SUB || op == OP_MUL || op == OP_DIV || op == OP_MOD) {
					node->nod_type = node->nod_data.nod_op.op_lhs->nod_type;
			} else if (op == OP_SUB && node->nod_data.nod_op.op_lhs->nod_type &&
					   ((struct dpp_type *)node->nod_data.nod_op.op_lhs->nod_type)->ty_kind == TYPE_PTR) {
					node->nod_type = dpp_type_new(arena, TYPE_LONG);
			} else {
					node->nod_type = node->nod_data.nod_op.op_lhs->nod_type;
			}

		}
		break;
	}
	case NOD_CAST_EXPR: {
		dpp_analyze_types(node->nod_child, tab, arena, target);
		node->nod_type = s_resolve_type(node, tab, arena);
		break;
	}
	case NOD_INDEX_EXPR: {
		dpp_analyze_types(node->nod_data.nod_op.op_lhs, tab, arena, target);
		dpp_analyze_types(node->nod_data.nod_op.op_rhs, tab, arena, target);
		struct dpp_type *lhs_ty = (struct dpp_type *)node->nod_data.nod_op.op_lhs->nod_type;
		if (lhs_ty && (lhs_ty->ty_kind == TYPE_PTR || lhs_ty->ty_kind == TYPE_ARRAY))
			node->nod_type = lhs_ty->ty_next;
		else
			node->nod_type = dpp_type_new(arena, TYPE_INT);
		break;
	}
	case NOD_COMPOUND_STMT: {
		dpp_symtab_push(tab);
		struct dpp_node *curr = node->nod_child;
		while (curr) {
			dpp_analyze_types(curr, tab, arena, target);
			curr = curr->nod_next;
		}
		dpp_symtab_pop(tab);
		break;
	}
	case NOD_IF_STMT: {
		struct dpp_node *cond     = node->nod_child;
		struct dpp_node *then_arm = cond->nod_next;
		struct dpp_node *else_arm = then_arm->nod_next;
		dpp_analyze_types(cond, tab, arena, target);
		dpp_analyze_types(then_arm, tab, arena, target);
		if (else_arm) dpp_analyze_types(else_arm, tab, arena, target);
		break;
	}
	case NOD_WHILE_STMT: {
		struct dpp_node *cond = node->nod_child;
		struct dpp_node *body = cond->nod_next;
		dpp_analyze_types(cond, tab, arena, target);
		dpp_analyze_types(body, tab, arena, target);
		break;
	}
	case NOD_RETURN_STMT: {
		if (node->nod_child) dpp_analyze_types(node->nod_child, tab, arena, target);
		break;
	}
	case NOD_UNARY_EXPR: {
		dpp_analyze_types(node->nod_child, tab, arena, target);
		node->nod_type = dpp_type_new(arena, TYPE_INT);
		break;
	}
	case NOD_SIZEOF: {
		dpp_analyze_types(node->nod_child, tab, arena, target);
		node->nod_type = dpp_type_new(arena, TYPE_INT);
		break;
	}
	case NOD_INIT_LIST: {
		struct dpp_node *curr = node->nod_child;
		while (curr) {
			dpp_analyze_types(curr, tab, arena, target);
			curr = curr->nod_next;
		}
		node->nod_type = dpp_type_new(arena, TYPE_VOID); /* Aggregate placeholder */
		break;
	}
	case NOD_EXPR_STMT:
		dpp_analyze_types(node->nod_child, tab, arena, target);
		break;
	default: {
		struct dpp_node *c = node->nod_child;
		while (c) {
			dpp_analyze_types(c, tab, arena, target);
			c = c->nod_next;
		}
		break;
	}
	}
}

void dpp_resolve_sizeof_recursive(struct dpp_node *node, struct dpp_target *target, struct dpp_arena *arena)
{
	if (!node) return;
	if (node->nod_kind == NOD_SIZEOF) {
		struct dpp_type *ty   = (struct dpp_type *)node->nod_child->nod_type;
		u32              size = 0;
		if (ty) {
			if (ty->ty_kind == TYPE_STRUCT || ty->ty_kind == TYPE_UNION) {
				struct dpp_node *st_node = ty->ty_data.ty_struct.struct_node;
				if (st_node && st_node->nod_child) {
					dpp_layout_compute(st_node, target);
					size = ty->ty_size;
				} else {
					dpp_diag_error(node, "invalid application of 'sizeof' to an incomplete type");
				}
			} else {
				size = ty->ty_size;
				if (size == 0) {
					if (ty->ty_kind == TYPE_PTR)
						size = 8;
					else if (ty->ty_kind == TYPE_LONG)
						size = 8;
					else
						size = 4;
				}
			}
		}
		node->nod_kind                 = NOD_INT_LITERAL;
		node->nod_data.nod_val.val_int = (s64)size;
	}
	struct dpp_node *c = node->nod_child;
	while (c) {
		dpp_resolve_sizeof_recursive(c, target, arena);
		c = c->nod_next;
	}
	if (node->nod_kind == NOD_BINARY_EXPR) {
		dpp_resolve_sizeof_recursive(node->nod_data.nod_op.op_lhs, target, arena);
		dpp_resolve_sizeof_recursive(node->nod_data.nod_op.op_rhs, target, arena);
	}
}

static u32 s_eval_sizeof_expr(struct dpp_node *size_node)
{
	if (!size_node) return 4;
	u32 base_size = 4;
	switch (size_node->nod_data.nod_id.id_type) {
	case TOK_CHAR:   base_size = 1; break;
	case TOK_SHORT:  base_size = 2; break;
	case TOK_INT:    base_size = 4; break;
	case TOK_LONG:   base_size = 8; break;
	case TOK_FLOAT:  base_size = 4; break;
	case TOK_DOUBLE: base_size = 8; break;
	case TOK_VOID:   base_size = 1; break;
	case TOK_BOOL:   base_size = 1; break;
	default: break;
	}
	for (u32 i = 0; i < size_node->nod_ptr_depth; i++) base_size = 8;
	return base_size;
}

static s64 s_eval_constant_expr(struct dpp_node *node)
{
	if (!node) return 0;
	switch (node->nod_kind) {
	case NOD_INT_LITERAL:
		return node->nod_data.nod_val.val_int;
	case NOD_SIZEOF:
		return (s64)s_eval_sizeof_expr(node->nod_child);
	case NOD_BINARY_EXPR: {
		s64 lhs = s_eval_constant_expr(node->nod_data.nod_op.op_lhs);
		s64 rhs = s_eval_constant_expr(node->nod_data.nod_op.op_rhs);
		switch ((enum dpp_op_kind)node->nod_data.nod_op.op_kind) {
		case OP_ADD: return lhs + rhs;
		case OP_SUB: return lhs - rhs;
		case OP_MUL: return lhs * rhs;
		case OP_DIV: return rhs ? lhs / rhs : 0;
		case OP_MOD: return rhs ? lhs % rhs : 0;
		default:     return 0;
		}
	}
	default:
		return 0;
	}
}

void dpp_eval_array_sizes(struct dpp_node *node)
{
	if (!node) return;
	if (node->nod_type) {
		struct dpp_type *ty = (struct dpp_type *)node->nod_type;
		while (ty) {
			if (ty->ty_kind == TYPE_ARRAY && ty->ty_data.ty_array.node) {
				s64 val = s_eval_constant_expr(ty->ty_data.ty_array.node);
				ty->ty_data.ty_array.size = (u32)val;
				if (ty->ty_next)
					ty->ty_size = ty->ty_next->ty_size * (u32)val;
				if (ty->ty_next)
					ty->ty_align = ty->ty_next->ty_align;
				ty->ty_data.ty_array.node = NULL;
			}
			ty = ty->ty_next;
		}
	}
	struct dpp_node *c = node->nod_child;
	while (c) {
		dpp_eval_array_sizes(c);
		c = c->nod_next;
	}
	if (node->nod_kind == NOD_BINARY_EXPR) {
		dpp_eval_array_sizes(node->nod_data.nod_op.op_lhs);
		dpp_eval_array_sizes(node->nod_data.nod_op.op_rhs);
	}
}
