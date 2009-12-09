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
 * Implementation and management of foreign keys. Foreign keys are references
 * from a relation R's attribute a to a relation S's primary-indexed attribute
 * b. a and b must have the same domain.
 * In my terminology, S is the foreign (key) relation (fgn_rl) and R is 
 * the reference relation (ref_rl).
 * Then, when a tuple is deleted from S and this tuple has value x in b,
 * all tuples from R with value x in a are deleted, too. And when a tuple
 * of R is updated and this update changed the value in b from x to y,
 * all tuples in S with value x in a are updated to value y in a.
 */

#ifndef __FGNKEY_H__
#define __FGNKEY_H__

#include "io.h"
#include <stdbool.h>

void init_reftable(struct srel *fgn_rl);

bool create_foreign_key(struct srel *fgn_rl, struct sattr *fgn_attr, 
		struct srel *ref_rl, struct sattr *attr);

/* Drops all relations that have `fgn_rl' as foreign key. */
bool drop_references(struct srel *fgn_rl);

/* Removes all reference entries to `ref_rl' of all foreign keys of `ref_rl'.
 * Remember that this does not affect any tuples, only relation header
 * entries. */
bool remove_references_to(struct srel *ref_rl);

/* Checks whether the data integrity would be given after an insertion or 
 * an update. This means that if an attribute `a' of `rl' is an foreign 
 * key this foreign relation must contain a tuple with same value like 
 * `new_tuple' in `a'. */
bool foreign_key_conflict(struct srel *fgn_rl, const char *new_tuple);

/* Updates all references to `fgn_rl', i.e. all relations that have 
 * `fgn_rl' as foreign key. If `new_tuple' changes an attribute `a's value
 * from `v' to `w' and if a referencing relation contains one or more tuples `t'
 * with the value `v' in the referencing attribute, `v' is changed to `w'.
 * Note that this function directly calls rl_update()/ix_insert()/ix_delete()
 * and does not use rlmngt.c's functions; this avoids conflicts with 
 * foreign_key_conflict(). */
bool update_references(struct srel *fgn_rl, const char *new_tuple,
		const char *old_tuple, tpcnt_t *tpcnt);

/* Deletes from all references to `fgn_rl', i.e. all relations that have 
 * `fgn_rl' as foreign key. If `tuple' has an attribute `a' with the value
 * `v' and if a referencing relation has tuples with the value `v' in the
 * referencing attribute, these tuples are deleted.
 * Note that this function directly calls rl_update()/ix_insert()/ix_delete()
 * and does not use rlmngt.c's functions; this avoids conflicts with 
 * foreign_key_conflict(). */
bool delete_references(struct srel *fgn_rl, const char *tuple, tpcnt_t *tpcnt);

#endif

