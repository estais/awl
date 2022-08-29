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

/* Get the nth byte of some value */
#define NBYT(v, n) ((v >> 8 * n) & 0xFF)

/* r/m part of ModR/M; aka, the register number */
typedef unsigned char reg;

static const reg RAX = 0;

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
						NBYT(number->u16, 0),
						NBYT(number->u16, 1),
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
						NBYT(number->u32, 0),
						NBYT(number->u32, 1),
						NBYT(number->u32, 2),
						NBYT(number->u32, 3),
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
						NBYT(number->u64, 0),
						NBYT(number->u64, 1),
						NBYT(number->u64, 2),
						NBYT(number->u64, 3),
						NBYT(number->u64, 4),
						NBYT(number->u64, 5),
						NBYT(number->u64, 6),
						NBYT(number->u64, 7),
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
}

static const char *fileext(const char *path)
{
	size_t length = strlen(path);
	char *res = acalloc(length + 2, sizeof(char));
	res = strdup(path);
	strncat(res, ".o", 3);

	return res;
}
