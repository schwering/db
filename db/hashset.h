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
 * Closed hashset implementation.
 */

#ifndef __HASHSET_H__
#define __HASHSET_H__

#include <stdbool.h>

/** Closed hashset using the square hashing method. */
struct hashset {
	/** The table of entries in the set. */
	struct hs_entry *tab;
	/** The capacity of the set. */
	int size;
	/** The percentage until which the set is filled without resizing. */
	float loadfactor;
	/** Stores a value for internal calculations (loadfactor * size).  */
	int threshold;
	/** The number of elements in the set. */
	int used;
	/** Creates a hashvalue of a value. */
	int (*hashf)(void *p);
	/** Interprates two elements as equal. */
	bool (*equalsf)(void *p, void *q);
};

/** One entry of the set. */
struct hs_entry {
	int status;
	int hashcode;
	void *val;
};

/** Creates a new hashset with a given initial capacity. The default loadfactor
 * is 0.75. */
struct hashset *hashset_init(int size, int (*hashf)(void *p),
		bool (*equalsf)(void *p, void *q));

/** Frees the memory used for the hashset. Note that this does not free the 
 * elements itselves. */
void hashset_free(struct hashset *set);

/** Inserts a new elements. No double entries are allowed. */
void *hashset_insert(struct hashset *set, void *val);

/** Deletes a entry from the set. */
bool hashset_delete(struct hashset *set, void *val);

/** Indicates whether the set contains an element. */
bool hashset_contains(struct hashset *set, void *val);

/** Returns an array of pointers to the elements of the set. */
void **hashset_entries(struct hashset *set);

#endif

