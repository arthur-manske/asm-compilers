#include "core/logger/logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Estado Interno */
static FILE           *log_file         = NULL;
static enum clog_level global_min_level = CLOG_LEVEL_NOTE;
static uint32_t        global_flags     = 0;

/* Códigos de Cores (Estilo GCC/Clang) */
#define COLOR_RESET   "\033[0m"
#define COLOR_BOLD    "\033[1m"
#define COLOR_DIM     "\033[2m"
#define COLOR_RED     "\033[31m" /* Erro */
#define COLOR_MAGENTA "\033[35m" /* Aviso */
#define COLOR_CYAN    "\033[36m" /* Info/Note */
#define COLOR_GREEN   "\033[32m" /* Números (Linha/Coluna) */

/* Helpers */
static int should_use_color(void)
{
	if (global_flags & CLOG_COLOR_FORCE) return 1;
	if (global_flags & CLOG_COLOR_IGN) return 0;
	return isatty(STDERR_FILENO);
}

static const char *level_to_string(enum clog_level level)
{
	switch (level) {
	case CLOG_LEVEL_NOTE:
		return "note";
	case CLOG_LEVEL_WARN:
		return "warning";
	case CLOG_LEVEL_ERR:
		return "error";
	case CLOG_LEVEL_FATAL:
		return "fatal error";
	case CLOG_LEVEL_ICE:
		return "internal compiler error";
	default:
		return "unknown";
	}
}

static const char *level_to_color(enum clog_level level)
{
	switch (level) {
	case CLOG_LEVEL_NOTE:
		return COLOR_CYAN;
	case CLOG_LEVEL_WARN:
		return COLOR_MAGENTA;
	case CLOG_LEVEL_ERR:
		return COLOR_RED;
	case CLOG_LEVEL_FATAL:
		return COLOR_RED;
	case CLOG_LEVEL_ICE:
		return COLOR_RED;
	default:
		return COLOR_RESET;
	}
}

/*
 * Lê uma linha específica de um arquivo para exibir o contexto.
 * Nota: Para compiladores profissionais, isso é feito via mapeamento de memória
 * ou buffer de fonte pré-carregado. Aqui usamos fopen/fseek para simplicidade.
 */
static void print_source_context(const char *filepath, int error_line, int error_col)
{
	FILE *f = fopen(filepath, "r");
	if (!f) {
		return;
	}

	char line[4096];
	int  current_line = 0;

	/* Busca a linha */
	while (fgets(line, sizeof(line), f)) {
		current_line++;
		if (current_line == error_line) {
			/* Remove newline se existir */
			line[strcspn(line, "\r\n")] = 0;

			fprintf(stderr, "CTXLINE file=%s line=%d col=%d: '%s'\n", filepath, error_line, error_col, line);

			/* Imprime a linha */
			fprintf(stderr, " %s | %s\n", (should_use_color() ? COLOR_GREEN : ""), line);

			/* Imprime o caret (^^^) apontando para a coluna */
			if (error_col > 0) {
				fprintf(stderr, " %s | ", (should_use_color() ? COLOR_GREEN : ""));

				/* Imprime espaços até a coluna do erro */
				for (int i = 1; i < error_col; i++) {
					/* Tratamento simples para tabulação (visualmente 1 tab = varios
					   espaços seria ideal, mas aqui assumimos 1 char = 1 col para
					   simplificar) */
					if (line[i - 1] == '\t')
						fputc('\t', stderr);
					else
						fputc(' ', stderr);
				}

				/* Imprime o marcador */
				fprintf(stderr, "%s^%s\n", (should_use_color() ? COLOR_BOLD COLOR_RED : ""),
				        COLOR_RESET);
			}
			break;
		}
	}
	fclose(f);
}

int clog_init(const char *log_filepath, enum clog_level min_level, uint32_t flags)
{
	global_min_level = min_level;
	global_flags     = flags;

	if (log_filepath) {
		log_file = fopen(log_filepath, "w");
		if (!log_file) return -1;
	}
	return 0;
}

int clog_term(void)
{
	if (log_file) {
		fclose(log_file);
		log_file = NULL;
	}
	return 0;
}

int clog_report(clog_source src, enum clog_level level, const char *code, const char *format, ...)
{
	if (level < global_min_level) return 0;

	int         use_color   = should_use_color();
	const char *level_str   = level_to_string(level);
	const char *level_color = level_to_color(level);

	char    msg_buf[1024];
	va_list args;
	va_start(args, format);
	vsnprintf(msg_buf, sizeof(msg_buf), format, args);
	va_end(args);

	/* Construção da saída estilo GCC: file:line:col: error: message */
	/* Ex: main.c:10:5: error: expected ';' before 'return' */

	FILE *out = stderr;

	/* 1. Localização e Severidade */
	if (use_color) fprintf(out, "%s", COLOR_BOLD); /* Nome do arquivo em negrito */

	if (src.file) {
		fprintf(out, "%s", src.file);
		if (src.line > 0) {
			fprintf(out, ":%d", src.line);
			if (src.col > 0) fprintf(out, ":%d", src.col);
		}
		fprintf(out, ": ");
	} else {
		fprintf(out, "<unknown>: ");
	}

	if (use_color)
		fprintf(out, "%s%s%s: ", level_color, COLOR_BOLD, level_str);
	else
		fprintf(out, "%s: ", level_str);

	if (use_color) fprintf(out, "%s", COLOR_RESET); /* Reseta negrito/cor nível */

	/* 2. Código do erro (opcional, ex: E001) */
	if (code) {
		if (use_color)
			fprintf(out, "[%s] ", code);
		else
			fprintf(out, "[%s] ", code);
	}

	/* 3. Mensagem */
	fprintf(out, "%s\n", msg_buf);

	/* 4. Contexto do Código (Source Snippet) */
	if ((global_flags & CLOG_SOURCE_SHOW) && src.file && src.line > 0) {
		print_source_context(src.file, src.line, src.col);
	}

	/* 5. Escreve no arquivo de log (sem cores/ansi) */
	if (log_file) {
		fprintf(log_file, "%s", src.file ? src.file : "<unknown>");
		if (src.line > 0) fprintf(log_file, ":%d", src.line);
		if (src.col > 0) fprintf(log_file, ":%d", src.col);
		fprintf(log_file, ": %s: %s", level_str, msg_buf);
		if (code) fprintf(log_file, " [%s]", code);
		fprintf(log_file, "\n");
	}

	return 0;
}
