#define _POSIX_C_SOURCE 200809L
#include "core/types.h"

#include "core/codegen/cg_llvm.h"
#include "c/lexer/lexer.h"
#include "c/parser/parser.h"
#include "c/preproc/preproc.h"
#include "c/sema/check.h"
#include "core/diagnostic/diag.h"
#include "c/sema/layout.h"
#include "core/diagnostic/report.h"
#include "core/sema/type.h"
#include "core/target/target.h"
#include "core/logger/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

extern void dpp_check_top_level(struct dpp_node *root);
extern void dpp_analyze_types(struct dpp_node *node, struct dpp_symtab *tab, struct dpp_arena *arena,
                              struct dpp_target *target);
extern void dpp_resolve_sizeof_recursive(struct dpp_node *node, struct dpp_target *target, struct dpp_arena *arena);
extern const char *dpp_node_kind_to_str(enum dpp_node_kind kind);
extern const char *dpp_op_kind_to_str(enum dpp_op_kind kind);

static void s_compute_layouts_recursive(struct dpp_node *node, struct dpp_target *target)
{
	if (!node) return;
	if (node->nod_kind == NOD_STRUCT_DECL || node->nod_kind == NOD_UNION_DECL) {
		dpp_layout_compute(node, target);
	}
	struct dpp_node *curr = node->nod_child;
	while (curr) {
		s_compute_layouts_recursive(curr, target);
		curr = curr->nod_next;
	}
}

static void print_ast(FILE *out, struct dpp_node *node, int indent)
{
	if (!node) return;
	for (int i = 0; i < indent; i++) fprintf(out, "  ");
	const char      *kind_str = dpp_node_kind_to_str(node->nod_kind);
	struct dpp_type *ty       = (struct dpp_type *)node->nod_type;
	fprintf(out, "Node: %s ", kind_str);
	if (node->nod_kind == NOD_IDENTIFIER || node->nod_kind == NOD_VAR_DECL || node->nod_kind == NOD_FUNCTION_DECL ||
	    node->nod_kind == NOD_PARAM_DECL || node->nod_kind == NOD_STRUCT_DECL) {
		fprintf(out, "['%.*s'] ", (int)node->nod_data.nod_id.id_len, node->nod_data.nod_id.id_name);
	}
	if (node->nod_kind == NOD_INT_LITERAL) fprintf(out, "[%ld] ", (long)node->nod_data.nod_val.val_int);
	if (node->nod_kind == NOD_FLOAT_LITERAL) fprintf(out, "[%f] ", node->nod_data.val_float);
	if (node->nod_kind == NOD_STRING_LITERAL)
		fprintf(out, "[\"%.*s\"] ", (int)node->nod_data.nod_id.id_len, node->nod_data.nod_id.id_name);
	if (node->nod_kind == NOD_BINARY_EXPR || node->nod_kind == NOD_UNARY_EXPR)
		fprintf(out, "(Op: %s) ", dpp_op_kind_to_str(node->nod_data.nod_op.op_kind));
	fprintf(out, "[Type: %s, TFlg: 0x%X], Flags: 0x%lX\n", (ty ? "known" : "unknown"), (ty ? (u32)ty->ty_flags : 0),
	        node->nod_attr_flags);
	struct dpp_node *curr = node->nod_child;
	while (curr) {
		print_ast(out, curr, indent + 1);
		curr = curr->nod_next;
	}
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "Usage: %s [options] <file.c>\n", argv[0]);
		return 1;
	}

	const char *input_file   = NULL;
	const char *output_file  = "a.out";
	bool        compile_only = false;
	char       *user_lib_paths[64];
	int         user_lib_path_count = 0;
	char       *user_libs[64];
	int         user_lib_count = 0;

	struct dpp_arena arena;
	dpp_arena_init(&arena, 1024 * 1024);

	clog_init(NULL, CLOG_LEVEL_NOTE, CLOG_SOURCE_SHOW);

	struct dpp_preproc pp;
	dpp_preproc_init(&pp, &arena);
	dpp_preproc_add_include_path(&pp, "src/i");
	dpp_preproc_add_include_path(&pp, "src");
	dpp_preproc_add_include_path(&pp, "h");
	dpp_preproc_add_include_path(&pp, "/usr/lib/llvm/21/lib/clang/21/include");
	dpp_preproc_add_include_path(&pp, "/usr/local/include");
	dpp_preproc_add_include_path(&pp, "/usr/include");

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-I") == 0 && i + 1 < argc)
			dpp_preproc_add_include_path(&pp, argv[++i]);
		else if (strncmp(argv[i], "-I", 2) == 0)
			dpp_preproc_add_include_path(&pp, &argv[i][2]);
		else if (strcmp(argv[i], "-L") == 0 && i + 1 < argc)
			user_lib_paths[user_lib_path_count++] = argv[++i];
		else if (strncmp(argv[i], "-L", 2) == 0)
			user_lib_paths[user_lib_path_count++] = &argv[i][2];
		else if (strcmp(argv[i], "-l") == 0 && i + 1 < argc)
			user_libs[user_lib_count++] = argv[++i];
		else if (strncmp(argv[i], "-l", 2) == 0)
			user_libs[user_lib_count++] = &argv[i][2];
		else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc)
			output_file = argv[++i];
		else if (strcmp(argv[i], "-c") == 0)
			compile_only = true;
		else if (argv[i][0] == '-') {
			fprintf(stderr, "error: unknown option: %s\n", argv[i]);
			return 1;
		} else
			input_file = argv[i];
	}

	if (!input_file) {
		fprintf(stderr, "error: no input file\n");
		return 1;
	}

	FILE *f_in = fopen(input_file, "rb");
	if (!f_in) {
		perror("fopen");
		return 1;
	}
	fseek(f_in, 0, SEEK_END);
	size_t size = ftell(f_in);
	fseek(f_in, 0, SEEK_SET);
	u8 *data = (u8 *)malloc(size + 1);
	fread(data, 1, size, f_in);
	data[size] = 0;
	fclose(f_in);

	struct dpp_lexer lexer;
	dpp_lexer_init(&lexer, strdup(input_file), data, size);
	dpp_preproc_push_lexer(&pp, &lexer);
	struct dpp_parser parser;
	dpp_c_parser_init(&parser, &pp);

    printf("DEBUG: Starting parser...\n");
	struct dpp_node *root = dpp_parser_parse(&parser);
    printf("DEBUG: Parser finished. Errors: %u\n", dpp_diag_get_error_count());
    
    if (dpp_diag_get_error_count() > 0) {
        fprintf(stderr, "\nerror: aborted due to %u parsing errors\n", dpp_diag_get_error_count());
        return 1;
    }

	struct dpp_target target;
	if (dpp_target_load_yaml(&target, "abis/linux-glibc/x86_64/abi.yaml") != 0) {
		fprintf(stderr, "Error loading ABI config\n");
		return 1;
	}

	/* Phase 1: Resolve all types and names */
	dpp_check_top_level(root);
	dpp_analyze_types(root, &parser.par_symtab, &parser.par_arena, &target);

	/* Phase 2: Evaluate any constant array-size expressions that were parsed */
	dpp_eval_array_sizes(root);

	/* Phase 3: Compute layouts for all structs/unions in the entire tree */
	s_compute_layouts_recursive(root, &target);

	/* Phase 4: Finalize all sizeof expressions with solid layout data */
	dpp_resolve_sizeof_recursive(root, &target, &parser.par_arena);

	if (dpp_diag_get_error_count() > 0) {
		fprintf(stderr, "\nerror: aborted due to %u semantic errors\n", dpp_diag_get_error_count());
		return 1;
	}

	time_t     now    = time(NULL);
	struct tm *tm_now = localtime(&now);
	char       log_name[256];
	strftime(log_name, sizeof(log_name), "dpp_ast_%Y_%m_%d_%H_%M.log", tm_now);
	FILE *ast_log = fopen(log_name, "w");
	if (ast_log) {
		dpp_report_layout(root);
		print_ast(ast_log, root, 0);
		fclose(ast_log);
		printf("AST log saved to %s\n", log_name);
	}

	struct dpp_codegen cg;
	dpp_codegen_init(&cg, input_file, &parser.par_symtab, &parser.par_arena);
	char obj_file[1024];
	if (compile_only)
		strncpy(obj_file, output_file, sizeof(obj_file));
	else
		snprintf(obj_file, sizeof(obj_file), "%s.o", input_file);
	dpp_codegen_emit(&cg, root, obj_file);

	if (!compile_only) {
		char        cmd[8192];
		const char *linker = getenv("LD");
		if (!linker) linker = target.tar_linker_cmd;
		if (!linker) linker = "/usr/bin/ld";
		int pos = snprintf(cmd, sizeof(cmd), "%s", linker);
		if (target.tar_dynamic_linker)
			pos += snprintf(cmd + pos, sizeof(cmd) - pos, " -dynamic-linker %s", target.tar_dynamic_linker);
		for (u32 i = 0; i < target.tar_crt_count; i++)
			pos += snprintf(cmd + pos, sizeof(cmd) - pos, " %s", target.tar_crt_objs[i]);
		for (int i = 0; i < user_lib_path_count; i++)
			pos += snprintf(cmd + pos, sizeof(cmd) - pos, " -L%s", user_lib_paths[i]);
		for (u32 i = 0; i < target.tar_lib_path_count; i++)
			pos += snprintf(cmd + pos, sizeof(cmd) - pos, " -L%s", target.tar_lib_paths[i]);
		pos += snprintf(cmd + pos, sizeof(cmd) - pos, " %s", obj_file);
		for (int i = 0; i < user_lib_count; i++)
			pos += snprintf(cmd + pos, sizeof(cmd) - pos, " -l%s", user_libs[i]);
		for (u32 i = 0; i < target.tar_lib_count; i++)
			pos += snprintf(cmd + pos, sizeof(cmd) - pos, " -l%s", target.tar_libs[i]);
		for (u32 i = 0; i < target.tar_post_crt_count; i++)
			pos += snprintf(cmd + pos, sizeof(cmd) - pos, " %s", target.tar_post_crt_objs[i]);
		pos += snprintf(cmd + pos, sizeof(cmd) - pos, " -o %s", output_file);
		printf("Linking: %s\n", cmd);
		if (system(cmd) != 0) fprintf(stderr, "error: linking failed\n");
	}

	return 0;
}
