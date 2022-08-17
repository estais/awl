/*
 * err.c
 *
 * This file is part of awl
 */

#include "err.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include "strbuf.h"
#include "mem.h"

#define SB_INITALLOC_OFFSET 10

void _err_internal(const char *filename, int line, const char *fmt, ...)
{
	fprintf(stderr, "awl (internal %s:%d): ", filename, line);

	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	fprintf(stderr, "\n");

	exit(EXIT_FAILURE);
}

void err_user(const char *fmt, ...)
{
	fprintf(stderr, "awl: ");

	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	fprintf(stderr, "\n");

	exit(EXIT_FAILURE);
}

void err_source(File *file, Span span, const char *fmt, ...)
{
	/* Source excerpt */
	const char *src = file->lines[span.linendx];

	/* Index -> Count values */
	int linenum = span.linendx + 1;
	int finum = span.first + 1;

	/* Digit count of linenum, to align '|' */
	int lnumdigs = floor(log10(abs(linenum))) + 1;

	/* Construct the offset */
	StrBuf *offset_sb = strbuf_new(SB_INITALLOC_OFFSET);
	for (const char *c = src; c < src + span.first; ++c) {
		strbuf_putc(offset_sb, (*c == '\t' ? '\t' : ' '));
	}
	const char *offset = strbuf_release(offset_sb);

	/*
	 * Construct the underline; length = last - first - 1, as the first position is
	 * occupied by '^'
	 */
	size_t ullen = (span.last - span.first) - 1;
	char *underline = NULL;
	if (ullen > 0) {
		underline = acalloc(ullen + 1, sizeof(char));
		memset(underline, (int)'~', ullen * sizeof(char));
	}

	/* The actual underline we will pass to fprintf() */
	const char *ulactual = underline ? underline : "";

	fprintf(stderr, "%s:%d:%d: ", file->path, linenum, finum);

	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	fprintf(stderr, "\n%d | %s\n%*c | %s^%s\n", linenum, src, lnumdigs, ' ', offset, ulactual);

	exit(EXIT_FAILURE);
}
