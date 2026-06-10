#include "core/codegen/cg_llvm.h"
#include "c/lexer/token.h"
#include "core/sema/symbol.h"
#include "core/sema/type.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct dpp_label_map {
	const u8             *name;
	size_t                len;
	LLVMBasicBlockRef     block;
	struct dpp_label_map *next;
};

static struct dpp_label_map *g_labels = NULL;

static LLVMBasicBlockRef s_get_label_block(struct dpp_codegen *cg, const u8 *name, size_t len, LLVMValueRef func)
{
	struct dpp_label_map *curr = g_labels;
	while (curr) {
		if (curr->len == len && memcmp(curr->name, name, len) == 0) return curr->block;
		curr = curr->next;
	}
	char buf[256];
	snprintf(buf, sizeof(buf), "label.%.*s", (int)len, name);
	LLVMBasicBlockRef     bb = LLVMAppendBasicBlockInContext(cg->context, func, buf);
	struct dpp_label_map *m  = malloc(sizeof(*m));
	m->name                  = name;
	m->len                   = len;
	m->block                 = bb;
	m->next                  = g_labels;
	g_labels                 = m;
	return bb;
}

static void s_clear_labels()
{
	struct dpp_label_map *curr = g_labels;
	while (curr) {
		struct dpp_label_map *next = curr->next;
		free(curr);
		curr = next;
	}
	g_labels = NULL;
}

/* ------------------------------------------------------------------ */
/*  s_map_type – resolve a dpp_type to its LLVM type                  */
/* ------------------------------------------------------------------ */
static LLVMTypeRef s_map_type(struct dpp_codegen *cg, struct dpp_type *ty)
{
	/* FIX #1: returning ptr for NULL ty silently masks sema bugs and
	   produces invalid GEP IR (ptr as source element type).  i32 is a
	   safe non-pointer fallback so any misuse becomes obvious.          */
	if (!ty) return LLVMInt32TypeInContext(cg->context);

	if (ty->ty_backend_type && ty->ty_kind != TYPE_STRUCT && ty->ty_kind != TYPE_UNION)
		return (LLVMTypeRef)ty->ty_backend_type;

	LLVMTypeRef res = NULL;
	switch (ty->ty_kind) {
	case TYPE_VOID:
		res = LLVMVoidTypeInContext(cg->context);
		break;
	case TYPE_BOOL:
		res = LLVMInt1TypeInContext(cg->context);
		break;
	case TYPE_CHAR:
		res = LLVMInt8TypeInContext(cg->context);
		break;
	case TYPE_INT:
		res = LLVMInt32TypeInContext(cg->context);
		break;
	case TYPE_LONG:
		res = LLVMInt64TypeInContext(cg->context);
		break;
	case TYPE_FLOAT:
		res = LLVMFloatTypeInContext(cg->context);
		break;
	case TYPE_DOUBLE:
		res = LLVMDoubleTypeInContext(cg->context);
		break;
	case TYPE_PTR:
		res = LLVMPointerTypeInContext(cg->context, 0);
		break;
	case TYPE_ARRAY:
		res = LLVMArrayType(s_map_type(cg, ty->ty_next), ty->ty_data.ty_array.size);
		break;
	case TYPE_STRUCT:
	case TYPE_UNION: {
		struct dpp_node *st_node = ty->ty_data.ty_struct.struct_node;
		char             st_name[256];
		if (st_node && st_node->nod_data.nod_id.id_name) {
			snprintf(st_name, sizeof(st_name), "struct.%.*s", (int)st_node->nod_data.nod_id.id_len,
			         st_node->nod_data.nod_id.id_name);
		} else {
			snprintf(st_name, sizeof(st_name), "struct.anon.%p", (void *)ty);
		}
		LLVMTypeRef st_ty = LLVMGetTypeByName(cg->module, st_name);
		if (!st_ty) {
			st_ty               = LLVMStructCreateNamed(cg->context, st_name);
			ty->ty_backend_type = st_ty;
			if (st_node && st_node->nod_child) {
				LLVMTypeRef      fields[256];
				u32              f_count = 0;
				struct dpp_node *f       = st_node->nod_child;
				while (f && f_count < 256) {
					if (f->nod_kind == NOD_VAR_DECL) {
						fields[f_count++] = s_map_type(cg, (struct dpp_type *)f->nod_type);
					}
					f = f->nod_next;
				}
				LLVMStructSetBody(st_ty, fields, f_count, false);
			}
		}
		res = st_ty;
		break;
	}
	default:
		res = LLVMInt32TypeInContext(cg->context);
		break;
	}
	ty->ty_backend_type = res;
	return res;
}

/* ------------------------------------------------------------------ */
/*  s_cast – insert an explicit conversion between two LLVM values     */
/* ------------------------------------------------------------------ */
static LLVMValueRef s_cast(struct dpp_codegen *cg, LLVMValueRef val, struct dpp_type *from, struct dpp_type *to)
{
	if (!to) return val;
	LLVMTypeRef llvm_to = s_map_type(cg, to);
	if (!val) return LLVMConstNull(llvm_to);
	LLVMTypeRef llvm_from = LLVMTypeOf(val);
	if (llvm_from == llvm_to) return val;
	if (to->ty_kind == TYPE_BOOL) {
		if (LLVMGetTypeKind(llvm_from) == LLVMPointerTypeKind)
			return LLVMBuildICmp(cg->builder, LLVMIntNE, val, LLVMConstNull(llvm_from), "ptr2bool");
		if (LLVMGetTypeKind(llvm_from) == LLVMIntegerTypeKind && LLVMGetIntTypeWidth(llvm_from) == 1)
			return val;
		return LLVMBuildICmp(cg->builder, LLVMIntNE, val, LLVMConstInt(llvm_from, 0, false), "int2bool");
	}
	if (from && from->ty_kind <= TYPE_LONG && to->ty_kind <= TYPE_LONG) {
		u32 f_bits = (from->ty_kind == TYPE_BOOL)   ? 1
		             : (from->ty_kind == TYPE_CHAR) ? 8
		             : (from->ty_kind == TYPE_LONG) ? 64
		                                            : 32;
		u32 t_bits = (to->ty_kind == TYPE_BOOL)   ? 1
		             : (to->ty_kind == TYPE_CHAR) ? 8
		             : (to->ty_kind == TYPE_LONG) ? 64
		                                          : 32;
		if (f_bits < t_bits) {
			if (from->ty_kind == TYPE_BOOL) return LLVMBuildZExt(cg->builder, val, llvm_to, "zext");
			return LLVMBuildSExt(cg->builder, val, llvm_to, "sext");
		}
		if (f_bits > t_bits) return LLVMBuildTrunc(cg->builder, val, llvm_to, "trunc");
	}
	if (to->ty_kind == TYPE_PTR) {
		if (LLVMGetTypeKind(llvm_from) == LLVMIntegerTypeKind)
			return LLVMBuildIntToPtr(cg->builder, val, llvm_to, "i2p");
		return LLVMBuildBitCast(cg->builder, val, llvm_to, "p2p");
	}
	if (from && from->ty_kind == TYPE_PTR && to->ty_kind <= TYPE_LONG)
		return LLVMBuildPtrToInt(cg->builder, val, llvm_to, "p2i");
	return val;
}

/* forward decl */
static LLVMValueRef s_emit_node(struct dpp_codegen *cg, struct dpp_node *node);

/* ------------------------------------------------------------------ */
/*  s_emit_addr – compute the l-value address of a node               */
/* ------------------------------------------------------------------ */
static LLVMValueRef s_emit_addr(struct dpp_codegen *cg, struct dpp_node *node)
{
	if (!node) return NULL;
	if (node->nod_kind == NOD_IDENTIFIER) return node->nod_ref ? node->nod_ref->nod_llvm_val : NULL;
	if (node->nod_kind == NOD_UNARY_EXPR && node->nod_data.nod_op.op_kind == '*')
		return s_emit_node(cg, node->nod_child);

	if (node->nod_kind == NOD_BINARY_EXPR) {
		s32 op = node->nod_data.nod_op.op_kind;

		/* -----------------------------------------------------------
		 * FIX #2: struct/union field access via DOT or ARROW
		 *
		 * The GEP2 source-element type MUST be the struct/union type,
		 * never an opaque ptr.  We peel away ALL pointer layers so
		 * that multi-level pointers (Foo**, Foo***, …) are handled
		 * correctly and the final type is always the aggregate.
		 * ----------------------------------------------------------- */
		if (op == TOK_DOT || op == TOK_ARROW) {
			struct dpp_node *ln  = node->nod_data.nod_op.op_lhs;
			struct dpp_type *lty = (struct dpp_type *)ln->nod_type;
			LLVMValueRef     bp;

			if (op == TOK_ARROW) {
				/* ARROW: dereference the value produced by the LHS */
				bp = s_emit_node(cg, ln);
			} else {
				/* DOT: take the address of the LHS l-value */
				bp = s_emit_addr(cg, ln);
			}
			if (!bp) return NULL;

			/* Peel every pointer layer until we reach the aggregate */
			while (lty && lty->ty_kind == TYPE_PTR && lty->ty_next)
				lty = lty->ty_next;

			/* Safety: if we didn't land on a struct/union, bail out */
			if (!lty || (lty->ty_kind != TYPE_STRUCT && lty->ty_kind != TYPE_UNION)) return NULL;

			LLVMTypeRef  pointee_ty = s_map_type(cg, lty);
			LLVMValueRef indices[]  = {
                                LLVMConstInt(LLVMInt32TypeInContext(cg->context), 0, false),
                                LLVMConstInt(LLVMInt32TypeInContext(cg->context), node->nod_member_index, false)};
			return LLVMBuildInBoundsGEP2(cg->builder, pointee_ty, bp, indices, 2, "maddr");
		}

		/* ---------- subscript [] ----------------------------------- */
		if (op == TOK_LBRACKET) {
			LLVMValueRef     base    = s_emit_node(cg, node->nod_data.nod_op.op_lhs);
			LLVMValueRef     idx     = s_emit_node(cg, node->nod_data.nod_op.op_rhs);
			struct dpp_type *base_ty = (struct dpp_type *)node->nod_data.nod_op.op_lhs->nod_type;
			if (base_ty && base_ty->ty_kind == TYPE_ARRAY) {
				LLVMTypeRef  arr_ty = s_map_type(cg, base_ty);
				LLVMValueRef idxs[] = {LLVMConstInt(LLVMInt32TypeInContext(cg->context), 0, false),
				                       idx};
				return LLVMBuildInBoundsGEP2(cg->builder, arr_ty, base, idxs, 2, "idxaddr");
			} else {
				struct dpp_type *res_ty  = (struct dpp_type *)node->nod_type;
				LLVMTypeRef      elem_ty = s_map_type(cg, res_ty);
				return LLVMBuildInBoundsGEP2(cg->builder, elem_ty, base, &idx, 1, "ptraddr");
			}
		}
	}
	return NULL;
}

/* ------------------------------------------------------------------ */
/*  s_emit_function                                                    */
/* ------------------------------------------------------------------ */
static LLVMValueRef s_emit_function(struct dpp_codegen *cg, struct dpp_node *node)
{
	if (!node) return NULL;
	s_clear_labels();
	struct dpp_type *ret_ty      = (struct dpp_type *)node->nod_type;
	LLVMTypeRef      llvm_ret_ty = s_map_type(cg, ret_ty);
	LLVMTypeRef      param_types[64];
	struct dpp_node *params[64];
	u32              param_count = 0;
	struct dpp_node *p_it        = node->nod_child;
	while (p_it && p_it->nod_kind == NOD_PARAM_DECL && param_count < 64) {
		struct dpp_type *pt = (struct dpp_type *)p_it->nod_type;
		if (pt && pt->ty_kind != TYPE_VOID) {
			params[param_count]        = p_it;
			param_types[param_count++] = s_map_type(cg, pt);
		}
		p_it = p_it->nod_next;
	}
	LLVMTypeRef func_type = LLVMFunctionType(llvm_ret_ty, param_types, param_count, node->nod_is_variadic);
	char        name[256];
	snprintf(name, sizeof(name), "%.*s", (int)node->nod_data.nod_id.id_len, node->nod_data.nod_id.id_name);
	LLVMValueRef func = LLVMGetNamedFunction(cg->module, name);
	if (!func) func = LLVMAddFunction(cg->module, name, func_type);
	node->nod_llvm_val = func;
	if (node->nod_storage & NOD_STORAGE_STATIC) LLVMSetLinkage(func, LLVMInternalLinkage);

	struct dpp_node *old_func = cg->current_func;
	cg->current_func          = node;
	struct dpp_node *body     = p_it;
	if (body && body->nod_kind == NOD_COMPOUND_STMT) {
		LLVMBasicBlockRef entry  = LLVMAppendBasicBlockInContext(cg->context, func, "entry");
		LLVMBasicBlockRef old_ip = LLVMGetInsertBlock(cg->builder);
		LLVMPositionBuilderAtEnd(cg->builder, entry);

		for (u32 i = 0; i < param_count; i++) {
			LLVMTypeRef p_ty = param_types[i];
			char        p_name[256];
			snprintf(p_name, sizeof(p_name), "%.*s.addr", (int)params[i]->nod_data.nod_id.id_len,
			         params[i]->nod_data.nod_id.id_name);
			LLVMValueRef alloca = LLVMBuildAlloca(cg->builder, p_ty, p_name);
			LLVMBuildStore(cg->builder, LLVMGetParam(func, i), alloca);
			params[i]->nod_llvm_val = alloca;
		}

		struct dpp_node *stmt = body->nod_child;
		while (stmt) {
			s_emit_node(cg, stmt);
			stmt = stmt->nod_next;
		}

		if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(cg->builder))) {
			if (ret_ty && ret_ty->ty_kind == TYPE_VOID)
				LLVMBuildRetVoid(cg->builder);
			else
				LLVMBuildRet(cg->builder, LLVMConstNull(llvm_ret_ty));
		}
		if (old_ip) LLVMPositionBuilderAtEnd(cg->builder, old_ip);
	}
	cg->current_func = old_func;
	return func;
}

/* ------------------------------------------------------------------ */
/*  s_emit_node – main recursive IR emitter                            */
/* ------------------------------------------------------------------ */
static LLVMValueRef s_emit_node(struct dpp_codegen *cg, struct dpp_node *node)
{
	if (!node || node->nod_kind == NOD_INVALID) return NULL;
	switch (node->nod_kind) {

	/* ---- function declaration ------------------------------------ */
	case NOD_FUNCTION_DECL:
		return s_emit_function(cg, node);

	/* ---- variable declaration ------------------------------------ */
	case NOD_VAR_DECL: {
		if (node->nod_type_flags & NOD_TYPE_TYPEDEF) return NULL;
		struct dpp_type *t       = (struct dpp_type *)node->nod_type;
		LLVMTypeRef      llvm_ty = s_map_type(cg, t);
		char             name[256];
		snprintf(name, sizeof(name), "%.*s", (int)node->nod_data.nod_id.id_len, node->nod_data.nod_id.id_name);
		LLVMBasicBlockRef current_bb = LLVMGetInsertBlock(cg->builder);
		if (!current_bb) {
			/* global */
			LLVMValueRef g = LLVMAddGlobal(cg->module, llvm_ty, name);
			if (node->nod_storage & NOD_STORAGE_STATIC) LLVMSetLinkage(g, LLVMInternalLinkage);
			if (!(node->nod_storage & NOD_STORAGE_EXTERN)) LLVMSetInitializer(g, LLVMConstNull(llvm_ty));
			node->nod_llvm_val = g;
			return g;
		} else {
			LLVMValueRef alloca = LLVMBuildAlloca(cg->builder, llvm_ty, name);
			printf("TRACE: Declaring variable '%s' at alloca %p\n", name, (void*)alloca);

			node->nod_llvm_val  = alloca;
			if (node->nod_child) {
				if (node->nod_child->nod_kind == NOD_INIT_LIST) {
					LLVMBuildStore(cg->builder, LLVMConstNull(llvm_ty), alloca);
				} else {
					LLVMValueRef init = s_emit_node(cg, node->nod_child);
					printf("TRACE: Storing init value to %p\n", (void*)alloca);
					LLVMBuildStore(cg->builder, init, alloca);
				}
			}
			return alloca;
		}
	}

	/* ---- label statement ----------------------------------------- */
	case NOD_LABEL_STMT: {
		LLVMValueRef      func = LLVMGetBasicBlockParent(LLVMGetInsertBlock(cg->builder));
		LLVMBasicBlockRef bb =
			s_get_label_block(cg, node->nod_data.nod_id.id_name, node->nod_data.nod_id.id_len, func);
		if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(cg->builder))) LLVMBuildBr(cg->builder, bb);
		LLVMPositionBuilderAtEnd(cg->builder, bb);
		if (node->nod_child) s_emit_node(cg, node->nod_child);
		return NULL;
	}

	/* ---- goto statement ------------------------------------------ */
	case NOD_GOTO_STMT: {
		LLVMValueRef      func = LLVMGetBasicBlockParent(LLVMGetInsertBlock(cg->builder));
		LLVMBasicBlockRef bb =
			s_get_label_block(cg, node->nod_data.nod_id.id_name, node->nod_data.nod_id.id_len, func);
		return LLVMBuildBr(cg->builder, bb);
	}

	/* ---- identifier (variable / function reference) --------------- */
	case NOD_IDENTIFIER: {
		if (node->nod_ref) {
			if (node->nod_ref->nod_kind == NOD_FUNCTION_DECL) {
				if (!node->nod_ref->nod_llvm_val) s_emit_function(cg, node->nod_ref);
				return node->nod_ref->nod_llvm_val;
			}
			if (node->nod_ref->nod_llvm_val) {
				struct dpp_type *t = (struct dpp_type *)node->nod_type;
				if (t &&
				    (t->ty_kind == TYPE_ARRAY || t->ty_kind == TYPE_STRUCT || t->ty_kind == TYPE_UNION))
					return node->nod_ref->nod_llvm_val;
				printf("TRACE: Loading from alloca %p\n", (void*)node->nod_ref->nod_llvm_val);
				LLVMValueRef val = LLVMBuildLoad2(cg->builder, s_map_type(cg, t), node->nod_ref->nod_llvm_val, "loadtmp");
				return val;

			}
		}
		return LLVMConstInt(LLVMInt32TypeInContext(cg->context), 0, false);
	}

	/* ---- literals ------------------------------------------------- */
	case NOD_INT_LITERAL:
		return LLVMConstInt(LLVMInt32TypeInContext(cg->context), node->nod_data.nod_val.val_int, true);
	case NOD_SIZEOF:
		return LLVMConstInt(LLVMInt32TypeInContext(cg->context), node->nod_data.nod_val.val_int, false);
	case NOD_STRING_LITERAL: {
		size_t len = node->nod_data.nod_id.id_len;
		u8    *buf = malloc(len + 1);
		memcpy(buf, node->nod_data.nod_id.id_name, len);
		buf[len]       = 0;
		LLVMValueRef g = LLVMBuildGlobalStringPtr(cg->builder, (const char *)buf, "strtmp");
		free(buf);
		return g;
	}

	/* ---- return statement ---------------------------------------- */
	case NOD_RETURN_STMT: {
		LLVMValueRef     val     = s_emit_node(cg, node->nod_child);
		struct dpp_type *fret_ty = cg->current_func ? (struct dpp_type *)cg->current_func->nod_type : NULL;
		if (val)
			return LLVMBuildRet(cg->builder,
			                    s_cast(cg, val, (struct dpp_type *)node->nod_child->nod_type, fret_ty));
		if (fret_ty && fret_ty->ty_kind == TYPE_VOID) return LLVMBuildRetVoid(cg->builder);
		return LLVMBuildRet(cg->builder, LLVMConstNull(s_map_type(cg, fret_ty)));
	}

	/* ---- if statement -------------------------------------------- */
	case NOD_IF_STMT: {
		LLVMValueRef      cv       = s_emit_node(cg, node->nod_child);
		struct dpp_type  *bt       = dpp_type_new(cg->arena, TYPE_BOOL);
		LLVMValueRef      cond     = s_cast(cg, cv, (struct dpp_type *)node->nod_child->nod_type, bt);
		LLVMValueRef      func     = LLVMGetBasicBlockParent(LLVMGetInsertBlock(cg->builder));
		LLVMBasicBlockRef then_bb  = LLVMAppendBasicBlockInContext(cg->context, func, "then");
		LLVMBasicBlockRef else_bb  = LLVMAppendBasicBlockInContext(cg->context, func, "else");
		LLVMBasicBlockRef merge_bb = LLVMAppendBasicBlockInContext(cg->context, func, "merge");
		LLVMBuildCondBr(cg->builder, cond, then_bb, else_bb);
		LLVMPositionBuilderAtEnd(cg->builder, then_bb);
		s_emit_node(cg, node->nod_child->nod_next);
		if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(cg->builder))) LLVMBuildBr(cg->builder, merge_bb);
		LLVMPositionBuilderAtEnd(cg->builder, else_bb);
		if (node->nod_child->nod_next->nod_next) s_emit_node(cg, node->nod_child->nod_next->nod_next);
		if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(cg->builder))) LLVMBuildBr(cg->builder, merge_bb);
		LLVMPositionBuilderAtEnd(cg->builder, merge_bb);
		return NULL;
	}

	/* ---- while statement ----------------------------------------- */
	case NOD_DO_WHILE_STMT: {
		LLVMValueRef      func    = LLVMGetBasicBlockParent(LLVMGetInsertBlock(cg->builder));
		LLVMBasicBlockRef body_bb = LLVMAppendBasicBlockInContext(cg->context, func, "dowhile.body");
		LLVMBasicBlockRef cond_bb = LLVMAppendBasicBlockInContext(cg->context, func, "dowhile.cond");
		LLVMBasicBlockRef end_bb  = LLVMAppendBasicBlockInContext(cg->context, func, "dowhile.end");
		LLVMBuildBr(cg->builder, body_bb);
		LLVMPositionBuilderAtEnd(cg->builder, body_bb);
		if (node->nod_child) s_emit_node(cg, node->nod_child);
		if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(cg->builder))) LLVMBuildBr(cg->builder, cond_bb);
		LLVMPositionBuilderAtEnd(cg->builder, cond_bb);
		if (node->nod_child && node->nod_child->nod_next) {
			LLVMValueRef     cv = s_emit_node(cg, node->nod_child->nod_next);
			struct dpp_type *bt = dpp_type_new(cg->arena, TYPE_BOOL);
			LLVMValueRef cond = s_cast(cg, cv, (struct dpp_type *)node->nod_child->nod_next->nod_type, bt);
			LLVMBuildCondBr(cg->builder, cond, body_bb, end_bb);
		} else {
			LLVMBuildBr(cg->builder, end_bb);
		}
		LLVMPositionBuilderAtEnd(cg->builder, end_bb);
		return NULL;
	}

	case NOD_WHILE_STMT: {
		LLVMValueRef      func    = LLVMGetBasicBlockParent(LLVMGetInsertBlock(cg->builder));
		LLVMBasicBlockRef cond_bb = LLVMAppendBasicBlockInContext(cg->context, func, "while.cond");
		LLVMBasicBlockRef body_bb = LLVMAppendBasicBlockInContext(cg->context, func, "while.body");
		LLVMBasicBlockRef end_bb  = LLVMAppendBasicBlockInContext(cg->context, func, "while.end");
		LLVMBuildBr(cg->builder, cond_bb);
		LLVMPositionBuilderAtEnd(cg->builder, cond_bb);
		LLVMValueRef     cv   = s_emit_node(cg, node->nod_child);
		struct dpp_type *bt   = dpp_type_new(cg->arena, TYPE_BOOL);
		LLVMValueRef     cond = s_cast(cg, cv, (struct dpp_type *)node->nod_child->nod_type, bt);
		LLVMBuildCondBr(cg->builder, cond, body_bb, end_bb);
		LLVMPositionBuilderAtEnd(cg->builder, body_bb);
		s_emit_node(cg, node->nod_child->nod_next);
		if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(cg->builder))) LLVMBuildBr(cg->builder, cond_bb);
		LLVMPositionBuilderAtEnd(cg->builder, end_bb);
		return NULL;
	}

	case NOD_FOR_STMT: {
		LLVMValueRef      func    = LLVMGetBasicBlockParent(LLVMGetInsertBlock(cg->builder));
		LLVMBasicBlockRef cond_bb = LLVMAppendBasicBlockInContext(cg->context, func, "for.cond");
		LLVMBasicBlockRef body_bb = LLVMAppendBasicBlockInContext(cg->context, func, "for.body");
		LLVMBasicBlockRef inc_bb  = LLVMAppendBasicBlockInContext(cg->context, func, "for.inc");
		LLVMBasicBlockRef end_bb  = LLVMAppendBasicBlockInContext(cg->context, func, "for.end");

		struct dpp_node *init = node->nod_child;
		struct dpp_node *cond = init ? init->nod_next : NULL;
		struct dpp_node *inc  = cond ? cond->nod_next : NULL;
		struct dpp_node *body = inc ? inc->nod_next : (cond ? cond->nod_next : (init ? init->nod_next : NULL));

		if (init) s_emit_node(cg, init);
		LLVMBuildBr(cg->builder, cond_bb);

		LLVMPositionBuilderAtEnd(cg->builder, cond_bb);
		if (cond) {
			LLVMValueRef     cv   = s_emit_node(cg, cond);
			struct dpp_type *bt   = dpp_type_new(cg->arena, TYPE_BOOL);
			LLVMValueRef     cval = s_cast(cg, cv, (struct dpp_type *)cond->nod_type, bt);
			LLVMBuildCondBr(cg->builder, cval, body_bb, end_bb);
		} else {
			LLVMBuildBr(cg->builder, body_bb);
		}

		LLVMPositionBuilderAtEnd(cg->builder, body_bb);
		if (body) s_emit_node(cg, body);
		LLVMBuildBr(cg->builder, inc_bb);

		LLVMPositionBuilderAtEnd(cg->builder, inc_bb);
		if (inc) {
			s_emit_node(cg, inc);
		}
		LLVMBuildBr(cg->builder, cond_bb);

		LLVMPositionBuilderAtEnd(cg->builder, end_bb);
		return NULL;
	}

	/* ---- binary expressions -------------------------------------- */
	case NOD_BINARY_EXPR: {
		s32 op = node->nod_data.nod_op.op_kind;

		/* ---- assignment ---- */
		if (op == OP_ASSIGN) {
			if (node->nod_data.nod_op.op_lhs->nod_type_flags & NOD_TYPE_CONST) {
				fprintf(stderr, "error: assignment to read-only variable\n");
				return NULL;
			}
			LLVMValueRef rv   = s_emit_node(cg, node->nod_data.nod_op.op_rhs);
			LLVMValueRef addr = s_emit_addr(cg, node->nod_data.nod_op.op_lhs);
			if (addr && rv) {
				rv = s_cast(cg, rv, (struct dpp_type *)node->nod_data.nod_op.op_rhs->nod_type,
				            (struct dpp_type *)node->nod_data.nod_op.op_lhs->nod_type);
				LLVMBuildStore(cg->builder, rv, addr);
				return rv;
			}
			return NULL;
		}

		/* ---- function call ---- */
		if (op == OP_CALL) {
			struct dpp_node *lhs = node->nod_data.nod_op.op_lhs;
			if (!lhs || !lhs->nod_ref || lhs->nod_ref->nod_kind != NOD_FUNCTION_DECL)
				return LLVMConstInt(LLVMInt32TypeInContext(cg->context), 0, false);
			struct dpp_node *fdecl = lhs->nod_ref;
			char             name[256];
			snprintf(name, sizeof(name), "%.*s", (int)fdecl->nod_data.nod_id.id_len,
			         (const char *)fdecl->nod_data.nod_id.id_name);

			/* va_start */
			if (strcmp(name, "va_start") == 0 || strcmp(name, "__builtin_va_start") == 0) {
				struct dpp_node *args = node->nod_data.nod_op.op_rhs;
				if (args && args->nod_ref && args->nod_ref->nod_llvm_val) {
					LLVMTypeRef va_ty = LLVMFunctionType(
						LLVMVoidTypeInContext(cg->context),
						(LLVMTypeRef[]){LLVMPointerTypeInContext(cg->context, 0)}, 1, false);
					LLVMValueRef va_f = LLVMGetNamedFunction(cg->module, "llvm.va_start");
					if (!va_f) va_f = LLVMAddFunction(cg->module, "llvm.va_start", va_ty);
					return LLVMBuildCall2(cg->builder, va_ty, va_f,
					                      (LLVMValueRef[]){args->nod_ref->nod_llvm_val}, 1, "");
				}
				return NULL;
			}
			/* va_end */
			if (strcmp(name, "va_end") == 0 || strcmp(name, "__builtin_va_end") == 0) {
				struct dpp_node *args = node->nod_data.nod_op.op_rhs;
				if (args && args->nod_ref && args->nod_ref->nod_llvm_val) {
					LLVMTypeRef va_ty = LLVMFunctionType(
						LLVMVoidTypeInContext(cg->context),
						(LLVMTypeRef[]){LLVMPointerTypeInContext(cg->context, 0)}, 1, false);
					LLVMValueRef va_f = LLVMGetNamedFunction(cg->module, "llvm.va_end");
					if (!va_f) va_f = LLVMAddFunction(cg->module, "llvm.va_end", va_ty);
					return LLVMBuildCall2(cg->builder, va_ty, va_f,
					                      (LLVMValueRef[]){args->nod_ref->nod_llvm_val}, 1, "");
				}
				return NULL;
			}

			LLVMValueRef func = LLVMGetNamedFunction(cg->module, name);
			if (!func) func = s_emit_function(cg, fdecl);
			struct dpp_type *fret_ty      = (struct dpp_type *)fdecl->nod_type;
			LLVMTypeRef      llvm_fret_ty = s_map_type(cg, fret_ty);
			LLVMTypeRef      f_p_tys[64];
			u32              f_p_count = 0;
			struct dpp_node *fp_it     = fdecl->nod_child;
			while (fp_it && fp_it->nod_kind == NOD_PARAM_DECL && f_p_count < 64) {
				struct dpp_type *pt = (struct dpp_type *)fp_it->nod_type;
				if (pt && pt->ty_kind != TYPE_VOID) f_p_tys[f_p_count++] = s_map_type(cg, pt);
				fp_it = fp_it->nod_next;
			}
			LLVMTypeRef call_func_type =
				LLVMFunctionType(llvm_fret_ty, f_p_tys, f_p_count, fdecl->nod_is_variadic);
			LLVMValueRef     args[64];
			u32              arg_count = 0;
			struct dpp_node *arg_it    = node->nod_data.nod_op.op_rhs;
			while (arg_it && arg_count < 64) {
					LLVMValueRef av = s_emit_node(cg, arg_it);
					if (av) {

					LLVMTypeRef pt = (arg_count < f_p_count) ? f_p_tys[arg_count] : NULL;
					if (pt && LLVMGetTypeKind(pt) == LLVMStructTypeKind &&
					    LLVMGetTypeKind(LLVMTypeOf(av)) == LLVMPointerTypeKind) {
						av = LLVMBuildLoad2(cg->builder, pt, av, "structarg");
					}
					args[arg_count++] = av;
				} else
					args[arg_count++] = LLVMConstNull(LLVMPointerTypeInContext(cg->context, 0));
				arg_it = arg_it->nod_next;
			}
			return LLVMBuildCall2(cg->builder, call_func_type, func, args, arg_count,
			                      (fret_ty && fret_ty->ty_kind == TYPE_VOID) ? "" : "calltmp");
		}

		/* ---- dot / arrow (struct field r-value) ---- */
		if (op == OP_DOT || op == OP_ARROW) {
			LLVMValueRef gep = s_emit_addr(cg, node);
			if (!gep) return NULL;
			struct dpp_type *res_ty = (struct dpp_type *)node->nod_type;
			if (res_ty && (res_ty->ty_kind == TYPE_ARRAY || res_ty->ty_kind == TYPE_STRUCT ||
			               res_ty->ty_kind == TYPE_UNION))
				return gep;
			return LLVMBuildLoad2(cg->builder, s_map_type(cg, res_ty), gep, "mload");
		}

		/* ---- evaluate both sides for remaining operators ---- */
		LLVMValueRef lhs = s_emit_node(cg, node->nod_data.nod_op.op_lhs);
		LLVMValueRef rhs = s_emit_node(cg, node->nod_data.nod_op.op_rhs);
		if (!lhs || !rhs) {
			return LLVMConstInt(LLVMInt32TypeInContext(cg->context), 0, false);
		}

		/* ----------------------------------------------------------
		 * FIX #3: short-circuit evaluation for && and ||
		 *
		 * The old code used bitwise And/Or which evaluates BOTH sides
		 * unconditionally.  C requires the RHS to be evaluated only
		 * when needed.  We emit a branch, evaluate RHS lazily, and
		 * merge with a phi node.
		 * ---------------------------------------------------------- */
		if (op == OP_LOGICAL_AND || op == OP_LOGICAL_OR) {
			struct dpp_type *bt   = dpp_type_new(cg->arena, TYPE_BOOL);
			LLVMValueRef     func = LLVMGetBasicBlockParent(LLVMGetInsertBlock(cg->builder));

			const char       *op_tag   = (op == OP_LOGICAL_AND) ? "and" : "or";
			LLVMBasicBlockRef rhs_bb   = LLVMAppendBasicBlockInContext(cg->context, func, op_tag);
			LLVMBasicBlockRef merge_bb = LLVMAppendBasicBlockInContext(cg->context, func, "merge");
			LLVMBasicBlockRef skip_bb  = LLVMAppendBasicBlockInContext(
                                cg->context, func, (op == OP_LOGICAL_AND) ? "and.skip" : "or.skip");

			LLVMValueRef lhs_val =
				s_cast(cg, lhs, (struct dpp_type *)node->nod_data.nod_op.op_lhs->nod_type, bt);

			/* AND: skip if lhs==false (0);  OR: skip if lhs==true (1) */
			LLVMIntPredicate skip_pred = (op == OP_LOGICAL_AND) ? LLVMIntEQ : LLVMIntNE;
			LLVMBuildCondBr(cg->builder, lhs_val, (op == OP_LOGICAL_AND) ? rhs_bb : skip_bb,
			                (op == OP_LOGICAL_AND) ? skip_bb : rhs_bb);

			/* skip path: short-circuit result */
			LLVMPositionBuilderAtEnd(cg->builder, skip_bb);
			LLVMValueRef skip_val = (op == OP_LOGICAL_AND)
			                                ? LLVMConstInt(LLVMInt1TypeInContext(cg->context), 0, false)
			                                : LLVMConstInt(LLVMInt1TypeInContext(cg->context), 1, false);
			LLVMBuildBr(cg->builder, merge_bb);
			LLVMBasicBlockRef skip_end = LLVMGetInsertBlock(cg->builder);

			/* rhs path: evaluate RHS */
			LLVMPositionBuilderAtEnd(cg->builder, rhs_bb);
			LLVMValueRef rhs_val =
				s_cast(cg, rhs, (struct dpp_type *)node->nod_data.nod_op.op_rhs->nod_type, bt);
			LLVMBuildBr(cg->builder, merge_bb);
			LLVMBasicBlockRef rhs_end = LLVMGetInsertBlock(cg->builder);

			/* merge: phi node */
			LLVMPositionBuilderAtEnd(cg->builder, merge_bb);
			LLVMValueRef      phi = LLVMBuildPhi(cg->builder, LLVMInt1TypeInContext(cg->context), op_tag);
			LLVMValueRef      phi_vals[] = {skip_val, rhs_val};
			LLVMBasicBlockRef phi_bbs[]  = {skip_end, rhs_end};
			LLVMAddIncoming(phi, phi_vals, phi_bbs, 2);
			return phi;
		}

		/* ---- normalize integer types for arithmetic/comparison ---- */
		{
			struct dpp_type *lty = (struct dpp_type *)node->nod_data.nod_op.op_lhs->nod_type;
			struct dpp_type *rty = (struct dpp_type *)node->nod_data.nod_op.op_rhs->nod_type;
			int is_comp = (op == OP_EQ || op == OP_NE || op == OP_LT || op == OP_GT || op == OP_LE ||
			               op == OP_GE);

			if (is_comp) {
				if (lty && lty->ty_kind == TYPE_LONG && rty && rty->ty_kind <= TYPE_INT)
					rhs = s_cast(cg, rhs, rty, lty);
				else if (rty && rty->ty_kind == TYPE_LONG && lty && lty->ty_kind <= TYPE_INT)
					lhs = s_cast(cg, lhs, lty, rty);
			} else if (op != OP_LOGICAL_AND && op != OP_LOGICAL_OR) {
				struct dpp_type *res_ty = (struct dpp_type *)node->nod_type;
				if (res_ty && res_ty->ty_kind != TYPE_PTR && res_ty->ty_kind != TYPE_ARRAY &&
				    res_ty->ty_kind != TYPE_FLOAT && res_ty->ty_kind != TYPE_DOUBLE) {
					if (lty && lty->ty_kind <= TYPE_LONG && rty && rty->ty_kind <= TYPE_LONG) {
						lhs = s_cast(cg, lhs, lty, res_ty);
						rhs = s_cast(cg, rhs, rty, res_ty);
					}
				}

			}
		}

		/* ---- remaining arithmetic / comparison ops ---- */
		switch (op) {
		case OP_ADD: {
			struct dpp_type *lty = (struct dpp_type *)node->nod_data.nod_op.op_lhs->nod_type;
			if (lty && (lty->ty_kind == TYPE_PTR || lty->ty_kind == TYPE_ARRAY)) {
				struct dpp_type *ety = lty->ty_next;
				LLVMTypeRef      lem = ety ? s_map_type(cg, ety) : LLVMInt8TypeInContext(cg->context);
				LLVMValueRef     idx =
					s_cast(cg, rhs, (struct dpp_type *)node->nod_data.nod_op.op_rhs->nod_type,
				               dpp_type_new(cg->arena, TYPE_LONG));
				return LLVMBuildGEP2(cg->builder, lem, lhs, &idx, 1, "gep.add");
			}
			struct dpp_type *rty = (struct dpp_type *)node->nod_data.nod_op.op_rhs->nod_type;
			if (rty && (rty->ty_kind == TYPE_PTR || rty->ty_kind == TYPE_ARRAY)) {
				struct dpp_type *ety = rty->ty_next;
				LLVMTypeRef      lem = ety ? s_map_type(cg, ety) : LLVMInt8TypeInContext(cg->context);
				LLVMValueRef     idx =
					s_cast(cg, lhs, (struct dpp_type *)node->nod_data.nod_op.op_lhs->nod_type,
				               dpp_type_new(cg->arena, TYPE_LONG));
				return LLVMBuildGEP2(cg->builder, lem, rhs, &idx, 1, "gep.add");
			}
			printf("DEBUG: Emitting LLVMBuildAdd for %p + %p\n", (void*)lhs, (void*)rhs);
			if (LLVMGetTypeKind(LLVMTypeOf(lhs)) == LLVMPointerTypeKind)
				lhs = LLVMBuildLoad2(cg->builder, s_map_type(cg, (struct dpp_type *)node->nod_data.nod_op.op_lhs->nod_type), lhs, "load.lhs");
			if (LLVMGetTypeKind(LLVMTypeOf(rhs)) == LLVMPointerTypeKind)
				rhs = LLVMBuildLoad2(cg->builder, s_map_type(cg, (struct dpp_type *)node->nod_data.nod_op.op_rhs->nod_type), rhs, "load.rhs");

			char *lhs_t = LLVMPrintTypeToString(LLVMTypeOf(lhs));
			char *rhs_t = LLVMPrintTypeToString(LLVMTypeOf(rhs));
			printf("DEBUG: Types: %s + %s\n", lhs_t, rhs_t);
			LLVMDisposeMessage(lhs_t);
			LLVMDisposeMessage(rhs_t);
			LLVMValueRef res = LLVMBuildAdd(cg->builder, lhs, rhs, "add");
			printf("DEBUG: OP_ADD result = %p\n", (void*)res);
			return res;
		}
		case OP_SUB: {
			struct dpp_type *lty = (struct dpp_type *)node->nod_data.nod_op.op_lhs->nod_type;
			if (lty && (lty->ty_kind == TYPE_PTR || lty->ty_kind == TYPE_ARRAY)) {
				struct dpp_type *rty = (struct dpp_type *)node->nod_data.nod_op.op_rhs->nod_type;
				if (rty && (rty->ty_kind == TYPE_PTR || rty->ty_kind == TYPE_ARRAY)) {
					LLVMValueRef     l    = LLVMBuildPtrToInt(cg->builder, lhs,
					                                          LLVMInt64TypeInContext(cg->context), "lp");
					LLVMValueRef     r    = LLVMBuildPtrToInt(cg->builder, rhs,
					                                          LLVMInt64TypeInContext(cg->context), "rp");
					LLVMValueRef     diff = LLVMBuildSub(cg->builder, l, r, "ptrdiff");
					struct dpp_type *ety  = lty->ty_next;
					if (ety && ety->ty_size > 0) {
						LLVMValueRef sz = LLVMConstInt(LLVMInt64TypeInContext(cg->context),
						                               ety->ty_size, false);
						diff            = LLVMBuildSDiv(cg->builder, diff, sz, "ptrdiff.norm");
					}
					struct dpp_type *res_ty = (struct dpp_type *)node->nod_type;
					if (res_ty) diff = s_cast(cg, diff, NULL, dpp_type_new(cg->arena, TYPE_LONG));
					return diff;
				}
				struct dpp_type *ety = lty->ty_next;
				LLVMTypeRef      lem = ety ? s_map_type(cg, ety) : LLVMInt8TypeInContext(cg->context);
				LLVMValueRef     idx = s_cast(
                                        cg, rhs, rty ? (struct dpp_type *)node->nod_data.nod_op.op_rhs->nod_type : NULL,
                                        dpp_type_new(cg->arena, TYPE_LONG));
				LLVMValueRef neg = LLVMBuildNeg(cg->builder, idx, "neg.idx");
				return LLVMBuildGEP2(cg->builder, lem, lhs, &neg, 1, "gep.sub");
			}
			return LLVMBuildSub(cg->builder, lhs, rhs, "sub");
		}
		case '*':
			return LLVMBuildMul(cg->builder, lhs, rhs, "mul");
		case '/':
			return LLVMBuildSDiv(cg->builder, lhs, rhs, "div");
		case OP_EQ:
			return LLVMBuildICmp(cg->builder, LLVMIntEQ, lhs, rhs, "eq");
		case OP_NE:
			return LLVMBuildICmp(cg->builder, LLVMIntNE, lhs, rhs, "ne");
		case OP_LT:
			return LLVMBuildICmp(cg->builder, LLVMIntSLT, lhs, rhs, "lt");
		case OP_GT:
			return LLVMBuildICmp(cg->builder, LLVMIntSGT, lhs, rhs, "gt");
		case OP_LE:
			return LLVMBuildICmp(cg->builder, LLVMIntSLE, lhs, rhs, "le");
		case OP_GE:
			return LLVMBuildICmp(cg->builder, LLVMIntSGE, lhs, rhs, "ge");
		case OP_AND:
			return LLVMBuildAnd(cg->builder, lhs, rhs, "and");
		case OP_OR:
			return LLVMBuildOr(cg->builder, lhs, rhs, "or");
		case OP_XOR:
			return LLVMBuildXor(cg->builder, lhs, rhs, "xor");
		case OP_SHL:
			return LLVMBuildShl(cg->builder, lhs, rhs, "shl");
		case OP_SHR:
			return LLVMBuildLShr(cg->builder, lhs, rhs, "shr");
		default:
			break;
		}
		break;
	}

	/* ---- unary expressions --------------------------------------- */
	case NOD_UNARY_EXPR: {
		s32 op = node->nod_data.nod_op.op_kind;
		if (op == OP_AND) return s_emit_addr(cg, node->nod_child);
		if (op == '*') {
			LLVMValueRef ptr = s_emit_node(cg, node->nod_child);
			if (!ptr) return NULL;
			return LLVMBuildLoad2(cg->builder, s_map_type(cg, (struct dpp_type *)node->nod_type), ptr,
			                      "ptrload");
		}
		if (op == '!') {
			LLVMValueRef cv = s_emit_node(cg, node->nod_child);
			if (!cv) return NULL;
			LLVMValueRef res =
				LLVMBuildICmp(cg->builder, LLVMIntEQ, cv, LLVMConstNull(LLVMTypeOf(cv)), "lnot");
			return s_cast(cg, res, dpp_type_new(cg->arena, TYPE_BOOL), (struct dpp_type *)node->nod_type);
		}
		if (op == OP_SUB) {
			LLVMValueRef v = s_emit_node(cg, node->nod_child);
			if (!v) return NULL;
			return LLVMBuildNeg(cg->builder, v, "neg");
		}
		if (op == OP_BITWISE_NOT) {
			LLVMValueRef v = s_emit_node(cg, node->nod_child);
			if (!v) return NULL;
			return LLVMBuildNot(cg->builder, v, "not");
		}
		if (op == OP_INC || op == OP_DEC) {
			LLVMValueRef ptr = s_emit_addr(cg, node->nod_child);
			printf("TRACE: Incrementing at ptr %p\n", (void*)ptr);
			if (!ptr) return NULL;
			LLVMValueRef val = LLVMBuildLoad2(cg->builder, s_map_type(cg, (struct dpp_type *)node->nod_child->nod_type), ptr, "load");
			LLVMValueRef one = LLVMConstInt(LLVMTypeOf(val), 1, false);
			LLVMValueRef res;
			if (op == OP_INC)
				res = LLVMBuildAdd(cg->builder, val, one, "inc");
			else
				res = LLVMBuildSub(cg->builder, val, one, "dec");
			printf("TRACE: Storing incremented value to %p\n", (void*)ptr);
			LLVMBuildStore(cg->builder, res, ptr);
			return node->nod_is_postfix ? val : res; // Return original for post, new for pre
		}
		break;
	}	case NOD_INDEX_EXPR: {
		LLVMValueRef base = s_emit_node(cg, node->nod_data.nod_op.op_lhs);
		LLVMValueRef idx  = s_emit_node(cg, node->nod_data.nod_op.op_rhs);
		if (!base || !idx) return NULL;
		struct dpp_type *base_ty = (struct dpp_type *)node->nod_data.nod_op.op_lhs->nod_type;
		if (base_ty && base_ty->ty_kind == TYPE_ARRAY) {
			LLVMTypeRef  arr_ty = s_map_type(cg, base_ty);
			LLVMValueRef idxs[] = {LLVMConstInt(LLVMInt32TypeInContext(cg->context), 0, false), idx};
			LLVMValueRef gep    = LLVMBuildInBoundsGEP2(cg->builder, arr_ty, base, idxs, 2, "idxaddr");
			return (node->nod_type && ((struct dpp_type *)node->nod_type)->ty_kind == TYPE_ARRAY)
			               ? gep
			               : LLVMBuildLoad2(cg->builder, s_map_type(cg, (struct dpp_type *)node->nod_type),
			                                gep, "idxload");
		} else {
			struct dpp_type *res_ty  = (struct dpp_type *)node->nod_type;
			LLVMTypeRef      elem_ty = s_map_type(cg, res_ty);
			LLVMValueRef     gep = LLVMBuildInBoundsGEP2(cg->builder, elem_ty, base, &idx, 1, "ptraddr");
			return (node->nod_type && ((struct dpp_type *)node->nod_type)->ty_kind == TYPE_ARRAY)
			               ? gep
			               : LLVMBuildLoad2(cg->builder, elem_ty, gep, "ptrload");
		}
	}

	/* ---- cast expression ----------------------------------------- */
	case NOD_CAST_EXPR: {
		LLVMValueRef val = s_emit_node(cg, node->nod_child);
		if (!val) return NULL;
		return s_cast(cg, val, (struct dpp_type *)node->nod_child->nod_type, (struct dpp_type *)node->nod_type);
	}

	/* ---- expression / compound statement wrapper ----------------- */
	case NOD_EXPR_STMT:
		return s_emit_node(cg, node->nod_child);
	case NOD_COMPOUND_STMT: {
		struct dpp_node *curr = node->nod_child;
		LLVMValueRef     last = NULL;
		while (curr) {
			last = s_emit_node(cg, curr);
			curr = curr->nod_next;
		}
		return last;
	}
	default:
		break;
	}
	return NULL;
}

/* ------------------------------------------------------------------ */
/*  Public API                                                         */
/* ------------------------------------------------------------------ */
void dpp_codegen_init(struct dpp_codegen *cg, const char *module_name, struct dpp_symtab *tab, struct dpp_arena *arena)
{
	cg->context      = LLVMContextCreate();
	cg->module       = LLVMModuleCreateWithNameInContext(module_name, cg->context);
	cg->builder      = LLVMCreateBuilderInContext(cg->context);
	cg->symtab       = tab;
	cg->arena        = arena;
	cg->current_func = NULL;
}

void dpp_codegen_free(struct dpp_codegen *cg)
{
	LLVMDisposeBuilder(cg->builder);
	LLVMDisposeModule(cg->module);
	LLVMContextDispose(cg->context);
}

void dpp_codegen_emit(struct dpp_codegen *cg, struct dpp_node *root, const char *out_file)
{
	struct dpp_node *curr = root->nod_child;
	while (curr) {
		s_emit_node(cg, curr);
		curr = curr->nod_next;
	}
	if (out_file) {
		char ir_file[1024];
		snprintf(ir_file, sizeof(ir_file), "%s.ll", out_file);
		LLVMPrintModuleToFile(cg->module, ir_file, NULL);
		LLVMInitializeX86TargetInfo();
		LLVMInitializeX86Target();
		LLVMInitializeX86TargetMC();
		LLVMInitializeX86AsmParser();
		LLVMInitializeX86AsmPrinter();

		const char *triple = LLVMGetDefaultTargetTriple();
		LLVMSetTarget(cg->module, triple);

		LLVMTargetRef target;
		char         *err;
		if (!LLVMGetTargetFromTriple(triple, &target, &err)) {
			LLVMTargetMachineRef machine =
				LLVMCreateTargetMachine(target, triple, "generic", "", LLVMCodeGenLevelDefault,
			                                LLVMRelocPIC, LLVMCodeModelDefault);
			LLVMTargetMachineEmitToFile(machine, cg->module, (char *)out_file, LLVMObjectFile, &err);
			LLVMDisposeTargetMachine(machine);
		}
	}
}
