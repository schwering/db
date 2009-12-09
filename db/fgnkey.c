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

#include "fgnkey.h"
#include "btree.h"
#include "io.h"
#include "ixmngt.h"
#include "rlmngt.h"
#include "str.h"
#include <assert.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

bool create_foreign_key(struct srel *fgn_rl, struct sattr *fgn_attr, 
		struct srel *ref_rl, struct sattr *ref_attr)
{
	struct index *ref_ix;
	struct sref *ref;
	short ref_attr_index, fgn_attr_index;
	bool success;
	int i;

	assert(fgn_rl != NULL);
	assert(fgn_attr != NULL);
	assert(ref_rl != NULL);
	assert(ref_attr != NULL);
	assert(fgn_rl != ref_rl);
	assert(fgn_attr->at_domain == ref_attr->at_domain);
	assert(fgn_attr->at_size == ref_attr->at_size);
	assert(ref_rl->rl_header.hd_fkeycnt < FKEY_MAX);

	if (fgn_rl->rl_header.hd_refcnt == REF_MAX)
		return false;

	ref_attr_index = -1;
	for (i = 0; i < ref_rl->rl_header.hd_atcnt; i++) {
		if (&ref_rl->rl_header.hd_attrs[i] == ref_attr) {
			ref_attr_index = i;
			break;
		}
	}
	assert(ref_attr_index != -1);

	fgn_attr_index = -1;
	for (i = 0; i < fgn_rl->rl_header.hd_atcnt; i++) {
		if (&fgn_rl->rl_header.hd_attrs[i] == fgn_attr) {
			fgn_attr_index = i;
			break;
		}
	}
	assert(fgn_attr_index != -1);

	if ((ref_ix = create_index(ref_rl, ref_attr, SECONDARY)) == NULL)
		return false;

	/* update fgn_rl's references */
	ref = &fgn_rl->rl_header.hd_refs[fgn_rl->rl_header.hd_refcnt];
	strntermcpy(ref->rf_refrl, ref_rl->rl_header.hd_name, RL_NAME_MAX+1);
	ref->rf_refattr = ref_attr_index;
	ref->rf_thisattr = fgn_attr_index;
	fgn_rl->rl_header.hd_refcnt++;
	success = rl_write_header(fgn_rl);
	assert(success);

	/* update ref_rl's foreign key */
	ref = &ref_rl->rl_header.hd_fkeys[ref_rl->rl_header.hd_fkeycnt];
	strntermcpy(ref->rf_refrl, fgn_rl->rl_header.hd_name, RL_NAME_MAX+1);
	ref->rf_refattr = fgn_attr_index;
	ref->rf_thisattr = ref_attr_index;
	ref_rl->rl_header.hd_fkeycnt++;
	success = rl_write_header(ref_rl);
	assert(success);

	return true;
}

bool drop_references(struct srel *fgn_rl)
{
	struct sref *ref;
	int i;
	bool success, retval;

	assert(fgn_rl != NULL);

	retval = true;
	/* this must be done from refcnt to zero, because drop_relation 
	 * invokes remove_reference_to(), which again modifies
	 * fgn_rl->rl_header.hd_refs and removes the references to the 
	 * dropped table and shifts the remaining references one down if
	 * it isn't the last */
	for (i = fgn_rl->rl_header.hd_refcnt - 1; i >= 0; i--) {
		ref = &fgn_rl->rl_header.hd_refs[i];
		retval &= drop_relation(ref->rf_refrl);
	}
	fgn_rl->rl_header.hd_refcnt = 0;
	success = rl_write_header(fgn_rl);
	assert(success);
	return retval;
}

bool remove_references_to(struct srel *ref_rl)
{
	struct sref *ref;
	struct srel *fgn_rl;
	int i, j, k;
	bool success;

	assert(ref_rl != NULL);

	for (i = 0; i < ref_rl->rl_header.hd_fkeycnt; i++) {
		ref = &ref_rl->rl_header.hd_fkeys[i];

		fgn_rl = open_relation(ref->rf_refrl);
		if (fgn_rl == NULL)
			return false;
		for (j = 0; j < fgn_rl->rl_header.hd_refcnt; j++) {
			if (!strncmp(fgn_rl->rl_header.hd_refs[j].rf_refrl,
						ref_rl->rl_header.hd_name,
						RL_NAME_MAX))
				break;
		}

		if (j < fgn_rl->rl_header.hd_refcnt) {
			for (k = j; k+1 < fgn_rl->rl_header.hd_refcnt; k++)
				fgn_rl->rl_header.hd_refs[k]
					= fgn_rl->rl_header.hd_refs[k+1];
			fgn_rl->rl_header.hd_refcnt--;
			success = rl_write_header(fgn_rl);
			assert(success);
		}
	}

	ref_rl->rl_header.hd_fkeycnt = 0;
	success = rl_write_header(ref_rl);
	assert(success);
	return true;
}

bool foreign_key_conflict(struct srel *ref_rl, const char *tuple)
{
	struct sref *ref;
	struct srel *fgn_rl;
	struct sattr *fgn_attr, *ref_attr;
	struct index *fgn_ix;
	const char *key;
	blkaddr_t addr;
	int i;

	assert(ref_rl != NULL);
	assert(tuple != NULL);

	/* Note that in rl->rl_header.hd_fgnkey, the rf_refrl and rf_refattr
	 * mean the foreign key relation and the foreign key's attribute */
	for (i = 0; i < ref_rl->rl_header.hd_fkeycnt; i++) {
		ref = &ref_rl->rl_header.hd_fkeys[i];

		ref_attr = &ref_rl->rl_header.hd_attrs[ref->rf_thisattr];
		key = tuple + ref_attr->at_offset;

		fgn_rl = open_relation(ref->rf_refrl);
		fgn_attr = &fgn_rl->rl_header.hd_attrs[ref->rf_refattr];
		assert(fgn_attr->at_indexed == PRIMARY);

		fgn_ix = open_index(fgn_rl, fgn_attr);
		assert(fgn_ix != NULL);
		addr = ix_search(fgn_ix, key);
		if (addr == INVALID_ADDR)
			return true;
	}
	return false;
}

static bool updrefs(struct srel *ref_rl, struct sattr *ref_attr,
		const char *old_val, const char *new_val, size_t size,
		tpcnt_t *tpcnt)
{
	struct index *ref_ix;
	char old_key[size+sizeof(blkaddr_t)], new_key[size+sizeof(blkaddr_t)];
	const char *old_tuple;
	char new_tuple[ref_rl->rl_header.hd_tpsize];
	blkaddr_t addr, invalid_addr;
	bool retval;

	assert(ref_rl != NULL);
	assert(ref_attr != NULL);
	assert(old_val != NULL);
	assert(new_val != NULL);
	assert(tpcnt != NULL);

	ref_ix = open_index(ref_rl, ref_attr);
	assert(ref_ix != NULL);

	invalid_addr = INVALID_ADDR;
	memcpy(old_key, old_val, size);
	memcpy(old_key + size, &invalid_addr, sizeof(blkaddr_t));
	memcpy(new_key, new_val, size);

	retval = true;
	while ((addr = ix_search(ref_ix, old_key)) != INVALID_ADDR) {
		old_tuple = rl_get(ref_rl, addr);
		memcpy(new_tuple, old_tuple, ref_rl->rl_header.hd_tpsize);
		memcpy(new_tuple + ref_attr->at_offset, new_val, size);
		retval &= update_relation(ref_rl, addr, old_tuple, new_tuple,
				tpcnt);
	}
	return retval;
}

bool update_references(struct srel *fgn_rl, const char *old_tuple,
		const char *new_tuple, tpcnt_t *tpcnt)
{
	bool retval;
	int i;

	assert(fgn_rl != NULL);
	assert(old_tuple != NULL);
	assert(new_tuple != NULL);
	assert(tpcnt != NULL);

	retval = true;
	for (i = 0; i < fgn_rl->rl_header.hd_refcnt; i++) {
		struct sref *ref;
		struct srel *ref_rl;
		struct sattr *fgn_attr, *ref_attr;
		const char *old_val, *new_val;
		size_t size;
		
		ref = &fgn_rl->rl_header.hd_refs[i];

		fgn_attr = &fgn_rl->rl_header.hd_attrs[ref->rf_thisattr];
		assert(fgn_attr->at_indexed == PRIMARY);
		ref_rl = open_relation(ref->rf_refrl);
		assert(ref_rl != NULL);
		ref_attr = &ref_rl->rl_header.hd_attrs[ref->rf_refattr];
		assert(ref_attr->at_indexed == SECONDARY);

		old_val = old_tuple + fgn_attr->at_offset;
		new_val = new_tuple + fgn_attr->at_offset;
		size = fgn_attr->at_size;

		if (memcmp(old_val, new_val, size) == 0)
			continue;

		retval &= updrefs(ref_rl, ref_attr, old_val, new_val, size,
				tpcnt);
	}
	return retval;
}

static bool delrefs(struct srel *ref_rl, struct sattr *ref_attr,
		const char *val, size_t size, tpcnt_t *tpcnt)
{
	struct index *ref_ix;
	char key[size + sizeof(blkaddr_t)];
	blkaddr_t addr, invalid_addr;
	const char *tuple;
	bool retval;

	assert(ref_rl != NULL);
	assert(ref_attr != NULL);
	assert(val != NULL);
	assert(tpcnt != NULL);

	ref_ix = open_index(ref_rl, ref_attr);
	assert(ref_ix != NULL);

	invalid_addr = INVALID_ADDR;
	memcpy(key, val, size);
	memcpy(key + size, &invalid_addr, sizeof(blkaddr_t));

	retval = true;
	while ((addr = ix_search(ref_ix, key)) != INVALID_ADDR) {
		tuple = rl_get(ref_rl, addr);
		retval &= delete_from_relation(ref_rl, addr, tuple, tpcnt);
	}
	return retval;
}

bool delete_references(struct srel *fgn_rl, const char *tuple, tpcnt_t *tpcnt)
{
	bool retval;
	int i;

	assert(fgn_rl != NULL);
	assert(tuple != NULL);
	assert(tpcnt != NULL);

	retval = true;
	for (i = 0; i < fgn_rl->rl_header.hd_refcnt; i++) {
		struct sref *ref;
		struct srel *ref_rl;
		struct sattr *fgn_attr, *ref_attr;
		const char *val;
		size_t size;
		
		ref = &fgn_rl->rl_header.hd_refs[i];

		fgn_attr = &fgn_rl->rl_header.hd_attrs[ref->rf_thisattr];
		assert(fgn_attr->at_indexed == PRIMARY);
		ref_rl = open_relation(ref->rf_refrl);
		assert(ref_rl != NULL);
		ref_attr = &ref_rl->rl_header.hd_attrs[ref->rf_refattr];
		assert(ref_attr->at_indexed == SECONDARY);

		val = tuple + fgn_attr->at_offset;
		size = fgn_attr->at_size;

		retval &= delrefs(ref_rl, ref_attr, val, size, tpcnt);
	}
	return retval;
}

