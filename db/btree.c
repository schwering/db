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

#include "btree.h"
#include "block.h"
#include "cache.h"
#include "constants.h"
#include "mem.h"
#include "str.h"
#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* the maximum size of the database cache */
#define TOTAL_CACHE_SIZE	(1024 * 10)

/* minimum order of tree */
#define MIN_ORDER		11

/* different node types (accessible through TYPE) */
#define AVAIL			0
#define INNER			1
#define LEAF			2

/* size of a address/key pair */
#define ENTRY_SIZE(ix)		(sizeof(blkaddr_t) + (ix)->ix_size)

/* the order of a tree, i.e. the maximum number of keys in a node */
#define ORDER(ix)		((ix)->ix_order)

#define TYPE_OFFSET		(0)
#define PREV_DEL_OFFSET		(TYPE_OFFSET + sizeof(char))
#define CNT_OFFSET		(TYPE_OFFSET + sizeof(char))
#define LNBR_OFFSET		(CNT_OFFSET + sizeof(short))
#define RNBR_OFFSET		(LNBR_OFFSET + sizeof(blkaddr_t))
#define BLK_OFFSET		(RNBR_OFFSET + sizeof(blkaddr_t))

/* type of node: either INNER, LEAF or AVAIL */
#define TYPE(buf)		(*(char *)(buf + TYPE_OFFSET))

/* the previous element in list of deleted blocks */
#define PREV_DEL(buf)		(*(blkaddr_t *)(buf + PREV_DEL_OFFSET))

/* count of keys stored in a node */
#define CNT(buf)		(*(short *)(buf + CNT_OFFSET))

/* left neighbor of node */
#define LNBR(buf)		(*(blkaddr_t *)(buf + LNBR_OFFSET))

/* right neighbor of node */
#define RNBR(buf)		(*(blkaddr_t *)(buf + RNBR_OFFSET))

/* the address of the i-th child of a node */
#define PTR(ix, buf, i)		(*(blkaddr_t *)((buf) + BLK_OFFSET	       \
				+ (i) * ENTRY_SIZE(ix)			       \
				+ (assert(i < CNT(buf)),0) ))

/* a pointer to the i-th key of a node */
#define KEY(ix, buf, i)		((char *)((buf) + BLK_OFFSET		       \
				+ (i) * ENTRY_SIZE(ix) + sizeof(blkaddr_t)     \
				+ (assert(i < CNT(buf)),0) ))

/* copies the key 'src' to 'dest' */
#define KEYCPY(ix, dest, src)	memcpy(dest, src, (ix)->ix_size)

/* calls key comparison function */
#define CMPF(ix, v1, v2)	((ix)->ix_cmpf((v1), (v2), (ix)->ix_size))

/* converts a block address (blkaddr_t) to a file position (off_t) */
#define ADDR_TO_POS(ix, addr)	(((off_t)(addr) * (ix)->ix_blksize + BLK_SIZE))

/* moves the file cursor to a the (absolute) position (off_t) */
#define GOTO_POS(ix, pos)	lseek((ix)->ix_fd, pos, SEEK_SET)

/* moves the file cursor to the address (blkaddr_t) */
#define GOTO_ADDR(ix, addr)	GOTO_POS((ix), ADDR_TO_POS((ix),(addr)))

/* fills a buffer from a given offset to another given position with zeros */
#define FILL_BUF(buf, from, to)	memset((buf)+from, 0, (to)-(from))

/* reads a given amount of bytes from a file into a pointer */
#define READ(fd, ptr, size)	((bool)(read(fd,ptr,size) == (ssize_t)(size)))

/* writes a given amount of bytes from a pointer to a file */
#define WRITE(fd, ptr, size)	((bool)(write(fd,ptr,size) == (ssize_t)(size)))

struct ix_header {
	size_t		ix_size;	/* a key's size */
	blkaddr_t	ix_root;	/* root address */
	blkaddr_t	ix_max;		/* last addressed block */
	blkaddr_t	ix_avail;	/* last deleted block */
	bool		ix_closed;	/* do we need to rebuild_header()? */
};

static inline bool ix_read(struct index *ix, blkaddr_t addr, char *buf)
{
	bool retval;

#ifndef NO_CACHE
	if (cache_search(ix->ix_cache, addr, buf))
		return true;
#endif

	GOTO_ADDR(ix, addr);
	retval = READ(ix->ix_fd, buf, ix->ix_blksize);
#ifndef NO_CACHE
	if (retval)
		cache_push(ix->ix_cache, addr, buf);
#endif
	return retval;
}

static inline bool ix_write(struct index *ix, blkaddr_t addr, const char *buf)
{
	bool retval;

	assert(addr == ix->ix_root || TYPE(buf) == AVAIL
			|| CNT(buf) >= ORDER(ix)/2);
	assert(addr != INVALID_ADDR);

	GOTO_ADDR(ix, addr);
	retval = WRITE(ix->ix_fd, buf, ix->ix_blksize);
#ifndef NO_CACHE
	if (retval)
		cache_update(ix->ix_cache, addr, 0, buf, ix->ix_blksize);
#endif
	return retval;
}

static blkaddr_t alloc_blk(struct index *ix)
{
	if (ix->ix_avail == INVALID_ADDR)  {
		return ++ix->ix_max;
	} else {
		blkaddr_t addr;
		char buf[ix->ix_blksize];

		addr = ix->ix_avail;
		if (!ix_read(ix, ix->ix_avail, buf))
			return INVALID_ADDR;
		ix->ix_avail = PREV_DEL(buf);
		return addr;
	}
}

static void free_blk(struct index *ix, blkaddr_t addr)
{
	char buf[ix->ix_blksize];

	assert(addr != INVALID_ADDR);
	assert(addr <= ix->ix_max);

	memset(buf, 0, ix->ix_size);
	PREV_DEL(buf) = ix->ix_avail;
	TYPE(buf) = AVAIL;
	if (!ix_write(ix, addr, buf))
		return;
	ix->ix_avail = addr;
}

static bool set_lnbr(struct index *ix, blkaddr_t addr, blkaddr_t lnbr_addr)
{
	char buf[ix->ix_blksize];

	assert(addr <= ix->ix_max);
	assert(lnbr_addr <= ix->ix_max);

	if (addr == INVALID_ADDR)
		return true;

	if (!ix_read(ix, addr, buf))
		return false;
	LNBR(buf) = lnbr_addr;
	if (!ix_write(ix, addr, buf))
		return false;
	return true;
}

static void rebuild_header(struct index *ix, struct ix_header *hd)
{
	blkaddr_t addr;
	char buf[ix->ix_blksize];

	hd->ix_avail = INVALID_ADDR;
	for (addr = 0; ix_read(ix, addr, buf); addr++) {
		hd->ix_max = addr;
		if (TYPE(buf) != AVAIL) {
			if (LNBR(buf) == INVALID_ADDR
					&& RNBR(buf) == INVALID_ADDR)
				hd->ix_root = addr;
		} else {
			PREV_DEL(buf) = hd->ix_avail;
			ix_write(ix, addr, buf);
			hd->ix_avail = addr;
		}
	}
}

static bool init_index(struct index *ix,
		int (*cmpf)(const char *, const char *, size_t))
{
	ix->ix_blksize = BLK_OFFSET + MIN_ORDER * ENTRY_SIZE(ix);
	ix->ix_blksize = ix->ix_blksize + (-(ix->ix_blksize) % BLK_SIZE);

	ix->ix_order = (ix->ix_blksize - BLK_OFFSET) / ENTRY_SIZE(ix);
	if (ix->ix_order % 2 == 0)
		ix->ix_order--; /* order must be uneven */

	ix->ix_cmpf = (cmpf != NULL) ? cmpf
		: (int (*)(const char *, const char *, size_t))memcmp;
	
	ix->ix_buf = xmalloc(ix->ix_blksize);
	if (ix->ix_buf == NULL)
		return false;
	memset(ix->ix_buf, 0, ix->ix_blksize);
	return true;
}

struct index *ix_create(const char *ix_name, size_t ix_size,
		int (*cmpf)(const char *, const char *, size_t))
{
	struct index *ix;
	struct ix_header hd;
	char buf[BLK_SIZE];
	int fd;

	assert(ix_name != NULL);
	assert(ix_size > 0);

	hd.ix_size = ix_size;
	hd.ix_root = 0;
	hd.ix_max = 0;
	hd.ix_avail = INVALID_ADDR;
	hd.ix_closed = false;
	memcpy(buf, &hd, sizeof(struct ix_header));
	FILL_BUF(buf, sizeof(struct ix_header), BLK_SIZE);

	fd = open(ix_name, CREATE_FLAGS, FILE_MODE);
	if (fd == -1)
		return NULL;

	if (!WRITE(fd, buf, BLK_SIZE)) {
		close(fd);
		return NULL;
	}

	ix = xmalloc(sizeof(struct index));
	if (ix == NULL) {
		close(fd);
		return NULL;
	}
	strntermcpy(ix->ix_name, ix_name, PATH_MAX+1);
	ix->ix_name[PATH_MAX] = '\0';
	ix->ix_fd = fd;
	ix->ix_size = hd.ix_size;
	ix->ix_root = hd.ix_root;
	ix->ix_max = hd.ix_max;
	ix->ix_avail = hd.ix_avail;

	if (!init_index(ix, cmpf)) {
		close(fd);
		free(ix);
		return NULL;
	}

#ifndef NO_CACHE
	ix->ix_cache = cache_init(ix->ix_blksize,
			TOTAL_CACHE_SIZE / ix->ix_blksize);
#endif

	TYPE(ix->ix_buf) = LEAF;
	CNT(ix->ix_buf) = 0;
	LNBR(ix->ix_buf) = INVALID_ADDR;
	RNBR(ix->ix_buf) = INVALID_ADDR;
	if (!ix_write(ix, 0, ix->ix_buf)) {
		close(fd);
#ifndef NO_CACHE
		cache_free(ix->ix_cache);
#endif
		free(ix);
		return NULL;
	}
	return ix;
}

struct index *ix_open(const char *ix_name, 
		int (*cmpf)(const char *, const char *, size_t))
{
	struct index *ix;
	struct ix_header hd;
	int fd;

	assert(ix_name != NULL);

	fd = open(ix_name, OPEN_RW_FLAGS, FILE_MODE);
	if (fd == -1)
		return NULL;

	if (!READ(fd, &hd, sizeof(struct ix_header))) {
		close(fd);
		return NULL;
	}

	ix = xmalloc(sizeof(struct index));
	strntermcpy(ix->ix_name, ix_name, PATH_MAX+1);
	ix->ix_name[PATH_MAX] = '\0';
	ix->ix_fd = fd;
	ix->ix_size = hd.ix_size;
	ix->ix_root = hd.ix_root;
	ix->ix_max = hd.ix_max;
	ix->ix_avail = hd.ix_avail;

	if (!init_index(ix, cmpf)) {
		close(fd);
		free(ix);
		return NULL;
	}

#ifndef NO_CACHE
	ix->ix_cache = cache_init(ix->ix_blksize,
			TOTAL_CACHE_SIZE / ix->ix_blksize);
#endif

	if (!hd.ix_closed) {
		rebuild_header(ix, &hd);
		ix->ix_root = hd.ix_root;
		ix->ix_max = hd.ix_max;
		ix->ix_avail = hd.ix_avail;
	}

	hd.ix_closed = false;
	GOTO_POS(ix, 0);
	if (!WRITE(ix->ix_fd, &hd, sizeof(struct ix_header))) {
		close(fd);
#ifndef NO_CACHE
		cache_free(ix->ix_cache);
#endif
		free(ix);
		return NULL;
	}
	return ix;
}

bool ix_close(struct index *ix)
{
	struct ix_header hd;
	bool retval;

	hd.ix_size = ix->ix_size;
	hd.ix_root = ix->ix_root;
	hd.ix_max = ix->ix_max;
	hd.ix_avail = ix->ix_avail;
	hd.ix_closed = true;
	GOTO_POS(ix, 0);
	retval = WRITE(ix->ix_fd, &hd, sizeof(struct ix_header));
	close(ix->ix_fd);
	ix->ix_fd = -1;
#ifndef NO_CACHE
	cache_free(ix->ix_cache);
#endif
	free(ix->ix_buf);
	free(ix);
	return retval;
}

blkaddr_t ix_search(struct index *ix, const char *key)
{
	short i;
	int cmpval = -1;
	char *buf;
	blkaddr_t addr;

	assert(ix != NULL);
	assert(key != NULL);

	buf = ix->ix_buf;
	addr = ix->ix_root;

next_level:
	if (!ix_read(ix, addr, buf))
		return INVALID_ADDR;
	for (i = 0; i < CNT(buf)
			&& (cmpval = CMPF(ix, key, KEY(ix, buf, i))) > 0; i++)
		;

	if (i < CNT(buf) && TYPE(buf) == INNER) {
		addr = PTR(ix, buf, i);
		goto next_level;
	} else
		return (i < CNT(buf) && cmpval == 0) ? PTR(ix, buf, i)
			: INVALID_ADDR;
}

struct ix_iter *ix_iterator(struct index *ix, const char *key)
{
	short i;
	int cmpval = -1;
	char *buf;
	blkaddr_t addr;

	assert(ix != NULL);
	assert(key != NULL);

	buf = ix->ix_buf;
	addr = ix->ix_root;

next_level:
	if (!ix_read(ix, addr, buf))
		return NULL;
	for (i = 0; i < CNT(buf)
			&& (cmpval = CMPF(ix, key, KEY(ix, buf, i))) > 0; i++)
		;

	if (i < CNT(buf) && TYPE(buf) == INNER) {
		addr = PTR(ix, buf, i);
		goto next_level;
	} else if (i == CNT(buf)) {
		struct ix_iter *iter;

		/* go on to most right leaf */
		while (TYPE(buf) == INNER) {
			i = CNT(buf) - 1;
			addr = PTR(ix, buf, i);
			if (!ix_read(ix, addr, buf))
				return NULL;
		}

		iter = xmalloc(sizeof(struct ix_iter));
		iter->it_ix = ix;
		iter->it_curindex = CNT(buf); /* set behind last element */
		iter->it_curcmpval = cmpval; /* cmpval > 0 */
		iter->it_origindex = CNT(buf);
		iter->it_origcmpval = cmpval;

		iter->it_key = xmalloc(ix->ix_size);
		memcpy(iter->it_key, key, ix->ix_size);

		iter->it_buf = xmalloc(ix->ix_blksize);
		memcpy(iter->it_buf, buf, ix->ix_blksize);
		return iter;
	} else { /* i < CNT(buf) && TYPE(buf) == LEAF && cmpval <= 0 */
		struct ix_iter *iter;

		iter = xmalloc(sizeof(struct ix_iter));
		iter->it_ix = ix;
		iter->it_curindex = i;
		iter->it_curcmpval = cmpval;
		iter->it_origaddr = addr;
		iter->it_origindex = i;
		iter->it_curcmpval = cmpval;

		iter->it_key = xmalloc(ix->ix_size);
		memcpy(iter->it_key, key, ix->ix_size);

		iter->it_buf = xmalloc(ix->ix_blksize);
		memcpy(iter->it_buf, buf, ix->ix_blksize);
		return iter;
	}
}

struct ix_iter *ix_min(struct index *ix)
{
	char *buf;
	blkaddr_t addr;
	struct ix_iter *iter;

	assert(ix != NULL);

	buf = ix->ix_buf;
	addr = ix->ix_root;

next_level:
	if (!ix_read(ix, addr, buf))
		return NULL;

	if (TYPE(buf) == INNER) {
		addr = PTR(ix, buf, 0);
		goto next_level;
	}

	iter = xmalloc(sizeof(struct ix_iter));
	iter->it_ix = ix;
	iter->it_curindex = 0; /* set at the last element */
	iter->it_curcmpval = 0;
	iter->it_origindex = 0;
	iter->it_origcmpval = 0;

	if (CNT(buf) > 0) {
		iter->it_key = xmalloc(ix->ix_size);
		memcpy(iter->it_key, KEY(ix, buf, 0), ix->ix_size);
	} else /* with respect to the scenario of an empty tree (CNT(buf)=0) */
		iter->it_key = NULL;

	iter->it_buf = xmalloc(ix->ix_blksize);
	memcpy(iter->it_buf, buf, ix->ix_blksize);
	return iter;
}

struct ix_iter *ix_max(struct index *ix)
{
	char *buf;
	blkaddr_t addr;
	struct ix_iter *iter;

	assert(ix != NULL);

	buf = ix->ix_buf;
	addr = ix->ix_root;

next_level:
	if (!ix_read(ix, addr, buf))
		return NULL;

	if (TYPE(buf) == INNER) {
		addr = PTR(ix, buf, CNT(buf) - 1);
		goto next_level;
	}

	iter = xmalloc(sizeof(struct ix_iter));
	iter->it_ix = ix;

	iter->it_curindex = CNT(buf); /* set behind last element */
	iter->it_curcmpval = 0;
	iter->it_origindex = CNT(buf);
	iter->it_origcmpval = 0;

	if (CNT(buf) > 0) {
		iter->it_key = xmalloc(ix->ix_size);
		memcpy(iter->it_key, KEY(ix, buf, CNT(buf) - 1), ix->ix_size);
	} else /* for the scenario of an empty tree (CNT(buf)=0) */
		iter->it_key = NULL;

	iter->it_buf = xmalloc(ix->ix_blksize);
	memcpy(iter->it_buf, buf, ix->ix_blksize);
	return iter;
}

void ix_iter_free(struct ix_iter *iter)
{
	if (iter != NULL) {
		if (iter->it_buf != NULL)
			free(iter->it_buf);
		if (iter->it_key)
			free(iter->it_key);
		free(iter);
	}
}

void ix_reset(struct ix_iter *iter)
{
	assert(iter != NULL);

	if (iter->it_origaddr != INVALID_ADDR) {
		ix_read(iter->it_ix, iter->it_origaddr, iter->it_buf);
		iter->it_curindex = iter->it_origindex;
		iter->it_curcmpval = iter->it_origcmpval;
	}
}

blkaddr_t ix_next(struct ix_iter *iter)
{
	blkaddr_t addr;

	if (iter->it_curindex < 0)
		return INVALID_ADDR;

	addr = ix_rnext(iter);
	if (addr != INVALID_ADDR && iter->it_curcmpval == 0)
		return addr;
	else {
		iter->it_curindex = -1;
		return INVALID_ADDR;
	}
}

blkaddr_t ix_lnext(struct ix_iter *iter)
{
	blkaddr_t ptr;
	const char *key;

	assert(iter != NULL);
	assert(iter->it_curindex >= 0);

	if (iter->it_curindex > 0) { /* left elem in block */
		iter->it_curindex--;
		ptr = PTR(iter->it_ix, iter->it_buf, iter->it_curindex);
		key = KEY(iter->it_ix, iter->it_buf, iter->it_curindex);
		iter->it_curcmpval = CMPF(iter->it_ix, iter->it_key, key);
		return ptr;
	} else if (LNBR(iter->it_buf) != INVALID_ADDR) { /* go to left block */
		blkaddr_t addr;

		addr = LNBR(iter->it_buf);
		if (!ix_read(iter->it_ix, addr, iter->it_buf))
			return INVALID_ADDR;
		iter->it_curindex = CNT(iter->it_buf) - 1;
		ptr = PTR(iter->it_ix, iter->it_buf, iter->it_curindex);
		key = KEY(iter->it_ix, iter->it_buf, iter->it_curindex);
		iter->it_curcmpval = CMPF(iter->it_ix, iter->it_key, key);
		return ptr;
	} else /* left end of leaves reached */
		return INVALID_ADDR;
}

const void *ix_lval(const struct ix_iter *iter)
{
	assert(iter != NULL);
	assert(iter->it_curindex >= 0);

	return iter->it_curindex < CNT(iter->it_buf)
		? KEY(iter->it_ix, iter->it_buf, iter->it_curindex)
		: NULL;
}

blkaddr_t ix_rnext(struct ix_iter *iter)
{
	blkaddr_t ptr;
	const char *key;

	assert(iter != NULL);
	assert(iter->it_curindex >= 0);

	if (iter->it_curindex < CNT(iter->it_buf)) { /* right elem in block */
		ptr = PTR(iter->it_ix, iter->it_buf, iter->it_curindex);
		key = KEY(iter->it_ix, iter->it_buf, iter->it_curindex);
		iter->it_curcmpval = CMPF(iter->it_ix, iter->it_key, key);
		iter->it_curindex++;
		return ptr;
	} else if (RNBR(iter->it_buf) != INVALID_ADDR) { /* go to right block */
		blkaddr_t addr;

		addr = RNBR(iter->it_buf);
		if (!ix_read(iter->it_ix, addr, iter->it_buf))
			return INVALID_ADDR;
		iter->it_curindex = 0;
		ptr = PTR(iter->it_ix, iter->it_buf, iter->it_curindex);
		key = KEY(iter->it_ix, iter->it_buf, iter->it_curindex);
		iter->it_curcmpval = CMPF(iter->it_ix, iter->it_key, key);
		iter->it_curindex++;
		return ptr;
	} else /* right end of leaves reached */
		return INVALID_ADDR;
}

const void *ix_rval(const struct ix_iter *iter)
{
	assert(iter != NULL);
	assert(iter->it_curindex-1 >= 0);

	return iter->it_curindex-1 < CNT(iter->it_buf)
		? KEY(iter->it_ix, iter->it_buf, iter->it_curindex-1)
		: NULL;
}

static bool split_child(struct index *ix,
		blkaddr_t paddr, char *pbuf,	/* the parent node */
		short i,			/* the index of lbuf in pbuf */
		blkaddr_t laddr, char *lbuf)	/* the full node */
{
	blkaddr_t raddr;
	char rbuf[ix->ix_blksize];
	short j, t;

	t = ORDER(ix) / 2 + 1;

	raddr = alloc_blk(ix);
	TYPE(rbuf) = TYPE(lbuf);
	CNT(rbuf) = t - 1;

	LNBR(rbuf) = laddr;
	RNBR(rbuf) = RNBR(lbuf);
	RNBR(lbuf) = raddr;
	if (!set_lnbr(ix, RNBR(rbuf), raddr))
		return false;

	/* initialize the rbuf with the upper half of entries of the old 
	 * old node (which is stored in lbuf at) */
	for (j = 0; j < CNT(rbuf); j++) {
		PTR(ix, rbuf, j) = PTR(ix, lbuf, j+t);
		KEYCPY(ix, KEY(ix, rbuf, j), KEY(ix, lbuf, j+t));
	}
	CNT(lbuf) = t;

	/* update parent node: firstly shift the entries to obtain space for a
	 * new one. then we have to interesting entries:
	 * entry #i:   ptr = laddr, key = lkey
	 * entry #i+1: ptr = laddr, key = lkey 
	 * now we let entry #i+1 point to the right node (raddr) and change 
	 * the key of the entry #i to the key where we split the old node */
	CNT(pbuf)++;
	for (j = CNT(pbuf)-2; j >= i; j--) {
		PTR(ix, pbuf, j+1) = PTR(ix, pbuf, j);
		KEYCPY(ix, KEY(ix, pbuf, j+1), KEY(ix, pbuf, j));
	}
	PTR(ix, pbuf, i+1) = raddr;
	KEYCPY(ix, KEY(ix, pbuf, i), KEY(ix, lbuf, CNT(lbuf)-1));

	if (!ix_write(ix, paddr, pbuf))
		return false;
	if (!ix_write(ix, laddr, lbuf))
		return false;
	if (!ix_write(ix, raddr, rbuf))
		return false;
	return true;
}

static bool insert(struct index *ix,
		blkaddr_t addr, char *buf,		/* current node */
		blkaddr_t tuple_addr, const char *key)	/* key/addr pair */
{
	short i;
	int cmpval = -1;

	assert(ix != NULL);
	assert(addr != INVALID_ADDR);
	assert(buf != NULL);

	for (i = 0; i < CNT(buf)
			&& (cmpval = CMPF(ix, key, KEY(ix, buf, i))) > 0; i++)
		;

	if (i < CNT(buf) && cmpval == 0) /* key exists already */
		return true;

	if (TYPE(buf) == LEAF) { /* LEAF: insert key/addr pair */
		short j;

		CNT(buf)++;
		for (j = CNT(buf) - 2; j >= i; j--) {
			KEYCPY(ix, KEY(ix, buf, j+1), KEY(ix, buf, j));
			PTR(ix, buf, j+1) = PTR(ix, buf, j);
		}

		PTR(ix, buf, i) = tuple_addr;
		KEYCPY(ix, KEY(ix, buf, i), key);
		if (!ix_write(ix, addr, buf))
			return false;
		return true;
	} else { /* INNER: search next; possibly update key and/or split */
		blkaddr_t son_addr;
		char son_buf[ix->ix_blksize];

		if (i == CNT(buf)) { /* update most right key and follow it */
			i--;
			KEYCPY(ix, KEY(ix, buf, i), key);
			if (!ix_write(ix, addr, buf))
				return false;
		}

		son_addr = PTR(ix, buf, i);
		if (!ix_read(ix, son_addr, son_buf))
			return false;

		if (CNT(son_buf) == ORDER(ix)) { /* split son */
			if (!split_child(ix, addr, buf, i, son_addr, son_buf))
				return false;
			/* detect to which node we need to step down: 
			 * left node is already read in son_buf, while we would
			 * need to read right node from disk */
			if (CMPF(ix, key, KEY(ix, buf, i)) > 0) {
				i++;
				son_addr = PTR(ix, buf, i);
				if (!ix_read(ix, son_addr, son_buf))
					return false;
			}
		}

		return insert(ix, son_addr, son_buf, tuple_addr, key);
	}
}

bool ix_insert(struct index *ix, blkaddr_t tuple_addr, const char *key)
{
	if (!ix_read(ix, ix->ix_root, ix->ix_buf))
		return false;

	if (CNT(ix->ix_buf) == ORDER(ix)) { /* root full => create a new */
		blkaddr_t root_addr, old_root_addr;
		char root_buf[ix->ix_blksize], *old_root_buf;
		
		old_root_addr = ix->ix_root;
		old_root_buf = ix->ix_buf;
		
		ix->ix_root = alloc_blk(ix);
		root_addr = ix->ix_root;
		TYPE(root_buf) = INNER;
		LNBR(root_buf) = INVALID_ADDR;
		RNBR(root_buf) = INVALID_ADDR;
		CNT(root_buf) = 1;
		PTR(ix, root_buf, 0) = old_root_addr;
		KEYCPY(ix, KEY(ix, root_buf, 0),
				KEY(ix, old_root_buf, CNT(old_root_buf) - 1));
		split_child(ix, root_addr, root_buf, 0, old_root_addr,
				old_root_buf);
		return insert(ix, root_addr, root_buf, tuple_addr, key);
	} else /* root is not full */
		return insert(ix, ix->ix_root, ix->ix_buf, tuple_addr, key);
}

static void merge_neighbors(struct index *ix, 
		char *pbuf,	/* parent node */
		short i,	/* index of left son */
		char *lbuf,	/* left son */
		char *rbuf)	/* right son */
{
	short j, old_lbuf_cnt;

	KEYCPY(ix, KEY(ix, pbuf, i), KEY(ix, pbuf, i+1));
	for (j = i+1; j < CNT(pbuf)-1; j++) {
		PTR(ix, pbuf, j) = PTR(ix, pbuf, j+1);
		KEYCPY(ix, KEY(ix, pbuf, j), KEY(ix, pbuf, j+1));
	}
	CNT(pbuf)--;

	old_lbuf_cnt = CNT(lbuf);
	CNT(lbuf) += CNT(rbuf);
	RNBR(lbuf) = RNBR(rbuf);
	for (j = 0; j < CNT(rbuf); j++) {
		PTR(ix, lbuf, old_lbuf_cnt + j) = PTR(ix, rbuf, j);
		KEYCPY(ix, KEY(ix, lbuf, old_lbuf_cnt + j), KEY(ix, rbuf, j));
	}
}

static void move_left(struct index *ix,
		char *pbuf,	/* parent node */
		short i,	/* index of left son */
		char *lbuf,	/* left son */
		char *rbuf)	/* right son */
{
	short j;

	CNT(lbuf)++;
	PTR(ix, lbuf, CNT(lbuf)-1) = PTR(ix, rbuf, 0);
	KEYCPY(ix, KEY(ix, lbuf, CNT(lbuf)-1), KEY(ix, rbuf, 0));
	for (j = 0; j < CNT(rbuf)-1; j++) {
		PTR(ix, rbuf, j) = PTR(ix, rbuf, j+1);
		KEYCPY(ix, KEY(ix, rbuf, j), KEY(ix, rbuf, j+1));
	}
	CNT(rbuf)--;
	KEYCPY(ix, KEY(ix, pbuf, i), KEY(ix, lbuf, CNT(lbuf)-1));
}

static void move_right(struct index *ix,
		char *pbuf,	/* parent node */
		short i,	/* index of left son */
		char *lbuf,	/* left son */
		char *rbuf)	/* right son */
{
	short j;

	CNT(rbuf)++;
	for (j = CNT(rbuf) - 1; j > 0; j--) {
		PTR(ix, rbuf, j) = PTR(ix, rbuf, j-1);
		KEYCPY(ix, KEY(ix, rbuf, j), KEY(ix, rbuf, j-1));
	}
	PTR(ix, rbuf, 0) = PTR(ix, lbuf, CNT(lbuf)-1);
	KEYCPY(ix, KEY(ix, rbuf, 0), KEY(ix, lbuf, CNT(lbuf)-1));
	CNT(lbuf)--;
	KEYCPY(ix, KEY(ix, pbuf, i), KEY(ix, lbuf, CNT(lbuf)-1));
}

static blkaddr_t delete(struct index *ix,
		blkaddr_t addr, char *buf,	/* current node */
		const char *key)		/* searched key */
{
	short i, t;
	int cmpval = 0;

	assert(ix != NULL);
	assert(addr != INVALID_ADDR);
	assert(key != NULL);

	t = ORDER(ix) / 2 + 1;

	for (i = 0; i < CNT(buf) 
			&& (cmpval = CMPF(ix, key, KEY(ix, buf, i))) > 0; i++)
		;

	if (i == CNT(buf)) /* key not found */
		return INVALID_ADDR;

	if (TYPE(buf) == LEAF) { /* LEAF: delete addr/key pair */
		if (cmpval == 0) {
			blkaddr_t tuple_addr;

			tuple_addr = PTR(ix, buf, i);
			for (; i < CNT(buf)-1; i++) {
				PTR(ix, buf, i) = PTR(ix, buf, i+1);
				KEYCPY(ix, KEY(ix, buf, i), KEY(ix, buf, i+1));
			}
			CNT(buf)--;
			if (!ix_write(ix, addr, buf))
				return INVALID_ADDR;
			return tuple_addr;
		} else
			return INVALID_ADDR;
	} else { /* INNER: search next; possibly update key and/or merge/move */
		blkaddr_t son_addr;
		char son_buf[ix->ix_blksize];

		son_addr = PTR(ix, buf, i);

		if (!ix_read(ix, son_addr, son_buf))
			return INVALID_ADDR;

		if (CNT(son_buf) == t-1) { /* son would have too less sons */
			blkaddr_t nbr_addr;
			char nbr_buf[ix->ix_blksize];
			bool nbr_is_left;

			if (i >= 1) {
				nbr_is_left = true;
				nbr_addr = PTR(ix, buf, i-1);
				if (!ix_read(ix, nbr_addr, nbr_buf))
					return INVALID_ADDR;
			} else {
				nbr_is_left = false;
				nbr_addr = PTR(ix, buf, i+1);
				if (!ix_read(ix, nbr_addr, nbr_buf))
					return INVALID_ADDR;
			}
	
			if (CNT(nbr_buf) == t-1) { /* merge nodes */
				if (nbr_is_left) {
					/* decrement i, because the current
					 * position shifts one left and this
					 * position might be needed some lines
					 * below again if cmp=0 */
					--i;
					merge_neighbors(ix, buf, i, nbr_buf,
							son_buf);
					free_blk(ix, son_addr);
					son_addr = nbr_addr;
					memcpy(son_buf, nbr_buf,
							ix->ix_blksize);
				} else {
					merge_neighbors(ix, buf, i, son_buf, 
							nbr_buf);
					free_blk(ix, nbr_addr);
				}
				/* update LNBR pointer of right neighbor */
				if (!set_lnbr(ix, RNBR(son_buf), son_addr))
					return INVALID_ADDR;
				assert(CNT(son_buf) > t-1);
			} else if (CNT(nbr_buf) > t-1) { /* move one entry */
				if (nbr_is_left) {
					move_right(ix, buf, i-1, nbr_buf,
							son_buf);
					/* update nbr_buf */
					if (!ix_write(ix, nbr_addr, nbr_buf))
						return INVALID_ADDR;
				} else {
					move_left(ix, buf, i, son_buf,
						nbr_buf);
					/* update nbr_buf */
					if (!ix_write(ix, nbr_addr, nbr_buf))
						return INVALID_ADDR;
				}
				assert(CNT(son_buf) > t-1);
				assert(CNT(nbr_buf) >= t-1);
			}
		}

		if (cmpval == 0) { /* the to-be-deleted key is in parent node
				    * and therefore must be replaced; but before
				    * replacing delete() must step down to the
				    * next level */
			blkaddr_t tuple_addr;

			tuple_addr = delete(ix, son_addr, son_buf, key);
			if (tuple_addr == INVALID_ADDR)
				return INVALID_ADDR;
			KEYCPY(ix, KEY(ix, buf, i),
					KEY(ix, son_buf, CNT(son_buf)-1));
			if (!ix_write(ix, addr, buf))
				return INVALID_ADDR;
			return tuple_addr;
		} else { /* because the to-be-deleted key is not in the parent
			  * node, we don't need to update the parent after
			  * delete()ing in the next level; thus we can achieve
			  * end-recursion by firstly writing the parent and
			  * delete()ing thereafter */
			if (!ix_write(ix, addr, buf))
				return INVALID_ADDR;
			return delete(ix, son_addr, son_buf, key);
		}
	}
}

blkaddr_t ix_delete(struct index *ix, const char *key)
{
	blkaddr_t tuple_addr;

	assert(ix != NULL);
	assert(key != NULL);

	if (!ix_read(ix, ix->ix_root, ix->ix_buf))
		return INVALID_ADDR;
	tuple_addr = delete(ix, ix->ix_root, ix->ix_buf, key);

	if (TYPE(ix->ix_buf) == INNER && CNT(ix->ix_buf) == 1) { /* kick root */
		free_blk(ix, ix->ix_root);
		ix->ix_root = PTR(ix, ix->ix_buf, 0);
	}

	return tuple_addr;
}


#ifndef NDEBUG

#include <stdio.h>

#define indent();	for (j=0; j < indent_lvl; j++) fprintf(fp, " ");

static short indent_lvl = 0;

static void print_node(struct index *ix, blkaddr_t addr, FILE *fp)
{
	short i, j;
	char buf[ix->ix_blksize]; /* need own buffer because of recursivity */

	if (addr > ix->ix_max)
		printf("addr out of range: %d\n", addr);
	assert(addr != INVALID_ADDR);
	assert(addr <= ix->ix_max);

	assert(ix_read(ix, addr, buf));
	indent();
	fprintf(fp, "node %d (left: %d | right: %d) {\n", addr,
			LNBR(buf), RNBR(buf));
	indent_lvl += 4;
	for (i = 0; i < CNT(buf); i++) {
		indent();
		fprintf(fp, "ptr[%d]=%d\n", i, PTR(ix,buf,i));
		indent();
		fprintf(fp, "key[%d]=%d\n", i, *(int *)KEY(ix,buf,i));
		if (TYPE(buf) != LEAF)
			print_node(ix, PTR(ix,buf,i), fp);
	}
	indent_lvl -= 4;
	indent();
	fprintf(fp, "}\n");
}

void ix_print(struct index *ix, const char *fn)
{
	FILE *fp;

	if (fn == NULL)
		fp = stdout;
	else
		fp = fopen(fn, "w");

	assert(fp != NULL);

	fprintf(fp, "ix->ix_root = %d\n", ix->ix_root);
	fprintf(fp, "ix->ix_size = %u\n", (unsigned int)ix->ix_size);
	fprintf(fp, "ix->ix_blksize = %u\n", (unsigned int)ix->ix_blksize);
	fprintf(fp, "ix->ix_order = %d\n", ix->ix_order);

	print_node(ix, ix->ix_root, fp);
	fprintf(fp, "\n\n---------------------------\n\n");
}

void draw(struct index *ix, blkaddr_t addr, FILE *fp)
{
	char str[4096];
	char *ptr = str;
	char buf[ix->ix_blksize];
	short i;

	if (!ix_read(ix, addr, buf))
		return;
	sprintf(str, "%d: ", addr);
	ptr += strlen(str);
	for (i = 0; i < CNT(buf); i++) {
		char *s = KEY(ix, buf, i);
		strcpy(ptr, s);
		ptr += strlen(s);
		if (i+1 < CNT(buf)) {
			strcpy(ptr, " | ");
			ptr += 3;
		}
	}

	fprintf(fp, "%d[label=\"%s\"]\n", addr, str);
	if (TYPE(buf) != LEAF) {
		for (i = 0; i < CNT(buf); i++) {
			draw(ix, PTR(ix, buf, i), fp);
			fprintf(fp, "%d -> %d\n", addr, PTR(ix, buf, i));
		}
	}
}

void ix_draw(struct index *ix, const char *fn)
{
	FILE *fp;

	if (fn == NULL)
		fp = stdout;
	else
		fp = fopen(fn, "w");

	assert(fp != NULL);

	fprintf(fp, "digraph {\n");
	draw(ix, ix->ix_root, fp);
	fprintf(fp, "}");
	fclose(fp);
}

#endif

