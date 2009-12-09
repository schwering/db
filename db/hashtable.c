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

#include "hashtable.h"
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#define S_EMPTY		0x00000001
#define S_DELETED	0x00000010
#define S_USED		0x00000100
#define S_VISITED	0x00001000

static bool resize(struct hashtable *table, int size);

static void reset_visited(struct hashtable *table, int count, int hashcode);

static inline int rehash(int hashcode,  int i, int size);

static inline int rehash_linear(int hashcode,  int i, int size);

static inline int rehash_square(int hashcode,  int i, int size);

struct hashtable *table_init(int size,
		int (*hashf)(void *p), bool (*equalsf)(void *p, void *q))
{
	int i;
	struct hashtable *table;
	struct ht_entry *e;

	table = malloc(sizeof(struct hashtable));
	if (table == NULL)
		return NULL;

	table->tab = malloc(size * sizeof(struct ht_entry));
	if (table->tab == NULL) {
		free(table);
		return NULL;
	}

	for (i = 0; i < size; i++) {
		e = &table->tab[i];
		e->status = S_EMPTY;
		e->key = NULL;
		e->val = NULL;
	}

	table->size = size;
	table->loadfactor = 0.75f;
	table->threshold = (int)(table->loadfactor * table->size);
	table->used = 0;
	table->hashf = hashf;
	table->equalsf = equalsf;
	return table;
}

static bool resize(struct hashtable *table, int size)
{
	struct hashtable *tmp;
	struct ht_entry *e;
	int i;

	tmp = table_init(size, table->hashf, table->equalsf);
	if (!tmp)
		return false;
	for (i = 0; i < table->size; i++) {
		e = &table->tab[i];
		if (e->status == S_USED)
			table_insert(tmp, e->key, e->val);
	}
	free(table->tab);
	table->tab = tmp->tab;
	table->size = tmp->size;
	table->threshold = (int)(table->loadfactor * table->size);
	table->used = tmp->used;
	free(tmp);
	return true;
}

void table_free(struct hashtable *table)
{
	if (table != NULL) {
		if (table->tab != NULL)
			free(table->tab);
		free(table);
	}
}

void *table_insert(struct hashtable *table, void *key, void *val)
{
	int hashcode, i, j, visited;
	struct ht_entry *e;
	void *old_val;

	hashcode = table->hashf(key);

	for (i = visited = 0; visited < table->used; i++) {
		j = rehash(hashcode, i, table->size);
		e = &table->tab[j];
		if (e->status == S_EMPTY) {
			break;
		} else if (e->status == S_USED
				&& hashcode == e->hashcode
				&& table->equalsf(key, e->key)) {
			old_val = e->val;
			e->val = val;
			reset_visited(table, visited, hashcode);
			return old_val;
		} else if ((e->status & S_VISITED) == 0) {
			e->status |= S_VISITED;
			visited++;
		}
	}
	reset_visited(table, visited, hashcode);
	
	if (table->used >= table->threshold) 
		if (!resize(table, 2 * table->size + 1))
			return NULL;

	for (i = 0; ; i++) {
		j = rehash(hashcode, i, table->size);
		e = &table->tab[j];
		if (e->status != S_USED) {
			e->status = S_USED;
			e->hashcode = hashcode;
			e->key = key;
			e->val = val;
			table->used++;
			return NULL;
		}
	}
	return NULL;
}

void *table_delete(struct hashtable *table, void *key)
{
	int hashcode, i, j, visited;
	struct ht_entry *e;

	hashcode = table->hashf(key);

	for (i = visited = 0; visited < table->size; i++) {
		j = rehash(hashcode, i, table->size);
		e = &table->tab[j];
		if (e->status == S_EMPTY) {
			reset_visited(table, visited, hashcode);
			return NULL;
		} else if (e->status == S_USED 
				&& hashcode == e->hashcode
				&& table->equalsf(key, e->key)) {
			e->status = S_DELETED;
			table->used--;
			reset_visited(table, visited, hashcode);
			return e->val;
		} else if ((e->status & S_VISITED) == 0) {
			e->status |= S_VISITED;
			visited++;
		}
	}
	reset_visited(table, visited, hashcode);
	return NULL;
}

void *table_keyptr(struct hashtable *table, void *key)
{
	int hashcode, i, j, visited;
	struct ht_entry *e;

	hashcode = table->hashf(key);

	for (i = visited = 0; visited < table->size; i++) {
		j = rehash(hashcode, i, table->size);
		e = &table->tab[j];
		if (e->status == S_EMPTY) {
			reset_visited(table, visited, hashcode);
			return NULL;
		} else if (e->status == S_USED
				&& hashcode == e->hashcode
				&& table->equalsf(key, e->key)) {
			reset_visited(table, visited, hashcode);
			return e->key;
		} else if ((e->status & S_VISITED) == 0) {
			e->status |= S_VISITED;
			visited++;
		}
	}
	reset_visited(table, visited, hashcode);
	return NULL;
}

void *table_search(struct hashtable *table, void *key)
{
	int hashcode, i, j, visited;
	struct ht_entry *e;

	hashcode = table->hashf(key);

	for (i = visited = 0; visited < table->size; i++) {
		j = rehash(hashcode, i, table->size);
		e = &table->tab[j];
		if (e->status == S_EMPTY) {
			reset_visited(table, visited, hashcode);
			return NULL;
		} else if (e->status == S_USED
				&& hashcode == e->hashcode
				&& table->equalsf(key, e->key)) {
			reset_visited(table, visited, hashcode);
			return e->val;
		} else if ((e->status & S_VISITED) == 0) {
			e->status |= S_VISITED;
			visited++;
		}
	}
	reset_visited(table, visited, hashcode);
	return NULL;
}

void **table_keys(struct hashtable *table)
{
	int i, j;
	void **arr;
	struct ht_entry *e;

	if (table == NULL)
		return NULL;

	arr = malloc((table->used + 1) * sizeof(void *));
	for (i = j = 0; i < table->size; i++) {
		e = &table->tab[i];
		if (e->status == S_USED)
			arr[j++] = e->key;
	}
	arr[j] = NULL;
	return arr;
}

void **table_entries(struct hashtable *table)
{
	int i, j;
	void **arr;
	struct ht_entry *e;

	if (table == NULL)
		return NULL;

	arr = malloc((table->used + 1) * sizeof(void *));
	for (i = j = 0; i < table->size; i++) {
		e = &table->tab[i];
		if (e->status == S_USED)
			arr[j++] = e->val;
	}
	arr[j] = NULL;
	return arr;
}

static void reset_visited(struct hashtable *table, int count, int hashcode)
{
	int i, j;
	struct ht_entry *e;

	for (i = 0; count > 0; i++) {
		j = rehash(hashcode, i, table->size);
		e = &table->tab[j];
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

