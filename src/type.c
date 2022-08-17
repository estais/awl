/* 
 * type.c
 *
 * This file is part of awl
 */

#include "type.h"

#include <string.h>
#include "vec.h"
#include "err.h"
#include "mem.h"

typedef enum primitive_kind {
	PRIM_U0,
	PRIM_U8,
	PRIM_U16,
	PRIM_U32,
	PRIM_U64,
	PRIM_S8,
	PRIM_S16,
	PRIM_S32,
	PRIM_S64,
	PRIM_BOOL,
} primitive_kind;

static const Type *primitives[] = {
#define PRIMADD(pk, nm, sz, sig) [pk] = &(Type) { .kind = TYPE_PRIMITIVE, .name = nm, .size = sz, .signd = sig }
	PRIMADD(PRIM_U0, "u0", 0, false),
	PRIMADD(PRIM_U8, "u8", 1, false),
	PRIMADD(PRIM_U16, "u16", 2, false),
	PRIMADD(PRIM_U32, "u32", 4, false),
	PRIMADD(PRIM_U64, "u64", 8, false),
	PRIMADD(PRIM_S8, "s8", 1, true),
	PRIMADD(PRIM_S16, "s16", 2, true),
	PRIMADD(PRIM_S32, "s32", 4, true),
	PRIMADD(PRIM_S64, "s64", 8, true),
	PRIMADD(PRIM_BOOL, "bool", 1, false),
#undef PRIMADD
};
static const size_t nprimitives = (sizeof(primitives) / sizeof(*primitives));

static typendx check_type(Typechecker *tc, PType *ptype);
static TVariable *check_variable(Typechecker *tc, PVariable *pvar, scopendx scope);
static TExpression *check_expression(Typechecker *tc, PExpression *pexpression, typendx ex);
static TStatement *check_statement(Typechecker *tc, PStatement *pstatement);
static TBlock *check_block(Typechecker *tc, PBlock *pblock, scopendx scope);
static TFun *check_fun(Typechecker *tc, PFun *pfun);

static void add_variable(Typechecker *tc, TVariable *tvar, scopendx scope);
static void add_fun(Typechecker *tc, TFun *tfun);

static typendx find_type_name(Typechecker *tc, Token name);
static varndx find_variable(Typechecker *tc, Token iden, scopendx scope);
static funndx find_fun(Typechecker *tc, Token iden);

static scopendx scope_add(Typechecker *tc, scopendx parent);
static Scope *scope_get(Typechecker *tc, scopendx scope);

static void numcompat(Typechecker *tc, Number *n, typendx ndx)
{
	Type *t = tc->tfile->types[ndx];
	size_t tbits = t->size * 8;
	
	if (n->bits > tbits) {
		err_source(tc->file, n->span, "size mismatch; expected %d bits but got %d bits", tbits, n->bits);
	}
}

Typechecker *typechecker_new()
{
	Typechecker *tc = alloct(Typechecker);
	tc->file = NULL;
	tc->pfile = NULL;
	tc->tfile = NULL;

	return tc;
}

TFile *typechecker_run(Typechecker *tc, File *file, PFile *pfile)
{
	tc->file = file;
	tc->pfile = pfile;
	tc->tfile = alloct(TFile);

	/* Initialise TFile */
	tc->tfile->scopes = NULL;
	tc->tfile->nscopes = 0;
	tc->tfile->tfuns = NULL;
	tc->tfile->ntfuns = 0;
	tc->tfile->fretcurs = NONDX;
	tc->tfile->tvariables = 0;
	tc->tfile->ntvariables = 0;
	tc->tfile->types = NULL;
	tc->tfile->ntypes = 0;

	/* Make primitives known to TFile */
	vec_join(tc->tfile->types, primitives, &tc->tfile->ntypes, nprimitives, sizeof(Type *));

	/* Create the root scope */
	scope_add(tc, NONDX);

	/* Check functions */
	for (size_t i = 0; i < pfile->npfuns; ++i) {
		TFun *tfun = check_fun(tc, pfile->pfuns[i]);
		add_fun(tc, tfun);
	}

	return tc->tfile;
}

void typechecker_reset(Typechecker *tc)
{
	tc->file = NULL;
	tc->pfile = NULL;
	tc->tfile = NULL;
}

static typendx check_type(Typechecker *tc, PType *ptype)
{
	switch (ptype->variant) {
		case PTYPE_NAMED: {
			Token name = ptype->name;
			typendx ndx = find_type_name(tc, name);
			if (ndx == NONDX) {
				err_source(tc->file, name.span, "unknown typename '%s'", name.content);
			}

			return ndx;
		}
		default: break;
	}

	return NONDX;
}

static TVariable *check_variable(Typechecker *tc, PVariable *pvar, scopendx scope)
{
	TVariable *tvariable = alloct(TVariable);
	tvariable->identifier = pvar->identifier;
	tvariable->type = NONDX;

	{
		Token iden = tvariable->identifier;
		varndx ndx = NONDX;
		if ((ndx = find_variable(tc, iden, scope)) != NONDX) {
			err_source(tc->file, iden.span, "redefinition of variable '%s'", iden.content);
		}
	}

	tvariable->type = check_type(tc, pvar->type);

	return tvariable;
}

static TExpression *check_expression(Typechecker *tc, PExpression *pexpression, typendx ex)
{
	TExpression *texpression = alloct(TExpression);
	texpression->variant = _TNODE_NULL;
	texpression->number = NULL;;

	switch (pexpression->variant) {
		case PEXPRESSION_NUMLIT: {
			Number *n = pexpression->number;
			numcompat(tc, n, ex);

			texpression->variant = TEXPRESSION_NUMLIT;
			texpression->number = n;
			break;
		}
		default: break;
	}

	return texpression;
}

static TStatement *check_statement(Typechecker *tc, PStatement *pstatement)
{
	TStatement *tstatement = alloct(TStatement);
	tstatement->variant = _TNODE_NULL;
	tstatement->expr = NULL;

	switch (pstatement->variant) {
		case PSTATEMENT_RETURN: {
			tstatement->variant = TSTATEMENT_RETURN;
			tstatement->expr = check_expression(tc, pstatement->expr, tc->tfile->fretcurs);
			break;
		}
		default: break;
	}

	return tstatement;
}

static TBlock *check_block(Typechecker *tc, PBlock *pblock, scopendx parent)
{
	TBlock *tblock = alloct(TBlock);
	tblock->scope = scope_add(tc, parent);
	tblock->statements = NULL;
	tblock->nstatements = 0;

	for (size_t i = 0; i < pblock->nstatements; ++i) {
		PStatement *ps = pblock->statements[i];
		TStatement *ts = check_statement(tc, ps);

		vec_push(tblock->statements, &ts, &tblock->nstatements, sizeof(TStatement *));
	}

	return tblock;
}

static TFun *check_fun(Typechecker *tc, PFun *pfun)
{
	TFun *tfun = alloct(TFun);
	tfun->scope = scope_add(tc, 0);
	tfun->identifier = pfun->identifier;
	tfun->rettype = NONDX;
	tfun->block = NULL;

	{
		Token iden = tfun->identifier;
		funndx ndx = NONDX;
		if ((ndx = find_fun(tc, iden)) != NONDX) {
			err_source(tc->file, iden.span, "redefinition of function '%s'", iden.content);
		}
	}

	for (size_t i = 0; i < pfun->nparams; ++i) {
		PVariable *pvar = pfun->params[i];
		TVariable *tvar = check_variable(tc, pvar, tfun->scope);
		add_variable(tc, tvar, tfun->scope);
	}

	/* If no type has been specified, default to u0 */
	if (!pfun->rettype) {
		tfun->rettype = PRIM_U0;
	} else {
		tfun->rettype = check_type(tc, pfun->rettype);
	}

	tc->tfile->fretcurs = tfun->rettype;

	tfun->block = check_block(tc, pfun->block, tfun->scope);

	return tfun;
}

static void add_variable(Typechecker *tc, TVariable *tvar, scopendx scope)
{
	Scope *obj = scope_get(tc, scope);
	varndx ndx = tc->tfile->ntvariables;

	/* Push object */
	vec_push(tc->tfile->tvariables, &tvar, &tc->tfile->ntvariables, sizeof(TVariable *));

	/* Push index */
	vec_push(obj->vars, &ndx, &obj->nvars, sizeof(varndx));
}

static void add_fun(Typechecker *tc, TFun *tfun) {
	Scope *scope = scope_get(tc, 0);
	funndx ndx = tc->tfile->ntfuns;

	vec_push(tc->tfile->tfuns, &tfun, &tc->tfile->ntfuns, sizeof(TFun *));
	vec_push(scope->funs, &ndx, &scope->nfuns, sizeof(funndx));
}

static typendx find_type_name(Typechecker *tc, Token name)
{
	for (size_t i = 0; i < tc->tfile->ntypes; ++i) {
		Type *type = tc->tfile->types[i];

		if (type->kind == TYPE_PRIMITIVE && !strcmp(name.content, type->name)) {
			return i;
		}
	}

	return NONDX;
}

static varndx find_variable(Typechecker *tc, Token iden, scopendx scope)
{
	scopendx ndx = scope;
	Scope *obj = NULL;

	while (ndx != NONDX) {
		obj = scope_get(tc, ndx);

		for (size_t i = 0; i < obj->nvars; ++i) {
			varndx v = obj->vars[i];
			TVariable *var = tc->tfile->tvariables[v];

			if (!strcmp(iden.content, var->identifier.content)) {
				return ndx;
			}
		}

		ndx = obj->parent;
	}

	return NONDX;
}

static funndx find_fun(Typechecker *tc, Token iden)
{
	Scope *scope = scope_get(tc, 0);

	for (size_t i = 0; i < scope->nfuns; ++i) {
		funndx ndx = scope->funs[i];
		TFun *tfun = tc->tfile->tfuns[ndx];
		
		if (!strcmp(iden.content, tfun->identifier.content)) {
			return ndx;
		}
	}

	return NONDX;
}

static scopendx scope_add(Typechecker *tc, scopendx parent)
{
	Scope *obj = alloct(Scope);
	obj->types = NULL;
	obj->ntypes = 0;
	obj->funs = NULL;
	obj->nfuns = 0;
	obj->parent = parent;
	obj->children = NULL;
	obj->nchildren = 0;

	scopendx ndx = tc->tfile->nscopes;
	vec_push(tc->tfile->scopes, &obj, &tc->tfile->nscopes, sizeof(Scope *));

	if (parent != NONDX) {
		Scope *parobj = scope_get(tc, parent);
		vec_push(parobj->children, &ndx, &parobj->nchildren, sizeof(scopendx));
	}

	return ndx;
}

static Scope *scope_get(Typechecker *tc, scopendx scope)
{
	Scope *obj = tc->tfile->scopes[scope];
	if (!obj) {
		err_internal("could not get invalid Scope object at index %d", scope);
	}

	return obj;
}
