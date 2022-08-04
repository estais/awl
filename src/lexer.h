/*
 * lexer.h
 *
 * This file is part of awl
 */

#pragma once

#include <stddef.h>
#include "file.h"
#include "span.h"

#define EMPTYTOKEN (Token){ .kind = _TOKEN_NULL, .content = NULL, .span = (Span){0} }

typedef enum token_kind {
	_TOKEN_NULL = 0,

	TOKEN_EOF = 0x01,

	TOKEN_IDENTIFIER = 0x02,
	TOKEN_NUMLIT_INT = 0x04,
	TOKEN_NUMLIT_FLT = 0x08,

	TOKEN_FUN = 0x10,
	TOKEN_RETURN = 0x20,

	TOKEN_ARROW = 0x40,
	TOKEN_LPAREN = 0x80,
	TOKEN_RPAREN = 0x100,
	TOKEN_LBRACE = 0x200,
	TOKEN_RBRACE = 0x300,
	TOKEN_SEMICOLON = 0x400,
	TOKEN_COMMA = 0x600,
} token_kind;

typedef struct Token {
	token_kind kind;
	const char *content;
	Span span;
} Token;

typedef struct Lexer {
	File *file;
	int linendx;
	int chndx;

	Token *tokens;
	size_t ntokens;
} Lexer;

Lexer *lexer_new();
Token *lexer_run(Lexer *lexer, File *file);
void lexer_reset(Lexer *lexer);
