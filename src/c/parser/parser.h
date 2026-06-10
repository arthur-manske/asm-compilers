#ifndef DPP_PARSER_H
#define DPP_PARSER_H

#include "core/parser/parser.h"
#include "core/ast/ast.h"
#include "c/preproc/preproc.h"

struct dpp_node *dpp_parser_parse(struct dpp_parser *par);
struct dpp_node *dpp_parser_parse_expr(struct dpp_parser *par, int min_prec);
void dpp_c_parser_init(struct dpp_parser *par, struct dpp_preproc *pp);

#endif
