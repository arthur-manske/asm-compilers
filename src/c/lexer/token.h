#ifndef DPP_TOKEN_H
#define DPP_TOKEN_H

#include "core/types.h"

enum dpp_token_kind {
	TOK_EOF     = 0,
	TOK_ERROR   = -1,
	TOK_PP_HASH = -2, /* # no início da linha */

	/* Keywords C89/C90 */
	TOK_AUTO = 256,
	TOK_BREAK,
	TOK_CASE,
	TOK_CHAR,
	TOK_CONST,
	TOK_CONTINUE,
	TOK_DEFAULT,
	TOK_DO,
	TOK_DOUBLE,
	TOK_ELSE,
	TOK_ENUM,
	TOK_EXTERN,
	TOK_FLOAT,
	TOK_FLOAT16,
	TOK_FLOAT32,
	TOK_FLOAT64,
	TOK_FLOAT128,
	TOK_FLOAT32X,
	TOK_FLOAT64X,
	TOK_FLOAT128X,
	TOK_FOR,
	TOK_GOTO,
	TOK_IF,
	TOK_INT,
	TOK_LONG,
	TOK_REGISTER,
	TOK_RETURN,
	TOK_SHORT,
	TOK_SIGNED,
	TOK_SIZEOF,
	TOK_STATIC,
	TOK_STRUCT,
	TOK_SWITCH,
	TOK_TYPEDEF,
	TOK_UNION,
	TOK_UNSIGNED,
	TOK_VOID,
	TOK_VOLATILE,
	TOK_WHILE,

	/* Keywords C99/C23 */
	TOK_INLINE,
	TOK_RESTRICT,
	TOK_BOOL,  /* _Bool */
	TOK_TRUE,  /* true */
	TOK_FALSE, /* false */
	TOK_BUILTIN_VA_LIST,
	TOK_COMPLEX,      /* _Complex */
	TOK_IMAGINARY,    /* _Imaginary */
	TOK_ATTRIBUTE,    /* __attribute__ */
	TOK_ALIGNAS,      /* _Alignas / alignas */
	TOK_ALIGNOF,      /* _Alignof / alignof */
	TOK_GENERIC,      /* _Generic */
	TOK_ACCUM,        /* _Accum */
	TOK_UACCUM,       /* _UAccum */
	TOK_DECIMAL_TYPE, /* decimal32, etc */
	TOK_ASM,          /* asm / __asm__ */
	TOK_TYPEOF,

	/* Literals */

	TOK_IDENT,
	TOK_NUMBER,
	TOK_STRING,
	TOK_CHAR_LITERAL,

	/* Operators */
	TOK_SEMICOLON = ';',
	TOK_LBRACE    = '{',
	TOK_RBRACE    = '}',
	TOK_LPAREN    = '(',
	TOK_RPAREN    = ')',
	TOK_LBRACKET  = '[',
	TOK_RBRACKET  = ']',
	TOK_COMMA     = ',',
	TOK_DOT       = '.',
	TOK_AMP       = '&',
	TOK_STAR      = '*',
	TOK_PLUS      = '+',
	TOK_MINUS     = '-',
	TOK_TILDE     = '~',
	TOK_BANG      = '!',
	TOK_DIV       = '/',
	TOK_MOD       = '%',
	TOK_LT        = '<',
	TOK_GT        = '>',
	TOK_CARET     = '^',
	TOK_PIPE      = '|',
	TOK_QUERY     = '?',
	TOK_COLON     = ':',
	TOK_ASSIGN    = '=',
	TOK_HASH      = '#',

	TOK_ARROW = 1000,  /* -> */
	TOK_ATTR_OPEN,     /* [[ */
	TOK_ATTR_CLOSE,    /* ]] */
	TOK_INC,           /* ++ */
	TOK_DEC,           /* -- */
	TOK_LSHIFT,        /* << */
	TOK_RSHIFT,        /* >> */
	TOK_LE,            /* <= */
	TOK_GE,            /* >= */
	TOK_EQ,            /* == */
	TOK_NE,            /* != */
	TOK_AND,           /* && */
	TOK_OR,            /* || */
	TOK_ADD_ASSIGN,    /* += */
	TOK_SUB_ASSIGN,    /* -= */
	TOK_MUL_ASSIGN,    /* *= */
	TOK_DIV_ASSIGN,    /* /= */
	TOK_MOD_ASSIGN,    /* %= */
	TOK_LSHIFT_ASSIGN, /* <<= */
	TOK_RSHIFT_ASSIGN, /* >>= */
	TOK_AND_ASSIGN,    /* &= */
	TOK_XOR_ASSIGN,    /* ^= */
	TOK_OR_ASSIGN,     /* |= */
	TOK_ELLIPSIS       /* ... */
};

const char *dpp_token_kind_to_str(s32 kind);
enum dpp_op_kind dpp_token_to_op(s32 kind);
enum dpp_type_kind dpp_token_to_type(s32 kind);

#endif
