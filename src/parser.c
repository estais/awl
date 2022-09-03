/*
 * parser.c
 *
 * This file is part of awl
 */

#include "parser.h"

#include <stdlib.h>
#include <ctype.h>
#include "vec.h"
#include "mem.h"
#include "err.h"

static void advancen(Parser *parser, size_t n);
static void advance(Parser *parser);
static Token peek(Parser *parser, size_t n);
static Token current(Parser *parser);
static token_kind istk(Parser *parser, token_kind kind);

static PType *parse_type(Parser *parser);
static PVariable *parse_variable(Parser *parser);
static PExpression *parse_expression(Parser *parser);
static PStatement *parse_statement(Parser *parser);
static PBlock *parse_block(Parser *parser);
static PFun *parse_fun(Parser *parser);

static __int128 atoi128(const char *s)
{
	const char *p = s;
	__int128 v = 0;

	while (isspace(*p)) ++p;

	while (*p >= '0' && *p <= '9') {
		v = (10 * v) + (*p - '0');
		++p;
	}

	return v;
}

Number *number_make(Token from)
{
	Number *number = alloct(Number);
	number->span = from.span;

	if (from.kind == TOKEN_NUMLIT_FLT) {
		err_internal("floating point numbers are not yet implemented");
	}

	if (from.kind != TOKEN_NUMLIT_INT) {
		err_internal("cannot convert token of type %d into a numeric literal", (int)from.kind);
	}

	__int128 i = atoi128(from.content);

	if (i >= 0) {
		/* Unsigned */

		if (i <= UINT8_MAX) {
			number->bits = 8;
			number->sig = false;
			number->u8 = (uint8_t)i;
		} else if (i > UINT8_MAX && i <= UINT16_MAX) {
			number->bits = 16;
			number->sig = false;
			number->u16 = (uint16_t)i;
		} else if (i > UINT16_MAX && i <= UINT32_MAX) {
			number->bits = 32;
			number->sig = false;
			number->u32 = (uint32_t)i;
		} else if (i > UINT32_MAX && i <= UINT64_MAX) {
			number->bits = 64;
			number->sig = false;
			number->u64 = (uint64_t)i;
		}
	} else if (i < 0) {
		/* Signed */

		if (i <= INT8_MAX) {
			number->bits = 8;
			number->sig = true;
			number->s8 = (int8_t)i;
		} else if (i > INT8_MAX && i <= INT16_MAX) {
			number->bits = 16;
			number->sig = true;
			number->s16 = (int16_t)i;
		} else if (i > INT16_MAX && i <= INT32_MAX) {
			number->bits = 32;
			number->sig = true;
			number->s32 = (int32_t)i;
		} else if (i > INT32_MAX && i <= INT64_MAX) {
			number->bits = 64;
			number->sig = true;
			number->s64 = (int64_t)i;
		}
	}

	return number;
}

Parser *parser_new()
{
	Parser *parser = alloct(Parser);
	parser->file = NULL;
	parser->lexer = lexer_new(); /* The lexer should NOT be reset in parser_reset() */
	parser->tokens = NULL;
	parser->tkndx = 0;
	parser->pfile = NULL;

	return parser;
}

PFile *parser_run(Parser *parser, File *file)
{
	parser->file = file;

	/* Lex the file */
	parser->tokens = lexer_run(parser->lexer, file);

	parser->pfile = alloct(PFile);
	parser->pfile->pfuns = NULL;
	parser->pfile->npfuns = 0;


	while (current(parser).kind != TOKEN_EOF) {
		switch (istk(parser, TOKEN_FUN)) {
			case TOKEN_FUN: {
				PFun *pfun = parse_fun(parser);
				vec_push(parser->pfile->pfuns, &pfun, &parser->pfile->npfuns, sizeof(PFun *));

				break;
			}
			default: err_source(parser->file, current(parser).span, "unexpected token");
		}
	}

	lexer_reset(parser->lexer);

	return parser->pfile;
}

void parser_reset(Parser *parser)
{
	parser->file = NULL;
	parser->tkndx = 0;
	parser->pfile = NULL;
}

static void advancen(Parser *parser, size_t n)
{
	parser->tkndx += n;
}

static void advance(Parser *parser)
{
	advancen(parser, 1);
}

static Token peek(Parser *parser, size_t n)
{
	return parser->tokens[parser->tkndx + n];
}

static Token current(Parser *parser)
{
	return peek(parser, 0);
}

static token_kind istk(Parser *parser, token_kind kind)
{
	token_kind cursor = current(parser).kind;
	return (cursor & kind ? cursor : _TOKEN_NULL);
}

/* type = typename */
static PType *parse_type(Parser *parser)
{
	PType *ptype = alloct(PType);
	ptype->variant = _PNODE_NULL;
	ptype->name = EMPTYTOKEN;

	if (istk(parser, TOKEN_IDENTIFIER)) {
		ptype->variant = PTYPE_NAMED;
		ptype->name = current(parser);

		advance(parser); /* typename */
	} else {
		err_source(parser->file, current(parser).span, "expected typename");
	}

	return ptype;
}

/* variable = identifier type */
static PVariable *parse_variable(Parser *parser)
{
	PVariable *pvariable = alloct(PVariable);
	pvariable->identifier = EMPTYTOKEN;
	pvariable->type = NULL;

	if (!istk(parser, TOKEN_IDENTIFIER)) {
		err_source(parser->file, current(parser).span, "expected identifier");
	}

	pvariable->identifier = current(parser);
	advance(parser); /* identifier */

	pvariable->type = parse_type(parser);

	return pvariable;
}

/* expression = numeric-literal */
static PExpression *parse_expression(Parser *parser)
{
	PExpression *pexpression = alloct(PExpression);
	pexpression->variant = _PNODE_NULL;
	pexpression->number = NULL;

	switch (istk(parser, TOKEN_NUMLIT_INT | TOKEN_NUMLIT_FLT)) {
		case TOKEN_NUMLIT_INT:
		case TOKEN_NUMLIT_FLT: {
			pexpression->variant = PEXPRESSION_NUMLIT;
			pexpression->number = number_make(current(parser));

			advance(parser); /* numeric-literal */
			break;
		}
		default: err_source(parser->file, current(parser).span, "expected expression");
	}

	return pexpression;
}

/* statement = "return" expression ";" */
static PStatement *parse_statement(Parser *parser)
{
	PStatement *pstatement = alloct(PStatement);
	pstatement->variant = _PNODE_NULL;
	pstatement->expr = NULL;

	bool reqsemi = false;

	switch (istk(parser, TOKEN_RETURN)) {
		case TOKEN_RETURN: {
			pstatement->span = current(parser).span;
			advance(parser); /* return */

			if (istk(parser, TOKEN_SEMICOLON)) {
				pstatement->variant = PSTATEMENT_RETURN_NOVAL;
				reqsemi = true;
				break;
			}

			pstatement->variant = PSTATEMENT_RETURN;
			pstatement->expr = parse_expression(parser);

			reqsemi = true;
			break;
		}
		default: err_source(parser->file, current(parser).span, "expected statement");
	}

	if (reqsemi && !istk(parser, TOKEN_SEMICOLON)) {
		err_source(parser->file, current(parser).span, "preceeding statement unterminated");
	} else if (reqsemi && istk(parser, TOKEN_SEMICOLON)) {
		advance(parser); /* ; */
	}

	return pstatement;
}

/* block = "{" [{statement}] "}" */
static PBlock *parse_block(Parser *parser)
{
	PBlock *pblock = alloct(PBlock);
	pblock->statements = NULL;
	pblock->nstatements = 0;

	if (!istk(parser, TOKEN_LBRACE)) {
		err_source(parser->file, current(parser).span, "expected '{'");
	}

	advance(parser); /* { */

	while (!istk(parser, TOKEN_RBRACE)) {
		PStatement *statement = parse_statement(parser);

		vec_push(pblock->statements, &statement, &pblock->nstatements, sizeof(PStatement *));
	}

	advance(parser); /* } */

	return pblock;
}

/* fun = "fun" identifier "(" [{parameters}] ")" [type] block */
static PFun *parse_fun(Parser *parser)
{
	PFun *pfun = alloct(PFun);
	pfun->identifier = EMPTYTOKEN;
	pfun->params = NULL;
	pfun->nparams = 0;
	pfun->rettype = NULL;

	advance(parser); /* fun */

	if (!istk(parser, TOKEN_IDENTIFIER)) {
		err_source(parser->file, current(parser).span, "expected function identifier");
	}

	pfun->identifier = current(parser);
	advance(parser); /* identifier */

	if (!istk(parser, TOKEN_LPAREN)) {
		err_source(parser->file, current(parser).span, "expected '('");
	}

	advance(parser); /* ( */

	bool paramdone = false;
	token_kind paramcurs = _TOKEN_NULL;
	while ((paramcurs = current(parser).kind) != TOKEN_EOF) {
		if (paramcurs == TOKEN_RPAREN) {
					advance(parser); /* ) */
			break;

		} else if (paramcurs == TOKEN_IDENTIFIER) {
			PVariable *param = parse_variable(parser);
			vec_push(pfun->params, &param, &pfun->nparams, sizeof(PVariable *));

			paramdone = true;

		} else if (paramcurs == TOKEN_COMMA) {
			if (!paramdone) {
				err_source(parser->file, current(parser).span, "preceeding parameter incomplete");
			}

			advance(parser); /* , */
			paramdone = false;

		} else {
			err_source(parser->file, current(parser).span, "unexpected token");
		}
	}

	/*
	 * Anything between the ')' of the param list and the '{' of the block should be
	 * considered the return type
	 */
	if (!istk(parser, TOKEN_LBRACE)) {
		pfun->rettype = parse_type(parser);
	}

	pfun->block = parse_block(parser);

	return pfun;
}
