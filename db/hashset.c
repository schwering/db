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

#include "hashset.h"
#include "mem.h"
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#define S_EMPTY		0x00000001
#define S_DELETED	0x00000010
#define S_USED		0x00000100
#define S_VISITED	0x00001000

static bool resize(struct hashset *set, int size);

static void reset_visited(struct hashset *set, int count, int hashcode);

static inline int rehash(int hashcode,  int i, int size);

static inline int rehash_linear(int hashcode,  int i, int size);

static inline int rehash_square(int hashcode,  int i, int size);

struct hashset *hashset_init(int size, int (*hashf)(void *p),
		bool (*equalsf)(void *p, void *q))
{
	int i;
	struct hashset *set;
	struct hs_entry *e;

	set = xmalloc(sizeof(struct hashset));
	if (set == NULL)
		return NULL;

	set->tab = xmalloc(size * sizeof(struct hs_entry));
	if (set->tab == NULL) {
		free(set);
		return NULL;
	}

	for (i = 0; i < size; i++) {
		e = &set->tab[i];
		e->status = S_EMPTY;
		e->val = NULL;
	}

	set->size = size;
	set->loadfactor = 0.75f;
	set->threshold = (int)(set->loadfactor * set->size);
	set->used = 0;
	set->hashf = hashf;
	set->equalsf = equalsf;
	return set;
}

static bool resize(struct hashset *set, int size)
{
	struct hashset *tmp;
	struct hs_entry *e;
	int i;

	tmp = hashset_init(size, set->hashf, set->equalsf);
	if (!tmp)
		return false;
	for (i = 0; i < set->size; i++) {
		e = &set->tab[i];
		if (e->status == S_USED)
			hashset_insert(tmp, e->val);
	}
	free(set->tab);
	set->tab = tmp->tab;
	set->size = tmp->size;
	set->threshold = (int)(set->loadfactor * set->size);
	set->used = tmp->used;
	free(tmp);
	return true;
}

void hashset_free(struct hashset *set)
{
	if (set != NULL) {
		if (set->tab != NULL)
			free(set->tab);
		free(set);
	}
}

void *hashset_insert(struct hashset *set, void *val)
{
	int hashcode, i, j, visited;
	struct hs_entry *e;
	void *old_val;

	hashcode = set->hashf(val);

	for (i = visited = 0; visited < set->used; i++) {
		j = rehash(hashcode, i, set->size);
		e = &set->tab[j];
		if (e->status == S_EMPTY) {
			break;
		} else if (e->status == S_USED
				&& hashcode == e->hashcode
				&& set->equalsf(val, e->val)) {
			old_val = e->val;
			e->val = val;
			reset_visited(set, visited, hashcode);
			return old_val;
		} else if ((e->status & S_VISITED) == 0) {
			e->status |= S_VISITED;
			visited++;
		}
	}
	reset_visited(set, visited, hashcode);
	
	if (set->used >= set->threshold) 
		if (!resize(set, 2 * set->size + 1))
			return NULL;

	for (i = 0; ; i++) {
		j = rehash(hashcode, i, set->size);
		e = &set->tab[j];
		if (e->status != S_USED) {
			e->status = S_USED;
			e->hashcode = hashcode;
			e->val = val;
			set->used++;
			return NULL;
		}
	}
	return NULL;
}

bool hashset_delete(struct hashset *set, void *val)
{
	int hashcode, i, j, visited;
	struct hs_entry *e;

	hashcode = set->hashf(val);

	for (i = visited = 0; visited < set->size; i++) {
		j = rehash(hashcode, i, set->size);
		e = &set->tab[j];
		if (e->status == S_EMPTY) {
			reset_visited(set, visited, hashcode);
			return false;
		} else if (e->status == S_USED 
				&& hashcode == e->hashcode
				&& set->equalsf(val, e->val)) {
			e->status = S_DELETED;
			set->used--;
			reset_visited(set, visited, hashcode);
			return true;
		} else if ((e->status & S_VISITED) == 0) {
			e->status |= S_VISITED;
			visited++;
		}
	}
	reset_visited(set, visited, hashcode);
	return false;
}

bool hashset_contains(struct hashset *set, void *val)
{
	int hashcode, i, j, visited;
	struct hs_entry *e;

	hashcode = set->hashf(val);

	for (i = visited = 0; visited < set->size; i++) {
		j = rehash(hashcode, i, set->size);
		e = &set->tab[j];
		if (e->status == S_EMPTY) {
			reset_visited(set, visited, hashcode);
			return NULL;
		} else if (e->status == S_USED
				&& hashcode == e->hashcode
				&& set->equalsf(val, e->val)) {
			reset_visited(set, visited, hashcode);
			return true;
		} else if ((e->status & S_VISITED) == 0) {
			e->status |= S_VISITED;
			visited++;
		}
	}
	reset_visited(set, visited, hashcode);
	return false;
}

void **hashset_entries(struct hashset *set)
{
	int i, j;
	void **arr;
	struct hs_entry *e;

	if (set == NULL)
		return NULL;

	arr = xmalloc((set->used + 1) * sizeof(void *));
	for (i = j = 0; i < set->size; i++) {
		e = &set->tab[i];
		if (e->status == S_USED)
			arr[j++] = e->val;
	}
	arr[j] = NULL;
	return arr;
}

static void reset_visited(struct hashset *set, int count, int hashcode)
{
	int i, j;
	struct hs_entry *e;

	for (i = 0; count > 0; i++) {
		j = rehash(hashcode, i, set->size);
		e = &set->tab[j];
		if ((e->status & S_VISITED) != 0) {
			e->status &= ~S_VISITED;
			count--;
		}
	}
}

static inline int rehash(int hashcode,  int i, int size)
{
	int limit, r;
	
	limit = (size - 1) / 2;
	if (i < limit)
		r = rehash_square(hashcode, i, size);
	else
		r = rehash_linear(hashcode, i, size);
	return (r < 0) ? -r : r;
}

static inline int rehash_linear(int hashcode, int i, int size)
{
	return (hashcode + i) % size;
}

static inline int rehash_square(int hashcode, int i, int size)
{
	if (i == 0) 
		return hashcode % size;

	if (i % 2 == 1) {
		i = (i+1) / 2;
		i *= i;
	} else {
		i /= 2;
		i *= i;
		i *= -1;
	}
	return (hashcode + i) % size;
}

