#include "core/ast/ast.h"
#include "core/arena/arena.h"
#include <string.h>

struct dpp_node *dpp_node_new(struct dpp_arena *ar, enum dpp_node_kind kind, u32 line, u32 column)
{
	struct dpp_node *node = dpp_arena_alloc(ar, sizeof(struct dpp_node));
	if (!node) return NULL;
	memset(node, 0, sizeof(*node));
	node->nod_kind   = kind;
	node->nod_line   = line;
	node->nod_column = column;
	return node;
}

const char *dpp_node_kind_to_str(enum dpp_node_kind kind)
{
	switch (kind) {
	case NOD_INVALID:
		return "Invalid";
	case NOD_TRANSLATION_UNIT:
		return "TranslationUnit";
	case NOD_FUNCTION_DECL:
		return "FunctionDecl";
	case NOD_VAR_DECL:
		return "VarDecl";
	case NOD_TYPEDEF:
		return "Typedef";
	case NOD_PARAM_DECL:
		return "ParamDecl";
	case NOD_STRUCT_DECL:
		return "StructDecl";
	case NOD_UNION_DECL:
		return "UnionDecl";
	case NOD_ENUM_DECL:
		return "EnumDecl";
	case NOD_ENUM_CONST:
		return "EnumConst";
	case NOD_COMPOUND_STMT:
		return "CompoundStmt";
	case NOD_RETURN_STMT:
		return "ReturnStmt";
	case NOD_IF_STMT:
		return "IfStmt";
	case NOD_WHILE_STMT:
		return "WhileStmt";
	case NOD_DO_WHILE_STMT:
		return "DoWhileStmt";
	case NOD_FOR_STMT:
		return "ForStmt";
	case NOD_BREAK_STMT:
		return "BreakStmt";
	case NOD_CONTINUE_STMT:
		return "ContinueStmt";
	case NOD_SWITCH_STMT:
		return "SwitchStmt";
	case NOD_CASE_STMT:
		return "CaseStmt";
	case NOD_DEFAULT_STMT:
		return "DefaultStmt";
	case NOD_EXPR_STMT:
		return "ExprStmt";
	case NOD_ASM_STMT:
		return "AsmStatement";
	case NOD_BINARY_EXPR:
		return "BinaryExpr";
	case NOD_TERNARY_EXPR:
		return "TernaryExpr";
	case NOD_UNARY_EXPR:
		return "UnaryExpr";
	case NOD_INDEX_EXPR:
		return "IndexExpr";
	case NOD_CAST_EXPR:
		return "CastExpr";
	case NOD_STMT_EXPR:
		return "StmtExpr";
	case NOD_INT_LITERAL:
		return "IntLiteral";
	case NOD_FLOAT_LITERAL:
		return "FloatLiteral";
	case NOD_STRING_LITERAL:
		return "StringLiteral";
	case NOD_IDENTIFIER:
		return "Identifier";
	case NOD_ATTR_ALWAYS:
		return "AlwaysModifier";
	case NOD_ATTR_ERANGE:
		return "ERangeAttribute";
	case NOD_ATTR_EVAL:
		return "EvalAttribute";
	case NOD_ATTR_CREATES:
		return "CreatesAttribute";
	case NOD_ATTR_DESTROYS:
		return "DestroysAttribute";
	case NOD_ATTR_INITIALIZED:
		return "InitializedAttribute";
	case NOD_ATTR_INITIALIZER:
		return "InitializerAttribute";
	case NOD_ATTR_DEINITIALIZER:
		return "DeinitializerAttribute";
	case NOD_ATTR_POSSIBLE_DEINITIALIZER:
		return "PossibleDeinitializerAttribute";
	case NOD_IMPLICIT_RET:
		return "ImplicitReturn";
	case NOD_TYPEOF:
		return "Typeof";
	default:
		return "Unknown";
	}
}

void dpp_node_add_child(struct dpp_node *parent, struct dpp_node *child)
{
	struct dpp_node *curr = parent->nod_child;
	while (curr) {
		if (curr == child) return;
		curr = curr->nod_next;
	}
	if (!parent->nod_child) {
		parent->nod_child = child;
	} else {
		curr = parent->nod_child;
		while (curr->nod_next) curr = curr->nod_next;
		curr->nod_next = child;
	}
}

const char *dpp_op_kind_to_str(enum dpp_op_kind kind)
{
    switch (kind) {
    case OP_ADD: return "+";
    case OP_SUB: return "-";
    case OP_MUL: return "*";
    case OP_DIV: return "/";
    case OP_MOD: return "%";
    case OP_AND: return "&";
    case OP_OR:  return "|";
    case OP_XOR: return "^";
    case OP_SHL: return "<<";
    case OP_SHR: return ">>";
    case OP_EQ:  return "==";
    case OP_NE:  return "!=";
    case OP_LT:  return "<";
    case OP_LE:  return "<=";
    case OP_GT:  return ">";
    case OP_GE:  return ">=";
    case OP_ASSIGN: return "=";
    case OP_DOT: return ".";
    case OP_ARROW: return "->";
    case OP_LOGICAL_AND: return "&&";
    case OP_LOGICAL_OR: return "||";
    case OP_INC: return "++";
    case OP_DEC: return "--";
    case OP_CALL: return "call";
    default: return "???";
    }
}
