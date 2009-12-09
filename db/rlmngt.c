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

#include "rlmngt.h"
#include "constants.h"
#include "err.h"
#include "fgnkey.h"
#include "io.h"
#include "ixmngt.h"
#include "hashtable.h"
#include "mem.h"
#include "str.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

static struct hashtable *table = NULL;

static void init_rltable(void)
{
	if (table == NULL) {
		table = table_init(7, (int (*)(void *))strhash,
				(bool (*)(void *, void *))strequals);
		assert(table != NULL);
	}
}

static void free_rltable(void)
{
	if (table != NULL) {
		table_free(table);
		table = NULL;
	}
}

static void rl_mkfn(char *filename, const char *name)
{
	int len;

	assert(filename != NULL);
	assert(name != NULL);
	assert(strlen(DB_BASEDIR) + strlen(name) + strlen(DB_SUFFIX)
			<= PATH_MAX);

	strcpy(filename, DB_BASEDIR);
	len = strlen(DB_BASEDIR);
	strcpy(filename+len, name);
	len += strlen(name);
	strcpy(filename+len, DB_SUFFIX);
}

struct srel *create_relation(const char *name, const struct sattr *attrs,
		int atcnt)
{
	struct srel *rl;
	char filename[PATH_MAX+1];
	int i;

	if (strlen(name) > RL_NAME_MAX)
		return NULL;

	rl_mkfn(filename, name);
	rl = xmalloc(sizeof(struct srel));
	if (rl == NULL)
		return NULL;

	strntermcpy(rl->rl_name, filename, PATH_MAX+1);
	strntermcpy(rl->rl_header.hd_name, name, RL_NAME_MAX+1);
	for (i = 0; i < atcnt; i++)
		rl->rl_header.hd_attrs[i] = attrs[i];
	rl->rl_header.hd_atcnt = i;
	rl->rl_tpbuf = NULL;
	rl->rl_cache = NULL;
	rl->rl_ixtable = NULL;

	if (rl_create(rl)) {
		bool ok;

		if (table == NULL)
			init_rltable();
		init_ixtable(rl);
		table_insert(table, rl->rl_header.hd_name, rl);

		ok = true;
		for (i = 0; i < rl->rl_header.hd_atcnt; i++)
			if (rl->rl_header.hd_attrs[i].at_indexed)
				ok &= create_index(rl,
						&rl->rl_header.hd_attrs[i],
						PRIMARY)
					!= NULL;

		if (!ok) {
			drop_relation(name);
			return NULL;
		} else
			return rl;
	} else {
		free(rl);
		return NULL;
	}
}

struct srel *open_relation(const char *name)
{
	struct srel *rl;
	char filename[PATH_MAX+1];

	if (strlen(name) > RL_NAME_MAX)
		return NULL;

	rl_mkfn(filename, name);
	if (table == NULL)
		init_rltable();

	rl = (struct srel *)table_search(table, (void *)name); 
	if (rl != NULL && rl->rl_fd != -1 && rl->rl_tpbuf != NULL)
		return rl;
	if (rl != NULL && rl->rl_fd == -1 && rl->rl_tpbuf != NULL)
		free(rl->rl_tpbuf);
	
	if (rl == NULL) {
		rl = xmalloc(sizeof(struct srel));
		if (rl == NULL)
			return NULL;
		rl->rl_tpbuf = NULL;
		rl->rl_cache = NULL;
		rl->rl_ixtable = NULL;
	}
	if (rl == NULL)
		return NULL;

	strntermcpy(rl->rl_name, filename, RL_NAME_MAX+1);

	if (rl_open(rl)) {
		init_ixtable(rl);
		open_indexes(rl);
		table_insert(table, rl->rl_header.hd_name, rl);
		return rl;
	} else {
		free(rl);
		return NULL;
	}
}

void close_relation(struct srel *rl)
{
	if (table == NULL)
		init_rltable();

	close_indexes(rl);
	table_free(rl->rl_ixtable);

	table_delete(table, rl->rl_header.hd_name);
	rl_close(rl);
	if (table->used == 0)
		free_rltable();
}

void close_relations(void)
{
	struct srel **relations;
	int i;

	if (table == NULL)
		return;

	relations = (struct srel **)table_entries(table);
	for (i = 0; relations[i]; i++)
		close_relation(relations[i]);
	free(relations);
}

bool drop_relation(const char *name)
{
	struct srel *rl;
	char buf[PATH_MAX+1];

	assert(name != NULL);

	rl = open_relation(name);
	if (rl == NULL)
		return false;

	remove_references_to(rl);
	drop_references(rl);
	drop_indexes(rl);

	strntermcpy(buf, rl->rl_name, PATH_MAX+1);
	close_relation(rl);
	return unlink(buf) == 0;
}

bool insert_into_relation(struct srel *rl, const char *tuple)
{
	blkaddr_t addr;

	assert(rl != NULL);
	assert(tuple != NULL);

	if (primary_key_conflict(rl, tuple, NULL)) {
		ERR(E_PRIMARY_KEY_CONFLICT);
		return false;
	}

	if (foreign_key_conflict(rl, tuple)) {
		ERR(E_FOREIGN_KEY_CONFLICT);
		return false;
	}

	if ((addr = rl_insert(rl, tuple)) == INVALID_ADDR)
		return false;

	if (!insert_into_indexes(rl, NULL, addr, tuple)) {
		ERR(E_INDEX_INSERT_FAILED);
		return false;
	}

	return true;
}

bool update_relation(struct srel *rl, blkaddr_t addr, const char *old_tuple,
		const char *new_tuple, tpcnt_t *tpcnt)
{
	int i;
	bool mod[ATTR_MAX];

	assert(rl != NULL);
	assert(addr != INVALID_ADDR);
	assert(old_tuple != NULL);
	assert(new_tuple != NULL);
	assert(tpcnt != NULL);

	if (primary_key_conflict(rl, new_tuple, old_tuple)) {
		ERR(E_PRIMARY_KEY_CONFLICT);
		return false;
	}

	if (foreign_key_conflict(rl, new_tuple)) {
		ERR(E_FOREIGN_KEY_CONFLICT);
		return false;
	}

	for (i = 0; i < rl->rl_header.hd_atcnt; i++) {
		size_t offset, size;

		offset = rl->rl_header.hd_attrs[i].at_offset;
		size = rl->rl_header.hd_attrs[i].at_size;
		mod[i] = memcmp(old_tuple+offset, new_tuple+offset, size) != 0;
	}

	if (!delete_from_indexes(rl, mod, addr, old_tuple)) {
		ERR(E_INDEX_DELETE_FAILED);
		return false;
	}

	if (!insert_into_indexes(rl, mod, addr, new_tuple)) {
		ERR(E_INDEX_INSERT_FAILED);
		return false;
	}

	if (!update_references(rl, old_tuple, new_tuple, tpcnt)) {
		ERR(E_FGNKEY_DELETE_FAILED);
		return false;
	}

	if (!rl_update(rl, addr, new_tuple)) {
		ERR(E_TUPLE_UPDATE_FAILED);
		return false;
	}

	(*tpcnt)++;
	return true;
}

bool delete_from_relation(struct srel *rl, blkaddr_t addr, const char *tuple,
		tpcnt_t *tpcnt)
{
	assert(rl != NULL);
	assert(addr != INVALID_ADDR);
	assert(tuple != NULL);
	assert(tpcnt != NULL);

	if (!delete_from_indexes(rl, NULL, addr, tuple)) {
		ERR(E_INDEX_DELETE_FAILED);
		return false;
	}

	if (!delete_references(rl, tuple, tpcnt)) {
		ERR(E_FGNKEY_DELETE_FAILED);
		return false;
	}

	if (!rl_delete(rl, addr)) {
		ERR(E_TUPLE_DELETE_FAILED);
		return false;
	}

	(*tpcnt)++;
	return true;
}

