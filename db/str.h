/*
 * Copyright (c) 2006, 2007 Christoph Schwering <schwering@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Simple string functions and other utilities.
 */

#ifndef __STR_H__
#define __STR_H__

#include "mem.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>

/* Returns the number of bytes needed for a string, i.e. strlen() + 1. */
size_t strsize(const char *str);

/* Copies a maximum of n-1 bytes from src to dest and null-terminates dest. */
char *strntermcpy(char *dest, const char *src, size_t n);

/* Returns true, if the s1 equals s2. */
bool strequals(const char *s1, const char *s2);

/* Calculates a hashvalue of a string (used for hashtables). */
int strhash(const char *s);

/* Concatenates `cnt' strings in a newly malloc()ed string. */
char *cat(int cnt, ...);

/* Behaves same like cat(), but uses gmalloc() with the specified id for
 * memory allocation. */
char *cat_gc(mid_t id, int cnt, ...);

/* Copies `size' bytes from ptr to newly malloc()ed memory and returns a 
 * pointer to it. */
void *copy(const void *ptr, size_t size);

/* Behaves same like copy(), but uses gmalloc() with the specified id for
 * memory allocation. */
void *copy_gc(const void *ptr, size_t size, mid_t id);

#endif

