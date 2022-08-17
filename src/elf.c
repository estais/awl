/*
 * elf.c
 *
 * This file is part of awl
 */

#include "elf.h"

#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include "vec.h"
#include "mem.h"
#include "err.h"

#define EHSIZE 0x40 /* ELF header size */
#define SHENTSIZE 0x40 /* Section header size */
#define STENTSIZE 0x18 /* Symtab entry size */

static uint8_t *createsymtab(Elf *elf, uint64_t *nlocalsyms);
static uint32_t addshstr(Elf *elf, const char *str);
static uint32_t addstr(Elf *elf, const char *str);
static void emithdr(Elf *elf);
static void emitsechdr(Elf *elf, ElfSecHdr hdr);
static void emit(Elf *elf, void *data, size_t size);
static void skip(Elf *elf, size_t n);
static void emitb(Elf *elf, uint8_t data);
static void emitw(Elf *elf, uint16_t data);
static void emitd(Elf *elf, uint32_t data);
static void emitq(Elf *elf, uint64_t data);

Elf *elf_new(const char *path)
{
	Elf *elf = alloct(Elf);
	elf->fd = -1;
	elf->curs = 0;
	elf->secndx = 0;
	elf->sections = NULL;
	elf->nsections = 0;
	elf->symbols = NULL;
	elf->nsymbols = 0;
	elf->shstrndx = 0;
	elf->shstrdat = NULL;
	elf->shstrsize = 0;
	elf->strdat = NULL;
	elf->strsize = 0;

	/* rw-r-r */
	mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
	if ((elf->fd = creat(path, mode)) == -1) {
		err_internal("could not create ELF file '%s'", path);
	}

	/* Null section */
	elf_add_section(elf, "", SHT_NULL, 0);

	/* Null symbol */
	elf_add_symbol(elf, SHN_UNDEF, "", STB_LOCAL, STT_NOTYPE, 0);

	return elf;
}

void elf_add_section(Elf *elf, const char *name, uint32_t type, uint64_t flags)
{
	ElfSection *sec = alloct(ElfSection);
	sec->strname = name;
	sec->header = (ElfSecHdr){
		.name = addshstr(elf, name),
		.type = type,
		.flags = flags,
		.addr = 0,
		.offset = 0,
		.size = 0,
		.link = 0,
		.info = 0,
		.addralign = 0,
		.entsize = 0,
	};
	sec->data = NULL;
	vec_push(elf->sections, &sec, &elf->nsections, sizeof(ElfSection *));
}

void elf_set_section(Elf *elf, const char *name)
{
	for (size_t i = 0; i < elf->nsections; ++i) {
		if (!strcmp(name, elf->sections[i]->strname)) {
			elf->secndx = i;
			return;
		}
	}

	err_internal("tried to access invalid ELF section '%s'", name);
}

void elf_add_symbol(Elf *elf, int sec, const char *name, uint8_t binding, uint8_t type, uint64_t value)
{
	ElfSymbol symbol = {
		.name = addstr(elf, name),
		.info = ((binding << 4) + (type & 0xF)),
		.other = 0,
		.shndx = (sec == SHN_CUR ? elf->secndx : sec),
		.value = value,
		.size = 0,
	};

	vec_push(elf->symbols, &symbol, &elf->nsymbols, sizeof(ElfSymbol));
}

void elf_write(Elf *elf, uint8_t *data, size_t size)
{
	ElfSection *curr = elf->sections[elf->secndx];
	vec_join(curr->data, data, &curr->header.size, size, sizeof(uint8_t));
}

void elf_end(Elf *elf)
{
	/* Construct symtab */
	size_t symtabndx = elf->nsections;
	elf_add_section(elf, ".symtab", SHT_SYMTAB, 0);

	size_t nlocalsyms = 0;
	uint8_t *symtabdat = createsymtab(elf, &nlocalsyms);

	elf->sections[symtabndx]->header.info = nlocalsyms;
	elf->sections[symtabndx]->header.size = elf->nsymbols * STENTSIZE;
	elf->sections[symtabndx]->header.entsize = STENTSIZE;
	elf->sections[symtabndx]->data = symtabdat;

	/* Construct strtab */
	size_t strtabndx = elf->nsections;
	elf_add_section(elf, ".strtab", SHT_STRTAB, 0);
	elf->sections[strtabndx]->header.size = elf->strsize;
	elf->sections[strtabndx]->data = elf->strdat;

	/* Set symtab link */
	elf->sections[symtabndx]->header.link = strtabndx;

	/* Construct shstrtab */
	size_t shstrtabndx = elf->nsections;
	elf_add_section(elf, ".shstrtab", SHT_STRTAB, 0);
	elf->sections[shstrtabndx]->header.size = elf->shstrsize;
	elf->sections[shstrtabndx]->data = elf->shstrdat;
	elf->shstrndx = shstrtabndx;

	/* Set section offset */
	uint64_t off = EHSIZE + (SHENTSIZE * elf->nsections);
	for (size_t i = 1; i < elf->nsections; ++i) {
		ElfSection *sec = elf->sections[i];
		sec->header.offset = off;
		off += sec->header.size;
	}

	/* Emit ELF header */
	emithdr(elf);

	/* Emit section headers */
	for (size_t i = 0; i < elf->nsections; ++i) {
		ElfSection *sec = elf->sections[i];
		emitsechdr(elf, sec->header);
	}

	/* Emit section data */
	for (size_t i = 0; i < elf->nsections; ++i) {
		ElfSection *sec = elf->sections[i];
		emit(elf, sec->data, sec->header.size);
	}

	afree(elf);
}

#define ENTSET(e, t, o, v) (*(t *)(e + o) = v)

static uint8_t *createsymtab(Elf *elf, uint64_t *nlocalsyms)
{
	uint8_t *res = acalloc(elf->nsymbols, STENTSIZE * sizeof(uint8_t));

	size_t entndx = 0;

	/* STB_LOCAL */
	for (size_t i = 0; i < elf->nsymbols; ++i) {
		ElfSymbol sym = elf->symbols[i];

		if (sym.info >> 4 != STB_LOCAL) continue;

		uint8_t *ent = res + entndx * STENTSIZE;
		*(uint32_t *)(ent + 0) = sym.name;
		*(uint8_t *)(ent + 4) = sym.info;
		*(uint8_t *)(ent + 5) = sym.other;
		*(uint16_t *)(ent + 6) = sym.shndx;
		*(uint64_t *)(ent + 8) = sym.value;
		*(uint64_t *)(ent + 16) = sym.size;

		++entndx;
		++(*nlocalsyms);
	}

	/* STB_GLOBAL */
	for (size_t i = 0; i < elf->nsymbols; ++i) {
		ElfSymbol sym = elf->symbols[i];

		if (sym.info >> 4 != STB_GLOBAL) continue;

		uint8_t *ent = res + entndx * STENTSIZE;
		ENTSET(ent, uint32_t, 0, sym.name);
		ENTSET(ent, uint8_t, 4, sym.info);
		ENTSET(ent, uint8_t, 5, sym.other);
		ENTSET(ent, uint16_t, 6, sym.shndx);
		ENTSET(ent, uint64_t, 8, sym.value);
		ENTSET(ent, uint64_t, 16, sym.size);

		++entndx;
	}

	return res;
}

static uint32_t addshstr(Elf *elf, const char *str)
{
	uint32_t off = elf->shstrsize;
	vec_join(elf->shstrdat, (uint8_t *)str, &elf->shstrsize, strlen(str) + 1, sizeof(uint8_t));
	return off;
}

static uint32_t addstr(Elf *elf, const char *str)
{
	uint32_t off = elf->strsize;
	vec_join(elf->strdat, (uint8_t *)str, &elf->strsize, strlen(str) + 1, sizeof(uint8_t));
	return off;
}

static void emithdr(Elf *elf)
{
	emitb(elf, 0x7F); /* EI_MAG0 */
	emitb(elf, 0x45); /* EI_MAG1 */
	emitb(elf, 0x4C); /* EI_MAG2 */
	emitb(elf, 0x46); /* EI_MAG3 */

	emitb(elf, 0x02); /* EI_CLASS = 64 bit */
	emitb(elf, 0x01); /* EI_DATA = little endian */
	emitb(elf, 0x01); /* EI_VERSION */
	emitb(elf, 0x00); /* EI_OSABI = System V */
	emitb(elf, 0x00); /* EI_ABIVERSION */

	skip(elf, 7); /* EI_PAD */

	emitw(elf, 0x01); /* e_type = ET_REL */
	emitw(elf, 0x3E); /* e_machine = AMD x86-64 */
	emitd(elf, 0x01); /* e_version */
	emitq(elf, 0x00); /* e_entry */
	emitq(elf, 0x00); /* e_phoff */
	emitq(elf, 0x40); /* e_shoff */
	emitd(elf, 0x00); /* e_flags */
	emitw(elf, EHSIZE); /* e_ehsize */
	emitw(elf, 0x00); /* e_phentsize */
	emitw(elf, 0x00); /* e_phnum */
	emitw(elf, SHENTSIZE); /* e_shentsize */
	emitw(elf, elf->nsections); /* e_shnum */
	emitw(elf, elf->shstrndx); /* e_shstrndx */
}

static void emitsechdr(Elf *elf, ElfSecHdr hdr)
{
	emitd(elf, hdr.name);
	emitd(elf, hdr.type);
	emitq(elf, hdr.flags);
	emitq(elf, hdr.addr);
	emitq(elf, hdr.offset);
	emitq(elf, hdr.size);
	emitd(elf, hdr.link);
	emitd(elf, hdr.info);
	emitq(elf, hdr.addralign);
	emitq(elf, hdr.entsize);
}

static void emit(Elf *elf, void *data, size_t size)
{
	write(elf->fd, data, size);
	elf->curs += size;
}

static void skip(Elf *elf, size_t n)
{
	for (size_t i = 0; i < n; ++i) {
		emitb(elf, 0);
	}
}

static void emitb(Elf *elf, uint8_t data)
{
	emit(elf, &data, 1);
}

static void emitw(Elf *elf, uint16_t data)
{
	emit(elf, &data, 2);
}

static void emitd(Elf *elf, uint32_t data)
{
	emit(elf, &data, 4);
}

static void emitq(Elf *elf, uint64_t data)
{
	emit(elf, &data, 8);
}
