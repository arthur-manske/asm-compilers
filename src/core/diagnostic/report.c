#include "core/ast/ast.h"
#include <stdio.h>

void dpp_report_layout(struct dpp_node *root)
{
	if (!root || root->nod_kind != NOD_TRANSLATION_UNIT) return;

	struct dpp_node *curr = root->nod_child;
	while (curr) {
		if (curr->nod_kind == NOD_STRUCT_DECL || curr->nod_kind == NOD_UNION_DECL) {
			fprintf(stderr, "\n--- Final Layout Report for '%.*s' (%s) ---\n",
			        (int)curr->nod_data.nod_id.id_len, curr->nod_data.nod_id.id_name,
			        (curr->nod_kind == NOD_STRUCT_DECL ? "struct" : "union"));

			struct dpp_node *field = curr->nod_child;
			while (field) {
				if (field->nod_kind == NOD_VAR_DECL) {
					fprintf(stderr, "  [Member] %.*s (Ptr: %u)\n",
					        (int)field->nod_data.nod_id.id_len, field->nod_data.nod_id.id_name,
					        field->nod_ptr_depth);
				}
				field = field->nod_next;
			}
			fprintf(stderr, "------------------------------------------\n");
		}
		curr = curr->nod_next;
	}
}
