/*
 * parser.h
 *
 * This file is part of awl
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "lexer.h"

typedef struct Number {
	Span span;
	size_t bits;
	bool sig;

	union {
		uint8_t u8;
		uint16_t u16;
		uint32_t u32;
		uint64_t u64;
		int8_t s8;
		int16_t s16;
		int32_t s32;
		int64_t s64;
	};
} Number;

Number *number_make(Token from);

/*
 * Some parser nodes have different variants; for example, a statement may be a
 * return statement or a while statement. Where this is the case, an instance of
 * this enum should be stored to determine the variant of the node.
 */
typedef enum p_node_variant {
	_PNODE_NULL,

	PTYPE_NAMED,

	PEXPRESSION_NUMLIT,

	PSTATEMENT_RETURN,
	PSTATEMENT_RETURN_NOVAL,
} p_node_variant;

typedef struct PType {
	p_node_variant variant;

	union {
		Token name;
	};
} PType;

typedef struct PVariable {
	Token identifier;
	PType *type;
} PVariable;

typedef struct PExpression {
	p_node_variant variant;

	union {
		Number *number;
	};
} PExpression;

typedef struct PStatement {
	p_node_variant variant;
	Span span;

	union {
		PExpression *expr;
	};
} PStatement;

typedef struct PBlock {
	PStatement **statements;
	size_t nstatements;
} PBlock;

typedef struct PFun {
	Token identifier;

	PVariable **params;
	size_t nparams;

	PType *rettype;

	PBlock *block;
} PFun;

typedef struct PFile {
	PFun **pfuns;
	size_t npfuns;
} PFile;

typedef struct Parser {
	File *file;
	Lexer *lexer;
	Token *tokens;
	int tkndx;

	PFile *pfile;
} Parser;

Parser *parser_new();
PFile *parser_run(Parser *parser, File *file);
void parser_reset(Parser *parser);
