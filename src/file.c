/*
 * file.c
 *
 * This file is part of awl
 */

#include "file.h"

#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include "vec.h"
#include "mem.h"
#include "err.h"

File *file_new(const char *path)
{
	File *file = alloct(File);
	file->path = path;
	file->lines = NULL;
	file->nlines = 0;

	int fd = -1;
	if ((fd = open(path, O_RDONLY)) == -1) {
		err_user("no such file '%s'", path);
	}

	struct stat st;
	if (fstat(fd, &st) == -1) {
		err_user("bad stat on file '%s'", path);
	}

	if (!S_ISREG(st.st_mode)) {
		err_user("not a regular file '%s'", path);
	}

	char *map = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	char *copy = strndup(map, st.st_size);

	char *line = strtok(copy, "\n");
	while (line) {
		vec_push(file->lines, &line, &file->nlines, sizeof(char *));
		line = strtok(NULL, "\n");
	}

	afree(line);
	munmap(map, st.st_size);
	close(fd);

	return file;
}
