/*
 * err.h
 *
 * This file is part of awl
 */

#pragma once

#define err_internal(fmt, ...) (_err_internal(__FILE__, __LINE__, fmt, ##__VA_ARGS__))

void _err_internal(const char *filename, int line, const char *fmt, ...);
void err_user(const char *fmt, ...);
