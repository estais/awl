/*
 * parser.c
 *
 * This file is part of awl
 */

#include "parser.h"

#include <stdlib.h>
#include <stdbool.h>
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

NumericLiteral numlit_make(Token from)
{
	NumericLiteral numlit;

	if (from.kind == TOKEN_NUMLIT_FLT) {
		err_internal("floating point numbers are not yet implemented");
	}

	if (from.kind != TOKEN_NUMLIT_INT) {
		err_internal("cannot convert token of type %d into a numeric literal", (int)from.kind);
	}

	__int128_t asint = strtol(from.content, NULL, 10);

	if (asint >= 0) {
		/* Unsigned */

		if (asint <= UINT8_MAX) {
			numlit.size = NUMLIT_U8;
			numlit.u8 = (uint8_t)asint;
		} else if (asint > UINT8_MAX && asint <= UINT16_MAX) {
			numlit.size = NUMLIT_U16;
			numlit.u16 = (uint16_t)asint;
		} else if (asint > UINT16_MAX && asint <= UINT32_MAX) {
			numlit.size = NUMLIT_U32;
			numlit.u32 = (uint32_t)asint;
		} else if (asint > UINT32_MAX && asint <= UINT64_MAX) {
			numlit.size = NUMLIT_U64;
			numlit.u64 = (uint64_t)asint;
		}
	} else if (asint < 0) {
		/* Signed */

		if (asint <= INT8_MAX) {
			numlit.size = NUMLIT_S8;
			numlit.s8 = (int8_t)asint;
		} else if (asint > INT8_MAX && asint <= INT16_MAX) {
			numlit.size = NUMLIT_S16;
			numlit.s16 = (int16_t)asint;
		} else if (asint > INT16_MAX && asint <= INT32_MAX) {
			numlit.size = NUMLIT_S32;
			numlit.s32 = (int32_t)asint;
		} else if (asint > INT32_MAX && asint <= INT64_MAX) {
			numlit.size = NUMLIT_S64;
			numlit.s64 = (int64_t)asint;
		}
	}

	return numlit;
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

	switch (istk(parser, TOKEN_FUN)) {
		case TOKEN_FUN: {
			PFun *pfun = parse_fun(parser);
			vec_push(parser->pfile->pfuns, &pfun, &parser->pfile->npfuns, sizeof(PFun *));

			break;
		}
		default: err_source(parser->file, current(parser).span, "unexpected token");
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
	pexpression->numlit = (NumericLiteral){ 0 };

	switch (istk(parser, TOKEN_NUMLIT_INT | TOKEN_NUMLIT_FLT)) {
		case TOKEN_NUMLIT_INT:
		case TOKEN_NUMLIT_FLT: {
			pexpression->variant = PEXPRESSION_NUMLIT;
			pexpression->numlit = numlit_make(current(parser));

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
			advance(parser); /* return */
			
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
