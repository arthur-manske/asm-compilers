#include "core/types.h"
#include "c/lexer/lexer.h"
#include "c/lexer/token.h"
#include "core/logger/logger.h"
#include <stdio.h>
#include <string.h>

static void
s_update_pos(struct dpp_lexer *lex)
{
    lex->lex_column += (u32)(lex->lex_cursor - lex->lex_token);
}

s32
dpp_lexer_next(struct dpp_lexer *lex)
{
lex_next:
    lex->lex_token = lex->lex_cursor;

/* clang-format off */
    /*!re2c
        re2c:define:YYCTYPE = u8;
        re2c:define:YYCURSOR = lex->lex_cursor;
        re2c:define:YYMARKER = lex->lex_marker;
        re2c:define:YYLIMIT = lex->lex_limit;
        re2c:yyfill:enable = 0;

        /* Padrões auxiliares */
        hex_digit = [0-9a-fA-F];
        hex_prefix = "0" [xX];
        int_suffix = [uU] [lL]? [lL]? | [lL] [lL]? [uU]? | [uU]? [kK] | [dD];
        float_suffix = [fFlL] | [uU]? [kK] | [dD];
        exponent = [eE] [+-]? [0-9]+;
        hex_exponent = [pP] [+-]? [0-9]+;

        white = [ \t\v\f]+;
        newline = "\r"? "\n";
        line_continuation = "\\" "\r"? "\n";
        
        white { lex->lex_column += (u32)(lex->lex_cursor - lex->lex_token); goto lex_next; }
        line_continuation {
            lex->lex_line++;
            lex->lex_column = 1;
            lex->lex_is_bol = true;
            goto lex_next;
        }
        newline { 
            lex->lex_line++;
            lex->lex_line_ptr = lex->lex_cursor;
            lex->lex_column = 1;
            lex->lex_is_bol = true;
            goto lex_next;
        }

        "#" {
            if (lex->lex_is_bol) {
                s_update_pos(lex);
                lex->lex_is_bol = false;
                return TOK_PP_HASH;
            }
            s_update_pos(lex);
            lex->lex_is_bol = false;
            return '#';
        }

        "//" [^\r\n]* { goto lex_next; }
        "/*" {
            for (;;) {
                if (lex->lex_cursor >= lex->lex_limit) return TOK_EOF;
                if (lex->lex_cursor[0] == '*' && lex->lex_cursor[1] == '/') {
                    lex->lex_cursor += 2;
                    goto lex_next;
                }
                if (*lex->lex_cursor == '\n') {
                    lex->lex_line++;
                    lex->lex_line_ptr = lex->lex_cursor + 1;
                    lex->lex_column = 1;
                    lex->lex_is_bol = true;
                }
                lex->lex_cursor++;
            }
        }

        /* Keywords */
        "auto"      { s_update_pos(lex); lex->lex_is_bol = false; return TOK_AUTO; }
        "break"     { s_update_pos(lex); lex->lex_is_bol = false; return TOK_BREAK; }
        "case"      { s_update_pos(lex); lex->lex_is_bol = false; return TOK_CASE; }
        "char"      { s_update_pos(lex); lex->lex_is_bol = false; return TOK_CHAR; }
        "const"     { s_update_pos(lex); lex->lex_is_bol = false; return TOK_CONST; }
        "continue"  { s_update_pos(lex); lex->lex_is_bol = false; return TOK_CONTINUE; }
        "default"   { s_update_pos(lex); lex->lex_is_bol = false; return TOK_DEFAULT; }
        "do"        { s_update_pos(lex); lex->lex_is_bol = false; return TOK_DO; }
        "double"    { s_update_pos(lex); lex->lex_is_bol = false; return TOK_DOUBLE; }
        "else"      { s_update_pos(lex); lex->lex_is_bol = false; return TOK_ELSE; }
        "enum"      { s_update_pos(lex); lex->lex_is_bol = false; return TOK_ENUM; }
        "extern"    { s_update_pos(lex); lex->lex_is_bol = false; return TOK_EXTERN; }
        "float"     { s_update_pos(lex); lex->lex_is_bol = false; return TOK_FLOAT; }
        "for"       { s_update_pos(lex); lex->lex_is_bol = false; return TOK_FOR; }
        "goto"      { s_update_pos(lex); lex->lex_is_bol = false; return TOK_GOTO; }
        "if"        { s_update_pos(lex); lex->lex_is_bol = false; return TOK_IF; }
        "inline" | "__inline" | "__inline__" { s_update_pos(lex); lex->lex_is_bol = false; return TOK_INLINE; }
        "int"       { s_update_pos(lex); lex->lex_is_bol = false; return TOK_INT; }
        "long"      { s_update_pos(lex); lex->lex_is_bol = false; return TOK_LONG; }
        "register"  { s_update_pos(lex); lex->lex_is_bol = false; return TOK_REGISTER; }
        "restrict" | "__restrict" | "__restrict__" { s_update_pos(lex); lex->lex_is_bol = false; return TOK_RESTRICT; }
        "return"    { s_update_pos(lex); lex->lex_is_bol = false; return TOK_RETURN; }
        "short"     { s_update_pos(lex); lex->lex_is_bol = false; return TOK_SHORT; }
        "signed" | "__signed" | "__signed__" { s_update_pos(lex); lex->lex_is_bol = false; return TOK_SIGNED; }
        "sizeof"    { s_update_pos(lex); lex->lex_is_bol = false; return TOK_SIZEOF; }
        "static"    { s_update_pos(lex); lex->lex_is_bol = false; return TOK_STATIC; }
        "struct"    { s_update_pos(lex); lex->lex_is_bol = false; return TOK_STRUCT; }
        "switch"    { s_update_pos(lex); lex->lex_is_bol = false; return TOK_SWITCH; }
        "typedef"   { s_update_pos(lex); lex->lex_is_bol = false; return TOK_TYPEDEF; }
        "union"     { s_update_pos(lex); lex->lex_is_bol = false; return TOK_UNION; }
        "unsigned"  { s_update_pos(lex); lex->lex_is_bol = false; return TOK_UNSIGNED; }
        "void"      { s_update_pos(lex); lex->lex_is_bol = false; return TOK_VOID; }
        "volatile" | "__volatile" | "__volatile__" { s_update_pos(lex); lex->lex_is_bol = false; return TOK_VOLATILE; }
        "while"     { s_update_pos(lex); lex->lex_is_bol = false; return TOK_WHILE; }
        "true"      { s_update_pos(lex); lex->lex_is_bol = false; return TOK_TRUE; }
        "false"     { s_update_pos(lex); lex->lex_is_bol = false; return TOK_FALSE; }
        "_Bool"     { s_update_pos(lex); lex->lex_is_bol = false; return TOK_BOOL; }
        "_Complex"  { s_update_pos(lex); lex->lex_is_bol = false; return TOK_COMPLEX; }
        "_Imaginary" { s_update_pos(lex); lex->lex_is_bol = false; return TOK_IMAGINARY; }
        "asm" | "__asm" | "__asm__" { s_update_pos(lex); lex->lex_is_bol = false; return TOK_ASM; }
        "__typeof__" | "typeof" { s_update_pos(lex); lex->lex_is_bol = false; return TOK_TYPEOF; }
        "__attribute__" | "__attribute" { s_update_pos(lex); lex->lex_is_bol = false; return TOK_ATTRIBUTE; }
        "__builtin_va_list" { s_update_pos(lex); lex->lex_is_bol = false; return TOK_BUILTIN_VA_LIST; }
        "__extension__" { s_update_pos(lex); goto lex_next; }
        
        "_Accum"    { s_update_pos(lex); lex->lex_is_bol = false; return TOK_ACCUM; }
        "_UAccum"   { s_update_pos(lex); lex->lex_is_bol = false; return TOK_UACCUM; }
        "_Float16"  { s_update_pos(lex); lex->lex_is_bol = false; return TOK_FLOAT16; }
        "_Float32"  { s_update_pos(lex); lex->lex_is_bol = false; return TOK_FLOAT32; }
        "_Float64"  { s_update_pos(lex); lex->lex_is_bol = false; return TOK_FLOAT64; }
        "_Float128" { s_update_pos(lex); lex->lex_is_bol = false; return TOK_FLOAT128; }
        "_Float32x" { s_update_pos(lex); lex->lex_is_bol = false; return TOK_FLOAT32X; }
        "_Float64x" { s_update_pos(lex); lex->lex_is_bol = false; return TOK_FLOAT64X; }
        "_Float128x"{ s_update_pos(lex); lex->lex_is_bol = false; return TOK_FLOAT128X; }

        /* Operadores e Pontuação */
        "..." { s_update_pos(lex); lex->lex_is_bol = false; return TOK_ELLIPSIS; }
        ">>=" { s_update_pos(lex); lex->lex_is_bol = false; return TOK_RSHIFT_ASSIGN; }
        "<<=" { s_update_pos(lex); lex->lex_is_bol = false; return TOK_LSHIFT_ASSIGN; }
        "+="  { s_update_pos(lex); lex->lex_is_bol = false; return TOK_ADD_ASSIGN; }
        "-="  { s_update_pos(lex); lex->lex_is_bol = false; return TOK_SUB_ASSIGN; }
        "*="  { s_update_pos(lex); lex->lex_is_bol = false; return TOK_MUL_ASSIGN; }
        "/="  { s_update_pos(lex); lex->lex_is_bol = false; return TOK_DIV_ASSIGN; }
        "%="  { s_update_pos(lex); lex->lex_is_bol = false; return TOK_MOD_ASSIGN; }
        "&="  { s_update_pos(lex); lex->lex_is_bol = false; return TOK_AND_ASSIGN; }
        "^="  { s_update_pos(lex); lex->lex_is_bol = false; return TOK_XOR_ASSIGN; }
        "|="  { s_update_pos(lex); lex->lex_is_bol = false; return TOK_OR_ASSIGN; }
        ">>"  { s_update_pos(lex); lex->lex_is_bol = false; return TOK_RSHIFT; }
        "<<"  { s_update_pos(lex); lex->lex_is_bol = false; return TOK_LSHIFT; }
        "++"  { s_update_pos(lex); lex->lex_is_bol = false; return TOK_INC; }
        "--"  { s_update_pos(lex); lex->lex_is_bol = false; return TOK_DEC; }
        "->"  { s_update_pos(lex); lex->lex_is_bol = false; return TOK_ARROW; }
        "&&"  { s_update_pos(lex); lex->lex_is_bol = false; return TOK_AND; }
        "||"  { s_update_pos(lex); lex->lex_is_bol = false; return TOK_OR; }
        "<="  { s_update_pos(lex); lex->lex_is_bol = false; return TOK_LE; }
        ">="  { s_update_pos(lex); lex->lex_is_bol = false; return TOK_GE; }
        "=="  { s_update_pos(lex); lex->lex_is_bol = false; return TOK_EQ; }
        "!="  { s_update_pos(lex); lex->lex_is_bol = false; return TOK_NE; }
        ";"   { s_update_pos(lex); lex->lex_is_bol = false; return ';'; }
        "{"   { s_update_pos(lex); lex->lex_is_bol = false; return '{'; }
        "}"   { s_update_pos(lex); lex->lex_is_bol = false; return '}'; }
        ","   { s_update_pos(lex); lex->lex_is_bol = false; return ','; }
        ":"   { s_update_pos(lex); lex->lex_is_bol = false; return ':'; }
        "="   { s_update_pos(lex); lex->lex_is_bol = false; return '='; }
        "("   { s_update_pos(lex); lex->lex_is_bol = false; return '('; }
        ")"   { s_update_pos(lex); lex->lex_is_bol = false; return ')'; }
        "["   { s_update_pos(lex); lex->lex_is_bol = false; return '['; }
        "]"   { s_update_pos(lex); lex->lex_is_bol = false; return ']'; }
        "."   { s_update_pos(lex); lex->lex_is_bol = false; return TOK_DOT; }
        "&"   { s_update_pos(lex); lex->lex_is_bol = false; return TOK_AMP; }
        "!"   { s_update_pos(lex); lex->lex_is_bol = false; return TOK_BANG; }
        "~"   { s_update_pos(lex); lex->lex_is_bol = false; return TOK_TILDE; }
        "-"   { s_update_pos(lex); lex->lex_is_bol = false; return TOK_MINUS; }
        "+"   { s_update_pos(lex); lex->lex_is_bol = false; return TOK_PLUS; }
        "*"   { s_update_pos(lex); lex->lex_is_bol = false; return TOK_STAR; }
        "/"   { s_update_pos(lex); lex->lex_is_bol = false; return TOK_DIV; }
        "%"   { s_update_pos(lex); lex->lex_is_bol = false; return TOK_MOD; }
        "<"   { s_update_pos(lex); lex->lex_is_bol = false; return TOK_LT; }
        ">"   { s_update_pos(lex); lex->lex_is_bol = false; return TOK_GT; }
        "^"   { s_update_pos(lex); lex->lex_is_bol = false; return TOK_CARET; }
        "|"   { s_update_pos(lex); lex->lex_is_bol = false; return TOK_PIPE; }
        "?"   { s_update_pos(lex); lex->lex_is_bol = false; return TOK_QUERY; }
        "[["  { s_update_pos(lex); lex->lex_is_bol = false; return TOK_ATTR_OPEN; }
        "]]"  { s_update_pos(lex); lex->lex_is_bol = false; return TOK_ATTR_CLOSE; }

        /* Literais */
        [a-zA-Z_][a-zA-Z0-9_]* { s_update_pos(lex); lex->lex_is_bol = false; return TOK_IDENT; }
        
        ([0-9]+ | hex_prefix hex_digit+) int_suffix? |
        ([0-9]* "." [0-9]+ | [0-9]+ ".") exponent? float_suffix? |
        [0-9]+ exponent float_suffix? |
        hex_prefix (hex_digit* "." hex_digit+ | hex_digit+ ".") hex_exponent float_suffix? |
        hex_prefix hex_digit+ hex_exponent float_suffix?
        { 
            s_update_pos(lex); lex->lex_is_bol = false; return TOK_NUMBER; 
        }

        "\"" ([^"\\] | "\\" .)* "\"" { s_update_pos(lex); lex->lex_is_bol = false; return TOK_STRING; }
        "'" ([^'\\] | "\\" .)* "'" { s_update_pos(lex); lex->lex_is_bol = false; return TOK_CHAR_LITERAL; }

        $ { return TOK_EOF; }
        . { s_update_pos(lex); lex->lex_is_bol = false; return TOK_EOF; }
    */
}
