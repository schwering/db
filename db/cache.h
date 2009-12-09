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
 * Least Recently Used (LRU) cache. The implementation is `abstract' in a 
 * sense that it is used by both, io.c (relation tuple file) and btree.c 
 * (B+-Tree implementation) to cache read operations of blocks. Write operations
 * are not cached, i.e. there is no flush-mechanism to write the cache to disk.
 * Nevertheless, write operations must be synchronized with the cache so 
 * that the cached elements are kept up to date.
 */

#ifndef __CACHE_H__
#define __CACHE_H__

#include "block.h"
#include <stdbool.h>
#include <sys/types.h>

struct cache { /* cache structure */
	size_t size;			/* size of a cached element */
	unsigned short maxcount;	/* max. count of elements in cache */
	unsigned short count;		/* current count of elements in cache */
	struct cache_entry *table;	/* table of cached elements */
	struct cache_entry *first;	/* first element in list */
	struct cache_entry *last;	/* last element in list */
};

struct cache_entry { /* element in cache table */
	blkaddr_t addr;			/* address in file of cached element */
	char *buf;			/* data of cached element */
	struct cache_entry *prev;	/* previous list element or NULL */
	struct cache_entry *next;	/* next list element or NULL */
};

/* Initializes a cache for `count' elements of `size' bytes. */
struct cache *cache_init(size_t size, size_t count);

/* Frees the memory used by the cache. */
void cache_free(struct cache *cache);

/* Checks whether `addr' is in the cache; in this case it copies the data into
 * buf and returns true; otherwise returns false. */
bool cache_search(struct cache *cache, blkaddr_t addr, char *buf);

/* Adds a new element to the cache. */
void cache_push(struct cache *cache, blkaddr_t addr, const char *buf);

/* Checks whether `addr' is in the cache; in this case it updates the data 
 * at offset `offset' with the data in `buf' with the length `len'. */
bool cache_update(struct cache *cache, blkaddr_t addr, size_t offset,
		const char *buf, size_t len);

#ifdef CACHE_STATS
/* Prints very simple cache statistics. */
void cache_print_stats(void);
#endif

#endif

