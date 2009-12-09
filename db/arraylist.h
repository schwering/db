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
 * Simple implementation of a a resizable array of void pointers.
 */

#ifndef __ARRAYLIST_H__
#define __ARRAYLIST_H__

#include "mem.h"
#include <stdbool.h>
#include <sys/types.h>

struct alist {
	void **table;
	int used;
	int size;
	float loadfactor;
	size_t (*sizef)(void *val);
};

struct alist *al_init(int size);
struct alist *al_init_gc(int size, mid_t id);
void al_freef(struct alist *list, void (*freef)(void *));
void al_free(struct alist *list);
void *al_get(struct alist *list, int index);
void al_insert(struct alist *list, int i, void *val);
void al_insert_and_merge(struct alist *list, int i, void *val);
void al_append(struct alist *list, void *val);
void al_append_and_free(struct alist *list, void *val);
void al_merge(struct alist *l1, struct alist *l2);

#endif

