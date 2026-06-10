#ifndef DPP_DIAG_H
#define DPP_DIAG_H

#include "core/ast/ast.h"

void dpp_diag_set_source(const char *filename);
void dpp_diag_error(struct dpp_node *node, const char *fmt, ...);
u32  dpp_diag_get_error_count(void);

#endif
