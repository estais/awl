/*
 * strbuf.c
 *
 * This file is part of awl
 */

#include "strbuf.h"

#include "mem.h"

#define CHSZ (sizeof(char))

static void expand(StrBuf *strbuf, size_t by);

StrBuf *strbuf_new(size_t initalloc)
{
	StrBuf *strbuf = alloct(StrBuf);
	strbuf->data = acalloc(initalloc ? initalloc : 1, CHSZ);
	strbuf->length = 0;
	strbuf->allocd = initalloc;

	return strbuf;
}

const char *strbuf_release(StrBuf *strbuf)
{
	char *str = strbuf->data;
	str[strbuf->length] = 0;

	afree(strbuf);
	return str;
}

size_t strbuf_length(StrBuf *strbuf)
{
	return strbuf->length;
}

void strbuf_putc(StrBuf *strbuf, char c)
{
	expand(strbuf, 1);
	strbuf->data[strbuf->length] = c;
	++strbuf->length;
}

static void expand(StrBuf *strbuf, size_t by)
{
	size_t avail = strbuf->allocd - strbuf->length;
	if (avail < by) {
		strbuf->data = arecalloc(strbuf->data, strbuf->allocd + by + 1, CHSZ);
		strbuf->allocd += by;
	}
}
