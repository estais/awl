/*
 * vec.h
 *
 * This file is part of awl
 */

#pragma once

#include <stddef.h>

#define vec_push(vec, elem, length, size) (vec = _vec_push(vec, elem, length, size))
void *_vec_push(void *vec, void *elem, size_t *length, size_t size);

#define vec_join(va, vb, alen, blen, size) (va = _vec_join(va, vb, alen, blen, size))
void *_vec_join(void *va, void *vb, size_t *alen, size_t blen, size_t size);
