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
 * Input/output core utilities for the relation file.
 * All tuples of a relation (= table) have a fixed size which is a multiple 
 * of BLK_SIZE. Tuples are stored in a single file, a so-called relation data 
 * file.
 * Each tuple has a status byte (which marks it either as deleted or active), 
 * a pointer to the next tuple and a pointer to the previous tuple. 
 * This makes a set of tuples form a double linked list. In fact, all 
 * active tuples form a double linked list and all deleted tuples form another 
 * double linked list.
 * Pointers to tuples are their addresses or indexes in the file. The first 
 * index is 0, which is the address of the first tuple, which is positioned 
 * directly after the relation data file header. The equivalent of NULL 
 * pointers in the context of tuple addresses is INVALID_ADDR = -1.
 * The mentioned relation data file header is a struct header, aligned to a 
 * multiple of BLK_SIZE. This header contains important information like the 
 * tuple size and the first nodes of the double linked lists of active 
 * respectively deleted tuples.
 * When a relation is closed explicitly, this header is written to the relation 
 * file and thus kept up to date. While this makes sense, omitting it does 
 * not destroy the relation, because the database rebuilds those header 
 * information that got out of sync with the database.
 */

#ifndef __IO_H__
#define __IO_H__

#include "block.h"
#include "constants.h"
#include "hashtable.h"
#include <stdbool.h>
#include <sys/types.h>

#define RL_NAME_MAX	63	/* relation's name size */
#define ATTR_MAX	30	/* max. attribute count */
#define AT_NAME_MAX	31	/* attribute name size */
#define REF_MAX		3	/* max. count of references to a relation */
#define FKEY_MAX	3	/* max. count of foreign keys of a relation */

#define NOT_INDEXED	0	/* attribute not indexed */
#define PRIMARY		1	/* primary index (no double values allowed) */
#define SECONDARY	2	/* secondary index (double values allowed) */

struct sattr { /* a stored relation attribute */
	enum domain	at_domain;		/* content type */
	char		at_name[AT_NAME_MAX+1];	/* name */
	size_t		at_size;		/* size in byte */
	size_t		at_offset;		/* byte offset in tuple */
	unsigned short	at_indexed;		/* NOT_INDEXED, PRIMARY,
						 * SECONDARY */
};

struct sref {
	char		rf_refrl[RL_NAME_MAX+1];/* name of other relation */
	unsigned short	rf_refattr;		/* index of other's rl attr */
	unsigned short	rf_thisattr;		/* index of owning attr */
};

struct srel_hdr { /* information container for a relation */
	char		hd_name[RL_NAME_MAX+1];	/* relation name */
	unsigned short	hd_atcnt;		/* count of attributes */
	struct sattr	hd_attrs[ATTR_MAX];	/* attributes */
	off_t		hd_asize;		/* aligned size of header */
	size_t		hd_tpsize;		/* status+addr+tuple size */
	size_t		hd_tpasize;		/* tpsize aligned to BLK_SIZE */
	tpcnt_t		hd_tpcnt;		/* count of tuples in table */
	blkaddr_t	hd_tpmax;		/* maximum address in table */
	blkaddr_t	hd_tplatest;		/* latest inserted tuple */
	blkaddr_t	hd_tpavail;		/* latest deleted tuple */
	struct sref	hd_fkeys[FKEY_MAX];	/* true if rl has fgn key */
	unsigned short	hd_fkeycnt;		/* count of foreign keys */
	struct sref	hd_refs[REF_MAX];	/* references to this rl */
	unsigned short	hd_refcnt;		/* count of references */
	bool		hd_rlclosed;		/* relation closed properly? */
};

struct srel { /* a stored relation */
	char			rl_name[PATH_MAX+1];	/* path to data file */
	int			rl_fd;			/* file descriptor */
	struct srel_hdr		rl_header;		/* table header */
	char			*rl_tpbuf;		/* aligned tuple buf */
	struct cache		*rl_cache;		/* tuple LRU cache */
	struct hashtable	*rl_ixtable;		/* index table */
};

struct srel_iter { /* sequential database iterator */
	struct srel	*it_rl;			/* owning relation */
	blkaddr_t	it_curaddr;		/* current tuple address */
	char		*it_tpbuf;		/* aligned tuple buffer */
};

/* Create a new stored relation. Returns a pointer to the relation on success, 
 * NULL otherwise. */
struct srel *rl_create(struct srel *rl);

/* Open an existing relation. Returns a pointer to the relation on success, 
 * NULL otherwise. */
struct srel *rl_open(struct srel *rl);

/* Updates the header of a stored relation. For example, this function must 
 * be invoked if an attribute has been indexed (and struct sattr's at_indexed
 * field has changed. */
bool rl_write_header(struct srel *rl);

/* Close a relation. Very important to keep the header up to date. */
bool rl_close(struct srel *rl);

/* Delete a tuple at a given address. */
bool rl_delete(struct srel *rl, blkaddr_t addr);

/* Update the data at a given tuple address. */
bool rl_update(struct srel *rl, blkaddr_t addr, const char *tp_data);

/* Insert a new tuple at the next available address and returns this address. */
blkaddr_t rl_insert(struct srel *rl, const char *tp_data);

/* Returns the tuple data at a given tuple address. */
const char *rl_get(struct srel *rl, blkaddr_t addr);

/* Creates a relation itator. */
struct srel_iter *rl_iterator(struct srel *rl);

/* Frees an iterator structure and its buffer. */
void srel_iter_free(struct srel_iter *iter);

/* Resets a relation iterator to beginning. */
void rl_iterator_reset(struct srel_iter *iter);

/* Iterate over the relation tuples. */
const char *rl_next(struct srel_iter *it);

#endif

