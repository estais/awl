/*
 * lexer.c
 *
 * This file is part of awl
 */

#include "lexer.h"

#include <string.h>
#include <ctype.h>
#include "strbuf.h"
#include "vec.h"
#include "mem.h"
#include "err.h"

#define SB_INITALLOC_KWIDEN 50
#define SB_INITALLOC_NUMLIT 25

/* One-width span for lexer errors */
#define LEXERRSPAN (Span){ .linendx = lexer->linendx, .first = lexer->chndx, .last = lexer->chndx + 1 }

static void advancen(Lexer *lexer, size_t n);
static void advance(Lexer *lexer);
static char peek(Lexer *lexer, size_t n);
static char current(Lexer *lexer);
static void linelex(Lexer *lexer, size_t linendx);
static void lex_kwiden(Lexer *lexer);
static void lex_numlit(Lexer *lexer);
static void lex_op(Lexer *lexer);
static token_kind kindofkwiden(const char *str);

/*
 * Enumeration of contents for tokens whose content is static (for each token
 * of the token kind, the content is the same as all other instances; e.g. the
 * content of each TOKEN_RETURN token is "return").
 */
static const char *tktab[] = {
	[TOKEN_FUN] = "fun",
	[TOKEN_RETURN] = "return",

	[TOKEN_ARROW] = "->",
	[TOKEN_LPAREN] = "(",
	[TOKEN_RPAREN] = ")",
	[TOKEN_LBRACE] = "{",
	[TOKEN_RBRACE] = "}",
	[TOKEN_SEMICOLON] = ";",
	[TOKEN_COMMA] = ",",
};

Lexer *lexer_new()
{
	Lexer *lexer = alloct(Lexer);
	lexer->file = NULL;
	lexer->linendx = 0;
	lexer->chndx = 0;
	lexer->tokens = NULL;
	lexer->ntokens = 0;

	return lexer;
}

Token *lexer_run(Lexer *lexer, File *file)
{
	lexer->file = file;

	for (size_t i = 0; i < file->nlines; ++i) {
		linelex(lexer, i);
		lexer->chndx = 0; /* Reset cursor for each line */
	}

	/* Terminate with TOKEN_EOF */
	Token token = {
		.kind = TOKEN_EOF,
		.content = NULL,
		.span = { .linendx = 0, .first = 0, .last = 0 },
	};
	vec_push(lexer->tokens, &token, &lexer->ntokens, sizeof(Token));

	return lexer->tokens;
}

void lexer_reset(Lexer *lexer)
{
	lexer->file = NULL;
	lexer->linendx = 0;
	lexer->chndx = 0;
	lexer->tokens = NULL;
	lexer->ntokens = 0;
}

static void advancen(Lexer *lexer, size_t n)
{
	lexer->chndx += n;
}

static void advance(Lexer *lexer)
{
	advancen(lexer, 1);
}

static char peek(Lexer *lexer, size_t n)
{
	return lexer->file->vlines[lexer->linendx][lexer->chndx + n];
}

static char current(Lexer *lexer)
{
	return peek(lexer, 0);
}

static void linelex(Lexer *lexer, size_t linendx)
{
	lexer->linendx = linendx;

	char c = 0;
	while ((c = current(lexer))) {
		if (isspace(c)) {
			advance(lexer);
			continue;

		} else if (isalpha(c)) {
			/* Keyword or identifier */
			lex_kwiden(lexer);

		} else if (isdigit(c)) {
			/* Numeric literal */
			lex_numlit(lexer);

		} else {
			/* Operator */
			lex_op(lexer);
		}
	}
}

static void lex_kwiden(Lexer *lexer)
{
	StrBuf *sb = strbuf_new(SB_INITALLOC_KWIDEN);
	size_t first = lexer->chndx;

	char c = 0;
	while ((c = current(lexer)) && isalnum(c)) {
		strbuf_putc(sb, c);
		advance(lexer);
	}

	size_t length = strbuf_length(sb);
	const char *str = strbuf_release(sb);

	Span span = {
		.linendx = lexer->linendx,
		.first = first,
		.last = first + length,
	};

	Token token = {
		.kind = kindofkwiden(str),
		.content = str,
		.span = span,
	};

	vec_push(lexer->tokens, &token, &lexer->ntokens, sizeof(Token));
}

static void lex_numlit(Lexer *lexer)
{
	StrBuf *sb = strbuf_new(SB_INITALLOC_NUMLIT);
	token_kind kind = TOKEN_NUMLIT_INT;
	size_t first = lexer->chndx;

	char c = 0;
	while ((c = current(lexer)) && (isdigit(c) || c == '.')) {
		if (c == '.') {
			if (kind == TOKEN_NUMLIT_FLT) {
				err_source(lexer->file, LEXERRSPAN, "this number is already a float");
			}

			kind = TOKEN_NUMLIT_FLT;
		}

		strbuf_putc(sb, c);
		advance(lexer);
	}

	size_t length = strbuf_length(sb);
	const char *str = strbuf_release(sb);

	Span span = {
		.linendx = lexer->linendx,
		.first = first,
		.last = first + length,
	};

	Token token = {
		.kind = kind,
		.content = str,
		.span = span,
	};

	vec_push(lexer->tokens, &token, &lexer->ntokens, sizeof(Token));
}

static void lex_op(Lexer *lexer)
{
	token_kind kind = _TOKEN_NULL;
	size_t first = lexer->chndx;

	char c = current(lexer);
	if (c == '-') {
		if (peek(lexer, 1) == '>') {
			kind = TOKEN_ARROW;
		}
	} else if (c == '(') {
		kind = TOKEN_LPAREN;
	} else if (c == ')') {
		kind = TOKEN_RPAREN;
	} else if (c == '{') {
		kind = TOKEN_LBRACE;
	} else if (c == '}') {
		kind = TOKEN_RBRACE;
	} else if (c == ';') {
		kind = TOKEN_SEMICOLON;
	} else if (c == ',') {
		kind = TOKEN_COMMA;
	} else {
		err_source(lexer->file, LEXERRSPAN, "unexpected character '%c'", c);
	}

	const char *str = tktab[kind];
	size_t len = strlen(str);

	advancen(lexer, len);

	Span span = {
		.linendx = lexer->linendx,
		.first = first,
		.last = first + len,
	};

	Token token = {
		.kind = kind,
		.content = str,
		.span = span,
	};

	vec_push(lexer->tokens, &token, &lexer->ntokens, sizeof(Token));
}

static token_kind kindofkwiden(const char *str)
{
#define CMP(kind) if (!strcmp(str, tktab[kind])) return kind;
	CMP(TOKEN_FUN);
	CMP(TOKEN_RETURN);
#undef CMP
	return TOKEN_IDENTIFIER;
}
