#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "lexer/lexer.h"
#include "c/parser/declarator.h"
#include "c/parser/parser.h"
#include "preproc/preproc.h"
#include "sema/type.h"
#include "core/arena.h"

static void setup_parser(struct dpp_parser *par, struct dpp_preproc *pp, struct dpp_lexer *lex, struct dpp_arena *arena,
                         const char *src)
{
	dpp_arena_init(arena, 64 * 1024);

	dpp_preproc_init(pp, arena);

	dpp_lexer_init(lex, "<test>", (const u8 *)src, strlen(src));

	dpp_preproc_push_lexer(pp, lex);

	dpp_parser_init(par, pp);
    
    // In dpp_parser_init, it already consumes the first token.
    // If we need to reset/ensure it starts at the beginning of our src:
    // The lexer is pushed, so next_token should work.
}

static void expect_kind(struct dpp_type *ty, enum dpp_type_kind kind)
{
	assert(ty != NULL);
	assert(ty->ty_kind == kind);
}

static void test_void_ptr(void)
{
	struct dpp_arena   arena;
	struct dpp_preproc pp;
	struct dpp_lexer   lex;
	struct dpp_parser  par;

	setup_parser(&par, &pp, &lex, &arena, "*ptr");

	struct dpp_type *base = dpp_type_new(&par.par_arena, TYPE_VOID);

	struct declarator_result res = parse_declarator_full(&par, base);

	assert(res.name != NULL);
	assert(res.name_len == 3);
	assert(memcmp(res.name, "ptr", 3) == 0);

	expect_kind(res.type, TYPE_PTR);
	expect_kind(res.type->ty_next, TYPE_VOID);

	dpp_parser_free(&par);
	dpp_arena_free(&arena);

	puts("[OK] void *ptr");
}

static void test_array_of_function_pointers(void)
{
	struct dpp_arena   arena;
	struct dpp_preproc pp;
	struct dpp_lexer   lex;
	struct dpp_parser  par;

	setup_parser(&par, &pp, &lex, &arena, "(*ptr[10])(void)");

	struct dpp_type *base = dpp_type_new(&par.par_arena, TYPE_VOID);

	struct declarator_result res = parse_declarator_full(&par, base);

	assert(res.name != NULL);
	assert(res.name_len == 3);
	assert(memcmp(res.name, "ptr", 3) == 0);

	expect_kind(res.type, TYPE_ARRAY);
	expect_kind(res.type->ty_next, TYPE_PTR);
	expect_kind(res.type->ty_next->ty_next, TYPE_FUNCTION);
	expect_kind(res.type->ty_next->ty_next->ty_next, TYPE_VOID);

	dpp_parser_free(&par);
	dpp_arena_free(&arena);

	puts("[OK] void (*ptr[10])(void)");
}

static void test_complex_nested_declarator(void)
{
	struct dpp_arena   arena;
	struct dpp_preproc pp;
	struct dpp_lexer   lex;
	struct dpp_parser  par;

	setup_parser(&par, &pp, &lex, &arena, "(**ptr[256])(void (*array[10])(void))");

	struct dpp_type *base = dpp_type_new(&par.par_arena, TYPE_VOID);

	struct declarator_result res = parse_declarator_full(&par, base);

	assert(res.name != NULL);
	assert(res.name_len == 3);
	assert(memcmp(res.name, "ptr", 3) == 0);

	expect_kind(res.type, TYPE_ARRAY);
	assert(res.type->ty_data.ty_array.size == 256);

	expect_kind(res.type->ty_next, TYPE_PTR);
	expect_kind(res.type->ty_next->ty_next, TYPE_PTR);

	expect_kind(res.type->ty_next->ty_next->ty_next, TYPE_FUNCTION);

	expect_kind(res.type->ty_next->ty_next->ty_next->ty_next, TYPE_VOID);

	struct dpp_type *fn = res.type->ty_next->ty_next->ty_next;

	assert(fn->ty_data.ty_function.num_params == 1);

	struct dpp_type *param = fn->ty_data.ty_function.params[0];

	expect_kind(param, TYPE_ARRAY);
	assert(param->ty_data.ty_array.size == 10);

	expect_kind(param->ty_next, TYPE_PTR);
	expect_kind(param->ty_next->ty_next, TYPE_FUNCTION);

	dpp_parser_free(&par);
	dpp_arena_free(&arena);

	puts("[OK] void (**ptr[256])(void (*array[10])(void))");
}

int main(void)
{
	puts("Running declarator tests...");

	test_void_ptr();
	test_array_of_function_pointers();
	test_complex_nested_declarator();

	puts("All tests passed.");

	return 0;
}
