/*
 * mem.c
 *
 * This file is part of awl
 */

#include "mem.h"

#include <stdlib.h>
#include "err.h"

void *acalloc(size_t nmemb, size_t size)
{
	void *temp = calloc(nmemb, size);

	if (!temp) {
		afree(temp);
		err_internal("could not allocate");
	}

	return temp;
}

void *arecalloc(void *ptr, size_t nmemb, size_t size)
{
	void *temp = realloc(ptr, nmemb * size);

	if (!temp) {
		afree(temp);
		err_internal("could not reallocate");
	}

	return temp;
}

void afree(void *ptr)
{
	free(ptr);
	ptr = NULL;
}
