/*
 * vec.c
 *
 * This file is part of awl
 */

#include "vec.h"

#include <string.h>
#include "mem.h"

void *_vec_push(void *vec, void *elem, size_t *length, size_t size)
{
	char *temp = arecalloc(vec, *length + 1, size);
	memcpy(temp + *length * size, elem, size);
	++(*length);
	return temp;
}

void *_vec_join(void *va, void *vb, size_t *alen, size_t blen, size_t size)
{
	char *temp = arecalloc(va, *alen + blen, size);
	memcpy(temp + *alen * size, vb, blen * size);
	*alen += blen;
	return temp;
}
