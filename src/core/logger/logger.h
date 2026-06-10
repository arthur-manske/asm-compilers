#ifndef COMPILER_LOGGER_H
#define COMPILER_LOGGER_H

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

/* Flags Globais */
#define CLOG_STDERR_IGN  (0x01) /** Não escreve no stderr (apenas arquivo se houver) */
#define CLOG_COLOR_FORCE (0x02) /** Força cores mesmo se não for TTY */
#define CLOG_COLOR_IGN   (0x04) /** Desabilita cores */
#define CLOG_SOURCE_SHOW (0x08) /** Mostra a linha de código com o marcador ^ (Caret) */

/* Níveis de Severidade (Mapeados para conceitos de compilador) */
enum clog_level {
	CLOG_LEVEL_NOTE,  /* Dicas informativas ("did you mean...?") */
	CLOG_LEVEL_WARN,  /* Avisos (Warnings) */
	CLOG_LEVEL_ERR,   /* Erros de compilação */
	CLOG_LEVEL_FATAL, /* Erros fatais (crash do compilador) */
	CLOG_LEVEL_ICE    /* Internal Compiler Error (Bug no compilador) */
};

/* Estrutura para localização do erro */
typedef struct {
	const char *file; /* Caminho do arquivo */
	int         line; /* Linha (1-based) */
	int         col;  /* Coluna (1-based, opcional, use 0 se desconhecido) */
} clog_source;

/* Inicialização e Terminação */
int clog_init(const char *log_filepath, enum clog_level min_level, uint32_t flags);
int clog_term(void);

/* Função Principal de Reporte */
int clog_report(clog_source src, enum clog_level level, const char *code, const char *format, ...);

/* Wrappers Convenientes (Sem código de erro explícito) */
#define clog_note(src, fmt, ...)  clog_report(src, CLOG_LEVEL_NOTE, NULL, fmt, ##__VA_ARGS__)
#define clog_warn(src, fmt, ...)  clog_report(src, CLOG_LEVEL_WARN, NULL, fmt, ##__VA_ARGS__)
#define clog_error(src, fmt, ...) clog_report(src, CLOG_LEVEL_ERR, NULL, fmt, ##__VA_ARGS__)

/* Wrapper com código de erro (ex: E001, W102) */
#define clog_error_code(src, code, fmt, ...) clog_report(src, CLOG_LEVEL_ERR, code, fmt, ##__VA_ARGS__)

#endif
