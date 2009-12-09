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

#include "arraylist.h"
#include "mem.h"
#include <stdlib.h>
#include <string.h>

static void resize(struct alist *list)
{
	if (list->used >= (int)(list->loadfactor * list->size)) {
		list->size = 2 * list->size;
		list->table = xrealloc(list->table,
				list->size * sizeof(void *));
	}
}

struct alist *al_init(int size)
{
	struct alist *list;

	list = xmalloc(sizeof(struct alist));
	list->table = xmalloc(size * sizeof(void *));
	list->used = 0;
	list->size = size;
	list->loadfactor = 0.75;
	return list;
}

struct alist *al_init_gc(int size, mid_t id)
{
	struct alist *list;

	list = gmalloc(sizeof(struct alist), id);
	list->table = gmalloc(size * sizeof(void *), id);
	list->used = 0;
	list->size = size;
	list->loadfactor = 0.75;
	return list;
}

void al_freef(struct alist *list, void (*freef)(void *))
{
	if (list != NULL) {
		if (list->table != NULL) {
			int i;
			for (i = 0; i < list->used; i++)
				if (freef != NULL)
					freef(list->table[i]);
			free(list->table);
		}
		free(list);
	}
}

void al_free(struct alist *list)
{
	al_freef(list, free);
}

void *al_get(struct alist *list, int index)
{
	return (index >= 0 && index < list->used)
		? list->table[index]
		: NULL;
}

void al_insert(struct alist *list, int i, void *val)
{
	int j;

	list->used++;
	resize(list);

	for (j = list->used-1; j > i; j--)
		list->table[j] = list->table[j-1];
	list->table[list->used-1] = val;
}

void al_insert_and_free(struct alist *list, int i, void *val)
{
	al_insert(list, i, val);
	free(val);
}

void al_append(struct alist *list, void *val)
{
	al_insert(list, list->used, val);
}

void al_append_and_free(struct alist *list, void *val)
{
	al_insert_and_free(list, list->used, val);
}

void al_merge(struct alist *l1, struct alist *l2)
{
	int i;

	for (i = 0; i < l2->used; i++)
		al_append(l1, al_get(l2, i));
	al_free(l2);
}

