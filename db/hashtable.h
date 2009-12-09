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
 * Closed hashtable implementation.
 */

#ifndef __HASHTABLE_H__
#define __HASHTABLE_H__

#include <stdbool.h>

/* Closed hashtable using the square hashing method. A table consists of a 
 * key element that correspondends with a value. */
struct hashtable {
	/* The table of entries in the table. */
	struct ht_entry *tab;
	/* The capacity of the table. */
	int size;
	/* The percentage until which the table is filled without resizing. */
	float loadfactor;
	/* Stores a value for internal calculations (loadfactor * size). */
	int threshold;
	/* The number of elements in the table. */
	int used;
	/* Creates a hashvalue of a key. */
	int (*hashf)(void *p);
	/* Interprates two elements as equal. */
	bool (*equalsf)(void *p, void *q);
};

/* One entry of the map. */
struct ht_entry {
	int status;
	int hashcode;
	void *key;
	void *val;
};

/* Creates a new hashtable with a given initial capacity. The default 
 * loadfactor is 0.75. */
struct hashtable *table_init(int size, int (*hashf)(void *p),
		bool (*equalsf)(void *p, void *q));

/* Frees the memory used for the hashtable. Note that this does not free the 
 * keys and values itselves. */
void table_free(struct hashtable *table);

/* Inserts a new key / value. No double keys are allowed. */
void *table_insert(struct hashtable *table, void *key, void *val);

/* Deletes an key / value pair from the table. */
void *table_delete(struct hashtable *table, void *key);

/* Returns a pointer to the key `key' in the table. */
void *table_keyptr(struct hashtable *table, void *key);

/* Indicates whether the table contains a given key. */
void *table_search(struct hashtable *table, void *key);

/* Returns an array of pointers to the keys in the table. */
void **table_keys(struct hashtable *table);

/* Returns an array of pointers to the values in the table. */
void **table_entries(struct hashtable *table);

#endif

