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

#include "ixmngt.h"
#include "attr.h"
#include "btree.h"
#include "constants.h"
#include "err.h"
#include "hashtable.h"
#include "io.h"
#include "mem.h"
#include "parser.h"
#include "rlmngt.h"
#include "str.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

static int hashf(const struct sattr *attr)
{
	assert(attr != NULL);
	assert(attr->at_name != NULL);

	return strhash(attr->at_name);
}

static bool attrequals(const struct sattr *a1, const struct sattr *a2)
{
	assert(a1 != NULL);
	assert(a2 != NULL);

	return a1 == a2;
}

void init_ixtable(struct srel *rl)
{
	assert(rl != NULL);

	if (rl->rl_ixtable == NULL) {
		rl->rl_ixtable = table_init(3, (int (*)(void *))hashf,
				(bool (*)(void *, void *))attrequals);
		assert(rl->rl_ixtable != NULL);
	}
}

static void ix_mkfn(char *filename, const struct srel *rl,
		const struct sattr *attr)
{
	int len;

	assert(filename != NULL);
	assert(rl != NULL);
	assert(attr != NULL);
	assert(strlen(IX_BASEDIR) + strlen(rl->rl_header.hd_name)
			+ strlen(IX_DELIM) + strlen(attr->at_name)
			+ strlen(IX_SUFFIX) <= PATH_MAX);

	strcpy(filename, IX_BASEDIR);
	len = strlen(IX_BASEDIR);
	strcpy(filename+len, rl->rl_header.hd_name);
	len += strlen(rl->rl_header.hd_name);
	strcpy(filename+len, IX_DELIM);
	len += strlen(IX_DELIM);
	strcpy(filename+len, attr->at_name);
	len += strlen(attr->at_name);
	strcpy(filename+len, IX_SUFFIX);
}

struct index *create_index(struct srel *rl, struct sattr *attr, int type)
{
	char ix_name[PATH_MAX+1];
	size_t ix_size;
	cmpf_t cmpf;
	struct index *ix;
	char at_indexed_old;

	assert(rl != NULL);
	assert(attr != NULL);
	assert(type == PRIMARY || type == SECONDARY);

	ix_mkfn(ix_name, rl, attr);
	ix_size = attr->at_size;
	if (type == SECONDARY)
		ix_size += sizeof(blkaddr_t);

	/* the next four lines: first, we set at_indexed to the future value,
	 * so that ixcmpf_by_sattr() returns the right comparison function; 
	 * aftercalling ixcmpf_by_sattr() we reset it to the old value again,
	 * because in case of failure the attribute should not be marked as
	 * indexed!
	 * marking the attribute as indexed and updating the relation's header
	 * is done later (only in case of success, of course) */
	at_indexed_old = attr->at_indexed;
	attr->at_indexed = type;
	cmpf = ixcmpf_by_sattr(attr);
	attr->at_indexed = at_indexed_old;

	ix = ix_create(ix_name, ix_size, cmpf);
	if (ix != NULL) {
		struct srel_iter *iter;
		blkaddr_t addr;
		const char *tuple;
		char data[ix_size];
		bool b;

		table_insert(rl->rl_ixtable, attr, ix);

		iter = rl_iterator(rl);
		assert(iter != NULL);
		while ((tuple = rl_next(iter))) {
			addr = iter->it_curaddr;
			memcpy(data, tuple + attr->at_offset, attr->at_size);
			memcpy(data + attr->at_size, &addr, sizeof(blkaddr_t));
			b = ix_insert(ix, addr, data);
			assert(b == true);
		}
		srel_iter_free(iter);

		attr->at_indexed = type;
		b = rl_write_header(rl);
		assert(b == true);

		return ix;
	} else
		return NULL;
}

struct index *open_index(struct srel *rl, struct sattr *attr)
{
	char ix_name[PATH_MAX+1];
	struct index *ix;

	assert(rl != NULL);
	assert(attr != NULL);

	if (attr->at_indexed != PRIMARY && attr->at_indexed != SECONDARY)
		return NULL;

	ix_mkfn(ix_name, rl, attr);

	if ((ix = table_search(rl->rl_ixtable, attr)) != NULL)
		return ix;
	else if ((ix = ix_open(ix_name, ixcmpf_by_sattr(attr))) != NULL) {
		table_insert(rl->rl_ixtable, attr, ix);
		return ix;
	} else
		return NULL;
}

void open_indexes(struct srel *rl)
{
	int i;

	assert(rl != NULL);

	for (i = 0; i < rl->rl_header.hd_atcnt; i++)
		open_index(rl, &rl->rl_header.hd_attrs[i]);
}

void close_index(struct srel *rl, struct sattr *attr)
{
	struct index *ix;

	assert(rl != NULL);
	assert(attr != NULL);

	if ((ix = table_search(rl->rl_ixtable, attr)) != NULL) {
		table_delete(rl->rl_ixtable, attr);
		ix_close(ix);
	}
}

void close_indexes(struct srel *rl)
{
	int i;

	assert(rl != NULL);

	for (i = 0; i < rl->rl_header.hd_atcnt; i++)
		if (rl->rl_header.hd_attrs[i].at_indexed != NOT_INDEXED)
			close_index(rl, &rl->rl_header.hd_attrs[i]);
}

bool drop_index(struct srel *rl, struct sattr *attr)
{
	char buf[PATH_MAX+1];
	struct index *ix;
	bool retval;

	assert(rl != NULL);
	assert(attr != NULL);

	retval = true;
	if ((ix = open_index(rl, attr)) != NULL) {
		strntermcpy(buf, ix->ix_name, PATH_MAX+1);
		close_index(rl, attr);
		retval &= unlink(buf) == 0;
	}
	return retval;
}

bool drop_indexes(struct srel *rl)
{
	bool retval;
	int i;

	assert(rl != NULL);

	retval = true;
	for (i = 0; i < rl->rl_header.hd_atcnt; i++)
		if (rl->rl_header.hd_attrs[i].at_indexed != NOT_INDEXED)
			retval &= drop_index(rl, &rl->rl_header.hd_attrs[i]);
	return retval;
}

bool primary_key_conflict(struct srel *rl, const char *new_tuple,
		const char *old_tuple)
{
	struct sattr *attr;
	struct index *ix;
	const char *key;
	int i;

	assert(rl != NULL);
	assert(new_tuple != NULL);

	for (i = 0; i < rl->rl_header.hd_atcnt; i++) {
		attr = &rl->rl_header.hd_attrs[i];
		if (attr->at_indexed != PRIMARY)
			continue;

		ix = open_index(rl, attr);
		if (ix == NULL)
			continue;

		if (old_tuple != NULL && !memcmp(new_tuple + attr->at_offset,
					old_tuple + attr->at_offset,
					attr->at_size))
			continue;

		key = new_tuple + attr->at_offset;
		if (ix_search(ix, key) != INVALID_ADDR)
			return true;
	}
	return false;
}

static size_t calc_max_key_size(struct srel *rl, bool *attrs)
{
	size_t size, max;
	int i;

	assert(rl != NULL);

	max = 0;
	for (i = 0; i < rl->rl_header.hd_atcnt; i++) {
		if (rl->rl_header.hd_attrs[i].at_indexed == NOT_INDEXED)
			continue;

		size = rl->rl_header.hd_attrs[i].at_size;
		if (rl->rl_header.hd_attrs[i].at_indexed != PRIMARY)
			size += sizeof(blkaddr_t);

		if ((attrs == NULL || attrs[i] == true) && max < size)
			max = size;
	}
	return max;
}

bool insert_into_indexes(struct srel *rl, bool attrs[],
		blkaddr_t addr, const char *tuple)
{
	int i;
	struct index *ix;
	char data[calc_max_key_size(rl, attrs)];
	struct sattr *attr;
	bool retval;

	assert(rl != NULL);
	assert(addr != INVALID_ADDR);
	assert(tuple != NULL);

	retval = true;
	for (i = 0; i < rl->rl_header.hd_atcnt; i++) {
		if (attrs != NULL && !attrs[i])
			continue;

		attr = &rl->rl_header.hd_attrs[i];
		if (attr->at_indexed == NOT_INDEXED)
			continue;

		ix = open_index(rl, attr);
		if (ix == NULL)
			continue;

#ifndef NDEBUG
		if (attr->at_indexed == PRIMARY)
			assert(ix->ix_size == attr->at_size);
		else if (attr->at_indexed == SECONDARY)
			assert(ix->ix_size == attr->at_size+sizeof(blkaddr_t));
#endif

		memcpy(data, tuple + attr->at_offset, attr->at_size);
		if (attr->at_indexed == SECONDARY)
			memcpy(data + attr->at_size, &addr, sizeof(blkaddr_t));

		retval &= ix_insert(ix, addr, data);
	}
	return retval;
}

bool delete_from_indexes(struct srel *rl, bool attrs[],
		blkaddr_t addr, const char *tuple)
{
	int i;
	struct index *ix;
	char data[calc_max_key_size(rl, attrs)];
	struct sattr *attr;
	bool retval;

	assert(rl != NULL);
	assert(tuple != NULL);

	retval = true;
	for (i = 0; i < rl->rl_header.hd_atcnt; i++) {
		if (attrs != NULL && !attrs[i])
			continue;

		attr = &rl->rl_header.hd_attrs[i];
		if (attr->at_indexed == NOT_INDEXED)
			continue;

		ix = open_index(rl, attr);
		if (ix == NULL)
			continue;

#ifndef NDEBUG
		if (attr->at_indexed == PRIMARY)
			assert(ix->ix_size == attr->at_size);
		else if (attr->at_indexed == SECONDARY)
			assert(ix->ix_size == attr->at_size+sizeof(blkaddr_t));
#endif

		memcpy(data, tuple + attr->at_offset, attr->at_size);
		if (attr->at_indexed == SECONDARY)
			memcpy(data + attr->at_size, &addr, sizeof(blkaddr_t));

		retval &= (ix_delete(ix, data) != INVALID_ADDR);
	}
	return retval;
}

struct ix_iter *search_in_index(struct srel *rl, struct sattr *attr,
		int compar, const char *key)
{
	struct index *ix;
	struct ix_iter *iter;

	assert(attr != NULL);
	assert(key != NULL);
	assert(attr->at_indexed == PRIMARY || attr->at_indexed == SECONDARY);

	ix = open_index(rl, attr);
	assert(ix != NULL);

	if (attr->at_indexed == PRIMARY) {
		assert(ix->ix_size == attr->at_size);

		iter = ix_iterator(ix, key);
		return iter;
	} else {
		char buf[attr->at_size + sizeof(blkaddr_t)];
		blkaddr_t invalid_addr;

		assert(ix->ix_size == attr->at_size + sizeof(blkaddr_t));

		invalid_addr = INVALID_ADDR;
		memcpy(buf, key, attr->at_size);
		memcpy(buf + attr->at_size, &invalid_addr, sizeof(blkaddr_t));
		iter = ix_iterator(ix, buf);
		return iter;
	}
}

static blkaddr_t next_leq(struct ix_iter *iter)
{
	/* imagine this scenario and a search <= 3 request: 
	 * values: 1 2 3 3 3 3 4 5 6 
	 *             A
	 *             |
	 *     iterator position
	 * we will move the iterator to the right to scan all 3s first, 
	 * and then go on at the left of the first 3. */
	if (iter->it_curcmpval == 0) {
		blkaddr_t addr;

		if ((addr = ix_next(iter)) != INVALID_ADDR)
			return addr;
		ix_reset(iter); /* kakadu! */
	}
	return ix_lnext(iter);
}

static blkaddr_t next_gt(struct ix_iter *iter)
{
	blkaddr_t addr;

	/* imagine this scenario and a search > 3 request: 
	 * values: 1 2 3 3 3 3 4 5 6 
	 *             A
	 *             |
	 *     iterator position
	 * we will skip all 3s and immediately return the first 4. */
	while ((addr = ix_rnext(iter)) != INVALID_ADDR
			&& iter->it_curcmpval == 0)
		;
	return addr;
}

blkaddr_t (*index_iterator_nextf(int compar))(struct ix_iter *)
{
	switch (compar) {
		case LT:	return ix_lnext;
		case LEQ:	return next_leq;
		case GT:	return next_gt;
		case GEQ:	return ix_rnext;
		case EQ:	return ix_next;
		default:	return NULL;
	}
}

