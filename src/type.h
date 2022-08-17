/*
 * type.h
 *
 * This file is part of awl
 */

#pragma once

#include "parser.h"

/*
 * Certain pieces of data are stored centrally in a TFile in dynamically-allocated
 * arrays. To avoid passing or storing structures in functions or other structures,
 * an index (indexing the specific object in its central storage location) is used
 * in stead. To differenatiate usage of such indexes, use these typedef'd ints.
 */
typedef int scopendx;
typedef int typendx;
typedef int varndx;
typedef int funndx;

#define NONDX -1 /* Default value for *ndx variables */

typedef enum type_kind {
	TYPE_PRIMITIVE,
} type_kind;

typedef struct Type {
	type_kind kind;
	const char *name;
	size_t size; /* Bytes */
	bool signd;
} Type;

/* Akin to p_node_variant, but for Typechecker nodes */
typedef enum t_node_variant {
	_TNODE_NULL,

	TEXPRESSION_NUMLIT,

	TSTATEMENT_RETURN,
} t_node_variant;

typedef struct Scope {
	typendx *types;
	size_t ntypes;

	funndx *funs;
	size_t nfuns;

	varndx *vars;
	size_t nvars;
	
	scopendx parent;

	scopendx *children;
	size_t nchildren;
} Scope;

typedef struct TVariable {
	Token identifier;
	typendx type;
} TVariable;

typedef struct TExpression {
	t_node_variant variant;
} TExpression;

typedef struct TStatement {
	t_node_variant variant;

	union {
		TExpression *expr;
	};
} TStatement;

typedef struct TBlock {
	scopendx scope;

	TStatement **statements;
	size_t nstatements;
} TBlock;

typedef struct TFun {
	scopendx scope;

	Token identifier;
	typendx rettype;
	TBlock *block;
} TFun;

typedef struct TFile {
	Scope **scopes;
	size_t nscopes;

	TFun **tfuns;
	size_t ntfuns;

	TVariable **tvariables;
	size_t ntvariables;

	Type **types;
	size_t ntypes;
} TFile;

typedef struct Typechecker {
	File *file;
	PFile *pfile;
	TFile *tfile;
} Typechecker;

Typechecker *typechecker_new();
TFile *typechecker_run(Typechecker *tc, File *file, PFile *pfile);
void typechecker_reset(Typechecker *tc);
