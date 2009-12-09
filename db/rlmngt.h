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
 * Relation management functions.
 * The functions defined here wrap the rl_open(), rl_create(), rl_close()
 * and also the rl_insert(), rl_delete(), rl_update() functions of io.h.
 * The functions in this file take care that no relation file is opened 
 * twice, the indexes are opened and that the indexes are kept up to date.
 */

#ifndef __RLMNGT_H__
#define __RLMNGT_H__

#include "io.h"

/* Creates a new relation with a given name and attributes. Returns a
 * pointer to the opened relation structure or NULL. */
struct srel *create_relation(const char *name, const struct sattr *attrs,
		int atcnt);

/* Opens a relation and returns a pointer to a relation structure or NULL. */
struct srel *open_relation(const char *name);

/* Closes a relation. */
void close_relation(struct srel *rl);

/* Closes all open relation. */
void close_relations(void);

/* Deletes all files belonging to a relation. */
bool drop_relation(const char *name);

/* Inserts a new tuple into the relation and keeps the indexes up to date. */
bool insert_into_relation(struct srel *rl, const char *tuple);

/* Updates a tuple in the relation and keeps the indexes up to date. */
bool update_relation(struct srel *rl, blkaddr_t addr, const char *old_tuple,
		const char *new_tuple, tpcnt_t *tpcnt);

/* Deletes a tuple from the relation and keeps the indexes up to date. */
bool delete_from_relation(struct srel *rl, blkaddr_t addr, const char *tuple,
		tpcnt_t *tpcnt);

#endif

