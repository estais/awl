/*
 * strbuf.h
 *
 * This file is part of awl
 */

#pragma once

#include <stddef.h>

typedef struct StrBuf {
	char *data;
	size_t length;
	size_t allocd;
} StrBuf;

StrBuf *strbuf_new(size_t initalloc);
const char *strbuf_release(StrBuf *strbuf);
size_t strbuf_length(StrBuf *strbuf);
void strbuf_putc(StrBuf *strbuf, char c);
