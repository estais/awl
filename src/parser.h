/*
 * parser.h
 *
 * This file is part of awl
 */

#pragma once

#include <stdint.h>
#include "lexer.h"

typedef struct NumericLiteral {
	enum {
		NUMLIT_U8,
		NUMLIT_U16,
		NUMLIT_U32,
		NUMLIT_U64,
		NUMLIT_S8,
		NUMLIT_S16,
		NUMLIT_S32,
		NUMLIT_S64,
	} size;

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
} NumericLiteral;

NumericLiteral numlit_make(Token from);

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
		NumericLiteral numlit;
	};
} PExpression;

typedef struct PStatement {
	p_node_variant variant;

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
