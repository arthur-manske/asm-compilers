#include "core/ast/ast_stream.h"
#include <stdlib.h>
#include <string.h>

struct dpp_ast_entry {
	enum dpp_node_kind kind;
	u32                line, col;
	s64                data;
	f64                data_float;
	const char        *name;
	u32                ptr_depth;
};

struct dpp_ast_stream {
	struct dpp_ast_entry *entries;
	size_t                count;
	size_t                cap;
};

void dpp_ast_stream_init(struct dpp_ast_stream *s)
{
	memset(s, 0, sizeof(*s));
	s->cap     = 1024;
	s->entries = (struct dpp_ast_entry *)malloc(sizeof(struct dpp_ast_entry) * s->cap);
}

static void s_push_node(struct dpp_ast_stream *s, struct dpp_node *n)
{
	if (!n) return;

	if (s->count >= s->cap) {
		s->cap *= 2;
		s->entries = (struct dpp_ast_entry *)realloc(s->entries, sizeof(struct dpp_ast_entry) * s->cap);
	}

	struct dpp_ast_entry *e = &s->entries[s->count++];
	e->kind                 = n->nod_kind;
	e->line                 = n->nod_line;
	e->col                  = n->nod_column;
	e->ptr_depth            = n->nod_ptr_depth;
	e->name                 = NULL;

	if (n->nod_kind == NOD_INT_LITERAL)
		e->data = n->nod_data.nod_val.val_int;
	else if (n->nod_kind == NOD_FLOAT_LITERAL)
		e->data_float = n->nod_data.val_float;
	else if (n->nod_kind == NOD_IDENTIFIER || n->nod_kind == NOD_VAR_DECL || n->nod_kind == NOD_FUNCTION_DECL) {
		e->name = (const char *)n->nod_data.nod_id.id_name;
	}

	struct dpp_node *curr = n->nod_child;
	while (curr) {
		s_push_node(s, curr);
		curr = curr->nod_next;
	}
}

void dpp_ast_stream_write(struct dpp_ast_stream *s, struct dpp_node *root)
{
	s_push_node(s, root);
}

void dpp_ast_stream_free(struct dpp_ast_stream *s)
{
	free(s->entries);
}
