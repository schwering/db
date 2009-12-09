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
 * Index management functions.
 * The functions defined here wrap the ix_open(), ix_create() and ix_close()
 * functions of btree.h. Besides invoking these functions, the index 
 * management functions open_index(), create_index(), close_index()
 * remember indexes that are already open. This caching mechanism avoids 
 * opening a indexes twice at once.
 */

#ifndef __IXMNGT_H__
#define __IXMNGT_H__

#include "btree.h"
#include "io.h"

/* Initializes a table's relation structure. */
void init_ixtable(struct srel *rl);

/* Creates a new index of a given relation and an attribute. The index is 
 * registered in the relation's ixtable. The type must be either 
 * PRIMARY or SECONDARY as they're defined in io.h. */
struct index *create_index(struct srel *rl, struct sattr *attr, int type);

/* Opens a specified index of a relation. The index is registered in the 
 * relation's ixtable. */
struct index *open_index(struct srel *rl, struct sattr *attr);

/* Opens all existing indexes of a relation. The index is registered in the
 * relation's ixtable. */
void open_indexes(struct srel *rl);

/* Closes a specified index of a relation. */
void close_index(struct srel *rl, struct sattr *attr);

/* Closes all indexes of a relation. */
void close_indexes(struct srel *rl);

/* Removes the index file belonging to the index of `attr' of `rl'. */
bool drop_index(struct srel *rl, struct sattr *attr);

/* Removes all files belonging to any indexes of a given relation. */
bool drop_indexes(struct srel *rl);

/* Determines whether there is a primary index conflict, i.e. that there 
 * already is a tuple whose value in an primary indexed attribute is the same
 * as in `new_tuple'. This check is only performed if `new_tuple' and 
 * `old_tuple' differ in this attribute (that's an update case) or if
 * `old_tuple' is NULL (that's an insertion case). */
bool primary_key_conflict(struct srel *rl, const char *new_tuple,
		const char *old_tuple);

/* Synchronisation a INSERT operation on all indexes of a relation. */
bool insert_into_indexes(struct srel *rl, bool sattrs[],
		blkaddr_t addr, const char *tuple);

/* Synchronisation a DELETE operation on all indexes of a relation. */
bool delete_from_indexes(struct srel *rl, bool attrs[],
		blkaddr_t addr, const char *tuple);

/* Returns an iterator that searches for tuple addresses that match `key' in 
 * relation with `compar'. */
struct ix_iter *search_in_index(struct srel *rl, struct sattr *attr,
		int compar, const char *key);

/* Returns the index iterator "next one, please" function that belongs to 
 * compar. This is either ix_next_left (LEQ, LT), ix_next_right (GEQ, GT)
 * or ix_next (EQ). (Moving the iterator to the right position is done in 
 * search_in_index().  */
blkaddr_t (*index_iterator_nextf(int compar))(struct ix_iter *);

#endif

