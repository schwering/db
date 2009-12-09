/*
 * Copyright (c) 2006, 2007 Christoph Schwering <schwering@gmail.com> *
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
 * Simple implementation of a a linked list. Not pointers are stored (in
 * contrast to arraylist.h), but copies of the values (whose sizes are 
 * specified in list_init()).
 */

#ifndef __LINKEDLIST_H__
#define __LINKEDLIST_H__

#include "mem.h"
#include <stdbool.h>
#include <stddef.h>

struct llentry {
	void *val;
	struct llentry *prev;
	struct llentry *next;
	bool markdel;
};

struct llist {
	struct llentry *first;	/* first element in list */
	size_t size;		/* size of a single element */
	size_t cnt;		/* count of elements */
	mid_t id;		/* memory id (or -1) */
};

struct llist *ll_init(size_t elem_size);
struct llist *ll_init_gc(size_t elem_size, mid_t id);
void ll_add(struct llist *ll, void *val);
void ll_del(struct llist *ll, struct llentry *e);
void ll_markdel(struct llist *ll, struct llentry *e);
size_t ll_delmarked(struct llist *ll);
void *ll_search(struct llist *ll, void *val);
void ll_free(struct llist *ll);

#endif

