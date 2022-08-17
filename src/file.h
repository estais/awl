/*
 * file.h
 *
 * This file is part of awl
 */

#pragma once

#include <stddef.h>

typedef struct File {
	const char *path;
	char **lines;
	size_t nlines;
} File;

File *file_new(const char *path);
