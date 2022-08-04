/*
 * lexer.h
 *
 * This file is part of awl
 */

#pragma once

#include <stddef.h>
#include "file.h"
#include "span.h"

typedef enum token_kind {
	_TOKEN_NULL,

	TOKEN_EOF,

	TOKEN_IDENTIFIER,
	TOKEN_NUMLIT_INT,
	TOKEN_NUMLIT_FLT,

	TOKEN_RETURN,

	TOKEN_ARROW,
	TOKEN_LPAREN,
	TOKEN_RPAREN,
	TOKEN_LBRACE,
	TOKEN_RBRACE,
	TOKEN_SEMICOLON,
	TOKEN_COMMA,
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

	Token *vtokens;
	size_t ntokens;
} Lexer;

Lexer *lexer_new();
Token *lexer_run(Lexer *lexer, File *file);
void lexer_reset(Lexer *lexer);
