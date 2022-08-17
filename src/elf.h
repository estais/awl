/*
 * elf.h
 *
 * This file is part of awl
 */

#pragma once

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

/* sh_type */
enum {
	SHT_NULL = 0x00,
	SHT_PROGBITS = 0x01,
	SHT_SYMTAB = 0x02,
	SHT_STRTAB = 0x03,
	SHT_RELA = 0x04,
	SHT_NOBITS = 0x08,
};

/* sh_flags */
enum {
	SHF_WRITE = 0x01,
	SHF_ALLOC = 0x02,
	SHF_EXECINSTR = 0x04,
	SHF_STRINGS = 0x20,
	SHF_INFO_LINK = 0x40,
};

/* specific shndx constants */
enum {
	SHN_CUR = -1, /* not ELF standard; here, identified as elf->secndx where needed */

	SHN_UNDEF = 0x0000,
	SHN_ABS = 0xFFF1,
	SHN_COMMON = 0xFFF2,
};

/* symbol bindings */
enum {
	STB_LOCAL = 0x00,
	STB_GLOBAL = 0x01,
};

/* symbol types */
enum {
	STT_NOTYPE = 0x00,
	STT_OBJECT = 0x01,
	STT_FUNC = 0x02,
	STT_SECTION = 0x03,
	STT_FILE = 0x04,
};

typedef struct ElfSecHdr {
	uint32_t name;
	uint32_t type;
	uint64_t flags;
	uint64_t addr;
	uint64_t offset;
	uint64_t size;
	uint32_t link;
	uint32_t info;
	uint64_t addralign;
	uint64_t entsize;
} ElfSecHdr;

typedef struct ElfSymbol {
	uint32_t name;
	uint8_t info;
	uint8_t other;
	int shndx;
	uint64_t value;
	uint64_t size;
} ElfSymbol;

typedef struct ElfSection {
	ElfSecHdr header;
	const char *strname;
	uint8_t *data;
} ElfSection;

typedef struct Elf {
	int fd;
	size_t curs; /* ELF file cursor, incremented when emitting */
	int secndx; /* section index set with elf_set_section() */

	ElfSection **sections;
	size_t nsections;

	ElfSymbol *symbols;
	size_t nsymbols;

	uint16_t shstrndx;

	uint8_t *shstrdat; /* data for .shstrtab */
	size_t shstrsize;

	uint8_t *strdat; /* data for .strtab */
	size_t strsize;
} Elf;

Elf *elf_new(const char *path);
void elf_add_section(Elf *elf, const char *name, uint32_t type, uint64_t flags);
void elf_set_section(Elf *elf, const char *name);
void elf_add_symbol(Elf *elf, int sec, const char *name, uint8_t binding, uint8_t type, uint64_t value);
void elf_write(Elf *elf, uint8_t *data, size_t size);
void elf_end(Elf *elf);
