/*
 * err.c
 *
 * This file is part of awl
 */

#include "err.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

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
