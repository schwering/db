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

#include "str.h"
#include "mem.h"
#include <assert.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

size_t strsize(const char *str)
{
	return strlen(str) + 1;
}

char *strntermcpy(char *dest, const char *src, size_t n)
{
	strncpy(dest, src, n-1);
	dest[n-1] = '\0';
	return dest;
}

int strhash(const char *s)
{
	int r;

	r = 0;
	for (; *s; s++)
		r += (*s) * (*s);
	return r;
}

bool strequals(const char *s1, const char *s2)
{
	while (*s1 && *s1 == *s2)
		s1++, s2++;
	return *s1 == '\0' && *s2 == '\0';
}

char *cat(int cnt, ...)
{
	va_list ap;
	const char *strs[cnt];
	char *str, *cur;
	int len, i;

	len = 0;
	va_start(ap, cnt);
	for (i = 0; i < cnt; i++) {
		strs[i] = va_arg(ap, const char *);
		len += strlen(strs[i]);
	}
	va_end(ap);

	str = xmalloc(len + 1);
	cur = str;
	for (i = 0; i < cnt; i++) {
		if (strs[i] != NULL) {
			while ((*(cur++) = *(strs[i]++)))
				;
			cur--;
		}
	}
	return str;
}

char *cat_gc(mid_t id, int cnt, ...)
{
	va_list ap;
	const char *strs[cnt];
	char *str, *cur;
	int len, i;

	len = 0;
	va_start(ap, cnt);
	for (i = 0; i < cnt; i++) {
		strs[i] = va_arg(ap, const char *);
		len += strlen(strs[i]);
	}
	va_end(ap);

	str = gmalloc(len + 1, id);
	cur = str;
	for (i = 0; i < cnt; i++) {
		if (strs[i] != NULL) {
			while ((*(cur++) = *(strs[i]++)))
				;
			cur--;
		}
	}
	return str;
}

void *copy(const void *ptr, size_t size)
{
	void *cp;

	assert(ptr != NULL);

	cp = xmalloc(size);
	memcpy(cp, ptr, size);
	return cp;
}

void *copy_gc(const void *ptr, size_t size, mid_t id)
{
	void *cp;

	assert(ptr != NULL);

	cp = gmalloc(size, id);
	memcpy(cp, ptr, size);
	return cp;
}

