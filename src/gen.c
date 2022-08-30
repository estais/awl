/*
 * gen.c
 *
 * This file is part of awl
 */

#include "gen.h"

#include <string.h>
#include "vec.h"
#include "mem.h"
#include "err.h"

#define STACKOFF_DEFAULT 4

/* Get the nth byte of some value */
#define NBYTE(v, n) ((v >> 8 * n) & 0xFF)

/* Get the nth bit of some value */
#define NBIT(v, n) ((v >> n) & 1)

/* Set the nth bit of some value to state (s) */
#define NBITSET(v, n, s) (v |= s << n)

/* Register number */
typedef unsigned char reg;

static const reg RAX = 0;
static const reg RCX = 1;
static const reg RDX = 2;
static const reg RBP = 5;
static const reg RSI = 6;
static const reg RDI = 7;

static const reg paramreg[4] = { RDI, RSI, RDX, RCX };

static uint8_t stack(Gen *gen, uint8_t size);
static uint8_t modrm(uint8_t mod, uint8_t op, uint8_t rm);
static void gen_expr(Gen *gen, TExpression *expression, reg dest);
static void gen_statement(Gen *gen, TStatement *statement);
static void gen_fun(Gen *gen, size_t ndx);
static const char *fileext(const char *path);

Gen *gen_new()
{
	Gen *gen = alloct(Gen);
	gen->tfile = NULL;
	gen->elf = NULL;

	return gen;
}

void gen_run(Gen *gen, const char *srcpath, TFile *tfile)
{
	const char *elfpath = fileext(srcpath);

	gen->tfile = tfile;
	gen->elf = elf_new(elfpath);
	gen->stackoff = STACKOFF_DEFAULT;

	elf_add_section(gen->elf, ".text", SHT_PROGBITS, SHF_ALLOC | SHF_EXECINSTR);

	for (size_t i = 0; i < tfile->ntfuns; ++i) {
		gen_fun(gen, i);
	}

	elf_end(gen->elf);
}

void gen_reset(Gen *gen)
{
	gen->tfile = NULL;
	gen->elf = NULL;
}

static uint8_t stack(Gen *gen, uint8_t size)
{
	uint8_t curr = gen->stackoff;
	gen->stackoff += size;
	return -curr;
}

static uint8_t modrm(uint8_t mod, uint8_t op, uint8_t rm)
{
	uint8_t i = 0;

	/* r/m */
	NBITSET(i, 0, NBIT(rm, 0));
	NBITSET(i, 1, NBIT(rm, 1));
	NBITSET(i, 2, NBIT(rm, 2));

	/* op */
	NBITSET(i, 3, NBIT(op, 0));
	NBITSET(i, 4, NBIT(op, 1));
	NBITSET(i, 5, NBIT(op, 2));

	/* mod */
	NBITSET(i, 6, NBIT(mod, 0));
	NBITSET(i, 7, NBIT(mod, 1));

	return i;
}

static void gen_expr(Gen *gen, TExpression *expression, reg dest)
{
	switch (expression->variant) {
		case TEXPRESSION_NUMLIT: {
			Number *number = expression->number;

			uint8_t *data = NULL;
			size_t ndata = 0;

			switch (number->bits) {
				case 8: {
					uint8_t temp[] = {
						0xB8 + dest,
						number->u8,
						0x00,
						0x00,
						0x00,
					};
					ndata = sizeof(temp);
					data = acalloc(ndata, sizeof(uint8_t));
					data = temp;
					break;
				}

				case 16: {
					uint8_t temp[] = {
						0xB8 + dest,
						NBYTE(number->u16, 0),
						NBYTE(number->u16, 1),
						0x00,
						0x00,
					};
					ndata = sizeof(temp);
					data = acalloc(ndata, sizeof(uint8_t));
					data = temp;
					break;
				}

				case 32: {
					uint8_t temp[] = {
						0xB8 + dest,
						NBYTE(number->u32, 0),
						NBYTE(number->u32, 1),
						NBYTE(number->u32, 2),
						NBYTE(number->u32, 3),
					};
					ndata = sizeof(temp);
					data = acalloc(ndata, sizeof(uint8_t));
					data = temp;
					break;
				}
				
				case 64: {
					uint8_t temp[] = {
						0x48,
						0xB8 + dest,
						NBYTE(number->u64, 0),
						NBYTE(number->u64, 1),
						NBYTE(number->u64, 2),
						NBYTE(number->u64, 3),
						NBYTE(number->u64, 4),
						NBYTE(number->u64, 5),
						NBYTE(number->u64, 6),
						NBYTE(number->u64, 7),
					};

					ndata = sizeof(temp);
					data = acalloc(ndata, sizeof(uint8_t));
					data = temp;
					break;
				}
				default: break;
			}

			if (!data) {
				err_internal("data is NULL, some fatal error has occurred; sign=%d, bits=%ld", number->sig, number->bits);
			}
			elf_write(gen->elf, data, ndata);

			break;
		}
		default: break;
	}
}

static void gen_statement(Gen *gen, TStatement *statement)
{
	switch (statement->variant) {
		case TSTATEMENT_RETURN: {
			gen_expr(gen, statement->expr, RAX);
			break;
		}
		default: break;
	}
}

static void gen_fun(Gen *gen, size_t ndx)
{
	TFun *tfun = gen->tfile->tfuns[ndx];

	elf_set_section(gen->elf, ".text");

	size_t value = gen->elf->sections[gen->elf->secndx]->header.size;

	elf_add_symbol(gen->elf, SHN_CUR, tfun->identifier.content, STB_GLOBAL, STT_FUNC, value);

	uint8_t prologue[] = {
		0x55, 			/* push %rbp */
		0x48, 0x89, 0xE5 	/* mov %rsi, %rbp */
	};
	elf_write(gen->elf, prologue, sizeof(prologue));

	/* Move parameters onto stack */
	Scope *scope = gen->tfile->scopes[tfun->scope];
	for (size_t i = 0; i < scope->nvars; ++i) {
		TVariable *v = gen->tfile->tvariables[scope->vars[i]];
		Type *t = gen->tfile->types[v->type];

		uint8_t data[] = {
			0x48, 0x89, modrm(1, paramreg[i], RBP), stack(gen, t->size),
		};

		elf_write(gen->elf, data, sizeof(data));
	}

	for (size_t i = 0; i < tfun->block->nstatements; ++i) {
		TStatement *statement = tfun->block->statements[i];
		gen_statement(gen, statement);
	}

	elf_set_section(gen->elf, ".text");
	uint8_t epilogue[] = {
		0x5D, 			/* pop %rbp */
		0xC3, 			/* ret */
	};
	elf_write(gen->elf, epilogue, sizeof(epilogue));

	gen->stackoff = STACKOFF_DEFAULT;
}

static const char *fileext(const char *path)
{
	size_t length = strlen(path);
	char *res = acalloc(length + 2, sizeof(char));
	res = strdup(path);
	strncat(res, ".o", 3);

	return res;
}
