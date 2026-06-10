#include "core/diagnostic/diag.h"
#include "core/logger/logger.h"
#include <stdarg.h>
#include <stdio.h>

static char g_source_filename[1024];
static u32  s_error_count = 0;

void dpp_diag_set_source(const char *filename)
{
	snprintf(g_source_filename, sizeof(g_source_filename), "%s", filename);
}

void dpp_diag_error(struct dpp_node *node, const char *fmt, ...)
{
	s_error_count++;
	clog_source src = {.file = g_source_filename, .line = (int)node->nod_line, .col = (int)node->nod_column};

	va_list args;
	va_start(args, fmt);
	char msg_buf[1024];
	vsnprintf(msg_buf, sizeof(msg_buf), fmt, args);
	va_end(args);

	clog_error(src, "%s", msg_buf);
}

u32 dpp_diag_get_error_count(void)
{
	return s_error_count;
}
