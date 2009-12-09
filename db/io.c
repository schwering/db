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

#include "io.h"
#include "block.h"
#include "cache.h"
#include "constants.h"
#include "err.h"
#include "mem.h"
#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

/* the maximum size of the database cache */
#define TOTAL_CACHE_SIZE	(1024 * 10)

/* converts a block address (blkaddr_t) to a file position (off_t) */
#define ADDR_TO_POS(rl, addr)	(((off_t)addr) * (rl)->rl_header.hd_tpasize\
				+ (rl)->rl_header.hd_asize)

/* moves the file cursor to the address (blkaddr_t) */
#define GOTO_ADDR(rl, addr)	lseek(rl->rl_fd, ADDR_TO_POS(rl,addr), SEEK_SET)

/* calculates the size of a 'size'-bytes large tuple aligned to BLK_SIZE */
#define CALC_ASIZE(size)	((size) + ((- (size)) % BLK_SIZE))

/* moves the file cursor cnt bytes forward from the current position */
#define SKIP_BYTES(rl, cnt)	lseek(rl->rl_fd, cnt, SEEK_CUR)

/* fills a buffer from a given offset to another given position with zeros */
#define FILL_BUF(buf, from, to)	memset(buf+from, 0, (to)-(from))

/* tuple types: TP_AVAIL is a free space, TP_OCCUP marks an active tuple */
#define TP_AVAIL		((tpstatus_t)0)
#define TP_OCCUP		(((tpstatus_t)1))

#define TP_PREV_OFFSET		sizeof(tpstatus_t)
#define TP_NEXT_OFFSET		(TP_PREV_OFFSET + sizeof(blkaddr_t))
#define TP_DATA_OFFSET		(TP_NEXT_OFFSET + sizeof(blkaddr_t))

/* the tuple's status: either TP_AVAIL or TP_OCCUP */
#define TP_STATUS(tp)		(*((tpstatus_t *)((char *)(tp))))

/* the tuple's following neighbor's address */
#define TP_NEXT_ADDR(tp)	(*((blkaddr_t *)((char *)(tp)+TP_NEXT_OFFSET)))

/* the tuple's preceeding neighbor's address */
#define TP_PREV_ADDR(tp)	(*((blkaddr_t *)((char *)(tp)+TP_PREV_OFFSET)))

/* the tuple's data itself */
#define TP_DATA(tp)		((char *)(tp)+TP_DATA_OFFSET)

/* reads a given amount of bytes from a file into a pointer */
#define READ(fd, ptr, size)	(read((fd),(ptr),(size)) == (ssize_t)(size))

/* writes a given amount of bytes from a pointer to a file */
#define WRITE(fd, ptr, size)	(write((fd),(ptr),(size)) == (ssize_t)(size))


typedef char tpstatus_t; /* a tuple's status (either TP_AVAIL or TP_OCCUP) */

static inline bool tp_read(struct srel *rl, blkaddr_t addr, char *buf)
{
	bool retval;

#ifndef NO_CACHE
	if (cache_search(rl->rl_cache, addr, buf))
		return true;
#endif

	GOTO_ADDR(rl, addr);
	retval = READ(rl->rl_fd, buf, rl->rl_header.hd_tpasize);
#ifndef NO_CACHE
	if (retval)
		cache_push(rl->rl_cache, addr, buf);
#endif
	return retval;
}

static inline bool tp_write(struct srel *rl, blkaddr_t addr, const char *buf)
{
	bool retval;

	GOTO_ADDR(rl, addr);
	retval = WRITE(rl->rl_fd, buf, rl->rl_header.hd_tpasize);
#ifndef NO_CACHE
	if (retval)
		cache_update(rl->rl_cache, addr, 0, buf,
				rl->rl_header.hd_tpasize);
#endif
	return retval;
}

static inline bool tp_write_range(struct srel *rl, blkaddr_t addr,
		size_t offset, const char *buf, size_t from, size_t to)
{
	bool retval;

	GOTO_ADDR(rl, addr);
	SKIP_BYTES(rl, offset);
	retval = WRITE(rl->rl_fd, buf+from, to-from);
#ifndef NO_CACHE
	if (retval)
		cache_update(rl->rl_cache, addr, offset, buf+from, to-from);
#endif
	return retval;
}

bool rl_write_header(struct srel *rl)
{
	char buf[rl->rl_header.hd_asize];

	assert(rl != NULL);

	memcpy(buf, &(rl->rl_header), sizeof(struct srel_hdr));
	FILL_BUF(buf, sizeof(struct srel_hdr), rl->rl_header.hd_asize);
	lseek(rl->rl_fd, 0, SEEK_SET);
	return WRITE(rl->rl_fd, buf, rl->rl_header.hd_asize);
}

static void rebuild_header(struct srel *rl)
{
	blkaddr_t addr;

	assert(rl != NULL);

	rl->rl_header.hd_tpcnt = 0;
	rl->rl_header.hd_tpmax = INVALID_ADDR;
	rl->rl_header.hd_tpavail = INVALID_ADDR;
	rl->rl_header.hd_tplatest = INVALID_ADDR;

	for (addr = 0; tp_read(rl, addr, rl->rl_tpbuf); addr++) {
		if (TP_STATUS(rl->rl_tpbuf) == TP_OCCUP)
			rl->rl_header.hd_tpcnt++;
		if (TP_STATUS(rl->rl_tpbuf) == TP_AVAIL
				&& TP_NEXT_ADDR(rl->rl_tpbuf) == INVALID_ADDR)
			rl->rl_header.hd_tpavail = addr;
		if (TP_STATUS(rl->rl_tpbuf) == TP_OCCUP
				&& TP_NEXT_ADDR(rl->rl_tpbuf) == INVALID_ADDR)
			rl->rl_header.hd_tplatest = addr;
	}

	rl->rl_header.hd_tpmax = addr - 1;
}

static bool read_header(struct srel *rl) 
{
	assert(rl != NULL);

	return READ(rl->rl_fd, &(rl->rl_header), sizeof(struct srel_hdr));
}

struct srel *rl_create(struct srel *rl)
{
	unsigned short i;
	size_t atsize_sum;

	assert(rl != NULL);

	rl->rl_fd = open(rl->rl_name, CREATE_FLAGS, FILE_MODE);
	if (rl->rl_fd == -1)
		return NULL;

	/* align header to a multiple BLK_SIZE byte */
	rl->rl_header.hd_asize = CALC_ASIZE(sizeof(struct srel_hdr));

	/* align tuple size to a multiple of BLK_SIZE byte */
	atsize_sum = 0;
	for (i = 0; i < rl->rl_header.hd_atcnt; i++) {
		rl->rl_header.hd_attrs[i].at_offset = atsize_sum;
		atsize_sum += rl->rl_header.hd_attrs[i].at_size;
	}
	rl->rl_header.hd_tpsize = TP_DATA_OFFSET + atsize_sum;
	rl->rl_header.hd_tpasize = CALC_ASIZE(rl->rl_header.hd_tpsize);

	rl->rl_header.hd_tpcnt = 0;
	rl->rl_header.hd_tpmax = INVALID_ADDR;
	rl->rl_header.hd_tpavail = INVALID_ADDR;
	rl->rl_header.hd_tplatest = INVALID_ADDR;
	rl->rl_header.hd_refcnt = 0;
	rl->rl_header.hd_fkeycnt = 0;
	rl->rl_header.hd_rlclosed = false;

	if (rl_write_header(rl)) {
		rl->rl_tpbuf = xmalloc(rl->rl_header.hd_tpasize);
#ifndef NO_CACHE
		rl->rl_cache = cache_init(rl->rl_header.hd_tpasize,
				TOTAL_CACHE_SIZE / rl->rl_header.hd_tpasize);
#endif
		return rl;
	} else {
		ERR(E_WRITE_FAILED);
		return NULL;
	}
}

struct srel *rl_open(struct srel *rl)
{
	assert(rl != NULL);

	rl->rl_fd = open(rl->rl_name, OPEN_RW_FLAGS, FILE_MODE);
	if (rl->rl_fd == -1) {
		ERR(E_OPEN_FAILED);
		return  NULL;
	}

	if (read_header(rl)) {
		rl->rl_tpbuf = xmalloc(rl->rl_header.hd_tpasize);
		if (rl->rl_header.hd_rlclosed) {
			rl->rl_header.hd_rlclosed = false;
			if (!rl_write_header(rl)) {
				ERR(E_WRITE_FAILED);
				free(rl->rl_tpbuf);
				return NULL;
			}
		} else {
			rl->rl_header.hd_rlclosed = false;
			rebuild_header(rl);
		}
#ifndef NO_CACHE
		rl->rl_cache = cache_init(rl->rl_header.hd_tpasize,
				TOTAL_CACHE_SIZE / rl->rl_header.hd_tpasize);
#endif
		return rl;
	} else 
		return NULL;
}

bool rl_close(struct srel *rl)
{
	bool retval;

	assert(rl != NULL);

	rl->rl_header.hd_rlclosed = true;
	if (!(retval = rl_write_header(rl)))
		ERR(E_WRITE_FAILED);

	close(rl->rl_fd);
	rl->rl_fd = -1;
#ifndef NO_CACHE
	cache_free(rl->rl_cache);
#endif
	free(rl->rl_tpbuf);
	rl->rl_tpbuf = NULL;
	free(rl);
	return retval;
}

static bool update_prev_addr(struct srel *rl, blkaddr_t addr,
		blkaddr_t prev_addr)
{
	assert(rl != NULL);

	if (addr == INVALID_ADDR)
		return true;
	return tp_write_range(rl, addr, TP_PREV_OFFSET,
			(const char *)&prev_addr, 0, sizeof(blkaddr_t));
}

static bool update_next_addr(struct srel *rl, blkaddr_t addr,
		blkaddr_t next_addr)
{
	assert(rl != NULL);

	if (addr == INVALID_ADDR)
		return true;
	return tp_write_range(rl, addr, TP_NEXT_OFFSET,
			(const char *)&next_addr, 0, sizeof(blkaddr_t));
}

bool rl_delete(struct srel *rl, blkaddr_t addr)
{
	blkaddr_t prev_addr, next_addr;

	assert(rl != NULL);

	if (addr > rl->rl_header.hd_tpmax) {
		ERR(E_ADDR_OUT_OF_RANGE);
		return false;
	}

	/* load tuple into buffer */
	if (!tp_read(rl, addr, rl->rl_tpbuf)) {
		ERR(E_READ_FAILED);
		return false;
	}
	if (TP_STATUS(rl->rl_tpbuf) == TP_AVAIL)
		return true;
	prev_addr = TP_PREV_ADDR(rl->rl_tpbuf);
	next_addr = TP_NEXT_ADDR(rl->rl_tpbuf);

	/* mark tuple as deleted */
	TP_STATUS(rl->rl_tpbuf) = TP_AVAIL;
	TP_NEXT_ADDR(rl->rl_tpbuf) = INVALID_ADDR;
	TP_PREV_ADDR(rl->rl_tpbuf) = rl->rl_header.hd_tpavail;
	FILL_BUF(rl->rl_tpbuf, TP_DATA_OFFSET, rl->rl_header.hd_tpasize);

	/* override tuple in file */
	if (tp_write(rl, addr, rl->rl_tpbuf)) {
		/* update linked list of deleted tuples */
		if (!update_next_addr(rl, rl->rl_header.hd_tpavail, addr)) {
			ERR(E_UPDATE_NEXT_ADDR_FAILED);
			return false;
		}
		rl->rl_header.hd_tpavail = addr;

		/* update linked list of active tuples */
		if (!update_next_addr(rl, prev_addr, next_addr)) {
			ERR(E_UPDATE_NEXT_ADDR_FAILED);
			return false;
		}
		if (!update_prev_addr(rl, next_addr, prev_addr)) {
			ERR(E_UPDATE_PREV_ADDR_FAILED);
			return false;
		}

		if (addr == rl->rl_header.hd_tplatest)
			rl->rl_header.hd_tplatest = prev_addr;
		rl->rl_header.hd_tpcnt--;
		return true;
	} else {
		ERR(E_WRITE_FAILED);
		return false;
	}
}

bool rl_update(struct srel *rl, blkaddr_t addr, const char *data)
{
	assert(rl != NULL);
	assert(data != NULL);
	assert(data != TP_DATA(rl->rl_tpbuf));

	if (addr > rl->rl_header.hd_tpmax) {
		ERR(E_ADDR_OUT_OF_RANGE);
		return false;
	}

	/* prepare buffer with updated data only */
	memcpy(TP_DATA(rl->rl_tpbuf), data,
			rl->rl_header.hd_tpsize - TP_DATA_OFFSET);
	FILL_BUF(rl->rl_tpbuf, rl->rl_header.hd_tpsize,
			rl->rl_header.hd_tpasize);

	/* write the updated data to file */
	if (tp_write_range(rl, addr, TP_DATA_OFFSET, TP_DATA(rl->rl_tpbuf), 0,
				rl->rl_header.hd_tpasize - TP_DATA_OFFSET))
		return true;
	else {
		ERR(E_ADDR_OUT_OF_RANGE);
		return false;
	}
}

blkaddr_t rl_insert(struct srel *rl, const char *tp_data)
{
	blkaddr_t addr, prev_addr, next_addr;

	assert(rl != NULL);
	assert(tp_data != NULL);

	/* determine tuple address: replace an available tuple (and update 
	 * the list of deleted tuples) or append it */
	addr = rl->rl_header.hd_tpavail;
	if (addr == INVALID_ADDR)
		addr = rl->rl_header.hd_tpmax + 1;

	if (addr <= rl->rl_header.hd_tpmax) {
		if (!tp_read(rl, addr, rl->rl_tpbuf)) {
			ERR(E_READ_FAILED);
			return INVALID_ADDR;
		}
		if (TP_STATUS(rl->rl_tpbuf) == TP_OCCUP) {
			ERR(E_TUPLE_ACTIVE);
			return INVALID_ADDR;
		}
		prev_addr = TP_PREV_ADDR(rl->rl_tpbuf);
		next_addr = TP_NEXT_ADDR(rl->rl_tpbuf);
	} else {
		prev_addr = INVALID_ADDR;
		next_addr = INVALID_ADDR;
	}


	/* initialize tuple buffer */
	TP_STATUS(rl->rl_tpbuf) = TP_OCCUP;
	TP_PREV_ADDR(rl->rl_tpbuf) = rl->rl_header.hd_tplatest;
	TP_NEXT_ADDR(rl->rl_tpbuf) = INVALID_ADDR;
	memcpy(TP_DATA(rl->rl_tpbuf), tp_data,
			rl->rl_header.hd_tpsize - TP_DATA_OFFSET);
	FILL_BUF(rl->rl_tpbuf, rl->rl_header.hd_tpsize,
			rl->rl_header.hd_tpasize);

	/* write tuple to file */
	if (tp_write(rl, addr, rl->rl_tpbuf)) {
		/* update linked list of active tuples */
		if (!update_next_addr(rl, rl->rl_header.hd_tplatest, addr)) {
			ERR(E_UPDATE_NEXT_ADDR_FAILED);
			return INVALID_ADDR;
		}
		rl->rl_header.hd_tplatest = addr;

		/* update linked list of deleted tuples */
		if (!update_next_addr(rl, prev_addr, next_addr)) {
			ERR(E_UPDATE_NEXT_ADDR_FAILED);
			return INVALID_ADDR;
		}
		rl->rl_header.hd_tpavail = prev_addr;

		if (addr > rl->rl_header.hd_tpmax)
			rl->rl_header.hd_tpmax = addr;
		rl->rl_header.hd_tpcnt++;
		return addr;
	} else {
		ERR(E_WRITE_FAILED);
		return INVALID_ADDR;
	}
}

const char *rl_get(struct srel *rl, blkaddr_t addr)
{
	assert(rl != NULL);

	if (addr > rl->rl_header.hd_tpmax) {
		ERR(E_ADDR_OUT_OF_RANGE);
		return NULL;
	}

	/* load tuple into buffer */
	if (tp_read(rl, addr, rl->rl_tpbuf)) {
		tpstatus_t status = TP_STATUS(rl->rl_tpbuf);
		if (status == TP_OCCUP)
			return rl->rl_tpbuf + TP_DATA_OFFSET;
		else {
			ERR(E_TUPLE_DELETED);
			return NULL;
		}
	} else {
		ERR(E_READ_FAILED);
		return NULL;
	}
}

struct srel_iter *rl_iterator(struct srel *rl)
{
	struct srel_iter *iter;

	assert(rl != NULL);

	iter = xmalloc(sizeof(struct srel_iter));
	iter->it_rl = rl;
	iter->it_curaddr = INVALID_ADDR;

	/* buffer is empty except the TP_PREV_ADDR field which points to 
	 * the youngest tuple */
	iter->it_tpbuf = xmalloc(rl->rl_header.hd_tpasize);
	FILL_BUF(iter->it_tpbuf, 0, rl->rl_header.hd_tpasize);
	TP_PREV_ADDR(iter->it_tpbuf) = rl->rl_header.hd_tplatest;

	return iter;
}

void srel_iter_free(struct srel_iter *iter)
{
	if (iter != NULL) {
		if (iter->it_tpbuf != NULL)
			free(iter->it_tpbuf);
		free(iter);
	}
}

void rl_iterator_reset(struct srel_iter *iter)
{
	assert(iter != NULL);

	iter->it_curaddr = INVALID_ADDR;

	/* buffer is empty except the TP_PREV_ADDR field which points to 
	 * the youngest tuple */
	FILL_BUF(iter->it_tpbuf, 0, iter->it_rl->rl_header.hd_tpasize);
	TP_PREV_ADDR(iter->it_tpbuf) = iter->it_rl->rl_header.hd_tplatest;
}

const char *rl_next(struct srel_iter *iter)
{
	assert(iter != NULL);

	if (iter == NULL) {
		ERR(E_NULL_POINTER);
		return NULL;
	}

	/* determine address of tuple */
	iter->it_curaddr = TP_PREV_ADDR(iter->it_tpbuf);
	if (iter->it_curaddr == INVALID_ADDR)
		return NULL;

	/* load tuple into buffer */
	if (!tp_read(iter->it_rl, iter->it_curaddr, iter->it_tpbuf)) {
		ERR(E_READ_FAILED);
		return NULL;
	}
	if (TP_STATUS(iter->it_tpbuf) == TP_OCCUP)
		return TP_DATA(iter->it_tpbuf);
	else {
		ERR(E_TUPLE_DELETED);
		return NULL;
	}
}

