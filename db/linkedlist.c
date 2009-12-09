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

#include "linkedlist.h"
#include "mem.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

struct llist *ll_init(size_t elem_size)
{
	struct llist *ll;

	ll = xmalloc(sizeof(struct llist));
	ll->first = NULL;
	ll->size = elem_size;
	ll->cnt = 0;
	ll->id = -1;
	return ll;
}

struct llist *ll_init_gc(size_t elem_size, mid_t id)
{
	struct llist *ll;

	ll = gmalloc(sizeof(struct llist), id);
	ll->first = NULL;
	ll->size = elem_size;
	ll->cnt = 0;
	ll->id = id;
	return ll;
}

void ll_add(struct llist *ll, void *val)
{
	struct llentry *e;

	assert(ll != NULL);
	assert(val != NULL);

	if (ll->id != -1) {
		e = gmalloc(sizeof(struct llentry), ll->id);
		e->val = gmalloc(ll->size, ll->id);
	} else {
		e = xmalloc(sizeof(struct llentry));
		e->val = xmalloc(ll->size);
	}
	memcpy(e->val, val, ll->size);
	e->markdel = true;

	e->prev = NULL;
	e->next = ll->first;

	if (ll->first != NULL)
		ll->first->prev = e;

	ll->first = e;
	ll->cnt++;
}

void ll_del(struct llist *ll, struct llentry *e)
{
	assert(ll != NULL);
	assert(e != NULL);

	if (ll->first == e)
		ll->first = e->next;
	if (e->prev != NULL)
		e->prev->next = e->next;
	if (e->next != NULL)
		e->next->prev = e->prev;

	if (ll->id != -1) {
		gfree(e->val, ll->id);
		gfree(e, ll->id);
	} else {
		free(e->val);
		free(e);
	}

	ll->cnt--;
}

void ll_markdel(struct llist *ll, struct llentry *e)
{
	assert(ll != NULL);
	assert(e != NULL);

	e->markdel = false;
}

size_t ll_delmarked(struct llist *ll)
{
	struct llentry *e, *next;
	size_t cnt;

	assert(ll != NULL);

	cnt = 0;
	next = ll->first;
	while ((e = next) != NULL) {
		next = e->next;

		if (e->markdel == false) {
			ll_del(ll, e);
			cnt++;
		}
	}
	return cnt;
}

void *ll_search(struct llist *ll, void *val)
{
	struct llentry *e;

	for (e = ll->first; e != NULL; e = e->next)
		if (!memcmp(val, e->val, ll->size))
			return e;
	return NULL;
}

void ll_free(struct llist *ll)
{
	while (ll->first != NULL)
		ll_del(ll, ll->first);
	if (ll->id != -1)
		gfree(ll, ll->id);
	else
		free(ll);
}

