#include "core/types.h"
#include "core/ast/ast.h"
#include "c/lexer/token.h"

const char *dpp_token_kind_to_str(s32 kind)
{
	switch (kind) {
	case TOK_EOF:
		return "end of file";
	case TOK_ERROR:
		return "error";
	case TOK_PP_HASH:
		return "#";
	case ';':
		return ";";
	case '{':
		return "{";
	case '}':
		return "}";
	case '(':
		return "(";
	case ')':
		return ")";
	case '[':
		return "[";
	case ']':
		return "]";
	case ',':
		return ",";
	case '.':
		return ".";
	case '?':
		return "?";
	case ':':
		return ":";
	case '=':
		return "=";
	case '+':
		return "+";
	case '-':
		return "-";
	case '*':
		return "*";
	case '/':
		return "/";
	case '%':
		return "%";
	case '&':
		return "&";
	case '|':
		return "|";
	case '^':
		return "^";
	case '~':
		return "~";
	case '!':
		return "!";
	case '<':
		return "<";
	case '>':
		return ">";

	/* Keywords */
	case TOK_AUTO:
		return "auto";
	case TOK_BREAK:
		return "break";
	case TOK_CASE:
		return "case";
	case TOK_CHAR:
		return "char";
	case TOK_CONST:
		return "const";
	case TOK_CONTINUE:
		return "continue";
	case TOK_DEFAULT:
		return "default";
	case TOK_DO:
		return "do";
	case TOK_DOUBLE:
		return "double";
	case TOK_ELSE:
		return "else";
	case TOK_ENUM:
		return "enum";
	case TOK_EXTERN:
		return "extern";
	case TOK_FLOAT:
		return "float";
	case TOK_FLOAT16:
		return "_Float16";
	case TOK_FLOAT32:
		return "_Float32";
	case TOK_FLOAT64:
		return "_Float64";
	case TOK_FLOAT128:
		return "_Float128";
	case TOK_FLOAT32X:
		return "_Float32x";
	case TOK_FLOAT64X:
		return "_Float64x";
	case TOK_FLOAT128X:
		return "_Float128x";
	case TOK_FOR:
		return "for";
	case TOK_GOTO:
		return "goto";
	case TOK_IF:
		return "if";
	case TOK_INT:
		return "int";
	case TOK_LONG:
		return "long";
	case TOK_REGISTER:
		return "register";
	case TOK_RETURN:
		return "return";
	case TOK_SHORT:
		return "short";
	case TOK_SIGNED:
		return "signed";
	case TOK_SIZEOF:
		return "sizeof";
	case TOK_STATIC:
		return "static";
	case TOK_STRUCT:
		return "struct";
	case TOK_SWITCH:
		return "switch";
	case TOK_TYPEDEF:
		return "typedef";
	case TOK_UNION:
		return "union";
	case TOK_UNSIGNED:
		return "unsigned";
	case TOK_TYPEOF:
		return "typeof";
	case TOK_ASM:
		return "asm";
	case TOK_BUILTIN_VA_LIST:
		return "__builtin_va_list";
	case TOK_VOID:
		return "void";
	case TOK_VOLATILE:
		return "volatile";
	case TOK_WHILE:
		return "while";
	case TOK_INLINE:
		return "inline";
	case TOK_RESTRICT:
		return "restrict";
	case TOK_BOOL:
		return "bool";
	case TOK_TRUE:
		return "true";
	case TOK_FALSE:
		return "false";

	case TOK_COMPLEX:
		return "_Complex";
	case TOK_IMAGINARY:
		return "_Imaginary";
	case TOK_ATTRIBUTE:
		return "__attribute__";
	case TOK_ALIGNAS:
		return "_Alignas";
	case TOK_ALIGNOF:
		return "_Alignof";
	case TOK_GENERIC:
		return "_Generic";
	case TOK_ACCUM:
		return "_Accum";
	case TOK_UACCUM:
		return "_UAccum";
	case TOK_DECIMAL_TYPE:
		return "decimal_type";

	/* Literals */
	case TOK_IDENT:
		return "identifier";
	case TOK_NUMBER:
		return "number";
	case TOK_STRING:
		return "string literal";

	/* Operators */
	case TOK_ARROW:
		return "->";
	case TOK_INC:
		return "++";
	case TOK_DEC:
		return "--";
	case TOK_LSHIFT:
		return "<<";
	case TOK_RSHIFT:
		return ">>";
	case TOK_LE:
		return "<=";
	case TOK_GE:
		return ">=";
	case TOK_EQ:
		return "==";
	case TOK_NE:
		return "!=";
	case TOK_AND:
		return "&&";
	case TOK_OR:
		return "||";
	case TOK_MUL_ASSIGN:
		return "*=";
	case TOK_DIV_ASSIGN:
		return "/=";
	case TOK_MOD_ASSIGN:
		return "%=";
	case TOK_ADD_ASSIGN:
		return "+=";
	case TOK_SUB_ASSIGN:
		return "-=";
	case TOK_LSHIFT_ASSIGN:
		return "<<=";
	case TOK_RSHIFT_ASSIGN:
		return ">>=";
	case TOK_AND_ASSIGN:
		return "&=";
	case TOK_XOR_ASSIGN:
		return "^=";
	case TOK_OR_ASSIGN:
		return "|=";
	case TOK_ELLIPSIS:
		return "...";
	case TOK_ATTR_OPEN:
		return "[[";
	case TOK_ATTR_CLOSE:
		return "]]";

	default:
		return "unknown token";
	}
}

enum dpp_op_kind dpp_token_to_op(s32 kind)
{
    switch (kind) {
    case TOK_PLUS: return OP_ADD;
    case TOK_MINUS: return OP_SUB;
    case TOK_STAR: return OP_MUL;
    case TOK_DIV: return OP_DIV;
    case TOK_MOD: return OP_MOD;
    case TOK_AMP: return OP_AND;
    case TOK_PIPE: return OP_OR;
    case TOK_CARET: return OP_XOR;
    case TOK_TILDE: return OP_BITWISE_NOT;
    case TOK_LSHIFT: return OP_SHL;
    case TOK_RSHIFT: return OP_SHR;
    case TOK_EQ: return OP_EQ;
    case TOK_NE: return OP_NE;
    case TOK_LT: return OP_LT;
    case TOK_LE: return OP_LE;
    case TOK_GT: return OP_GT;
    case TOK_GE: return OP_GE;
    case TOK_ASSIGN: return OP_ASSIGN;
    case TOK_DOT: return OP_DOT;
    case TOK_ARROW: return OP_ARROW;
    case TOK_AND: return OP_LOGICAL_AND;
    case TOK_OR: return OP_LOGICAL_OR;
    case TOK_INC: return OP_INC;
    case TOK_DEC: return OP_DEC;
    default: return OP_INVALID;
    }
}

enum dpp_type_kind dpp_token_to_type(s32 kind)
{
    switch (kind) {
    case TOK_CHAR: return TYPE_CHAR;
    case TOK_INT: return TYPE_INT;
    case TOK_FLOAT: return TYPE_FLOAT;
    case TOK_FLOAT16: return TYPE_FLOAT16;
    case TOK_FLOAT32: return TYPE_FLOAT32;
    case TOK_FLOAT64: return TYPE_FLOAT64;
    case TOK_FLOAT128: return TYPE_FLOAT128;
    case TOK_FLOAT32X: return TYPE_FLOAT32X;
    case TOK_FLOAT64X: return TYPE_FLOAT64X;
    case TOK_FLOAT128X: return TYPE_FLOAT128X;
    case TOK_DOUBLE: return TYPE_DOUBLE;
    case TOK_BOOL: return TYPE_BOOL;
    case TOK_VOID: return TYPE_VOID;
    case TOK_STRUCT: return TYPE_STRUCT;
    case TOK_UNION: return TYPE_UNION;
    case TOK_ENUM: return TYPE_ENUM;
    case TOK_LONG: return TYPE_LONG;
    default: return TYPE_INVALID;
    }
}
