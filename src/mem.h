/*
 * mem.h
 *
 * This file is part of awl
 */

#pragma once

#include <stddef.h>

#define alloct(T) (acalloc(1, sizeof(T)))

void *acalloc(size_t nmemb, size_t size);
void *arecalloc(void *ptr, size_t nmemb, size_t size);
void afree(void *ptr);
