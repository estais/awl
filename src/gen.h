/*
 * gen.h
 *
 * This file is part of awl
 */

#pragma once

#include "elf.h"
#include "type.h"

typedef struct Gen {
	TFile *tfile;
	Elf *elf;
} Gen;

Gen *gen_new();
void gen_run(Gen *gen, const char *srcpath, TFile *tfile);
void gen_reset(Gen *gen);
