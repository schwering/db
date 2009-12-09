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

#include "cache.h"
#include "mem.h"
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#ifdef CACHE_STATS
#include <stdio.h>
unsigned long cache_searches = 0;
unsigned long cache_successful_searches = 0;
unsigned long cache_pushes = 0;
unsigned long cache_updates = 0;
#endif

struct cache *cache_init(size_t size, size_t maxcount)
{
	struct cache *cache;
	unsigned short i;

	assert(size > 0);

	if (maxcount == 0)
		return NULL;

	cache = xmalloc(sizeof(struct cache));
	cache->size = size;
	cache->maxcount = maxcount;
	cache->count = 0;
	cache->first = NULL;
	cache->last = NULL;
	cache->table = xmalloc(maxcount * sizeof(struct cache_entry));
	for (i = 0; i < maxcount; i++)
		cache->table[i].buf = xmalloc(size);
	return cache;
}

void cache_free(struct cache *cache)
{
	size_t i;

	if (cache == NULL)
		return;

	for (i = 0; i < cache->count; i++)
		free(cache->table[i].buf);
	free(cache->table);
	free(cache);
}

bool cache_search(struct cache *cache, blkaddr_t addr, char *buf)
{
	struct cache_entry *p;

	if (cache == NULL)
		return false;

	assert(addr != INVALID_ADDR);
	assert(buf != NULL);

#ifdef CACHE_STATS
	cache_searches++;
#endif

	for (p = cache->first; p != NULL; p = p->next)
		if (addr == p->addr)
			break;
	if (p == NULL)
		return false;

	if (p != cache->first) {
		assert(cache->first != NULL);
		assert(cache->last != NULL);
		assert(p->prev != NULL);

		p->prev->next = p->next;
		if (p->next != NULL)
			p->next->prev = p->prev;
	
		if (p == cache->last)
			cache->last = p->prev;

		p->prev = NULL;
		p->next = cache->first;
		cache->first->prev = p;
		cache->first = p;
	}
	memcpy(buf, p->buf, cache->size);
#ifdef CACHE_STATS
	cache_successful_searches++;
#endif
	return true;
}

void cache_push(struct cache *cache, blkaddr_t addr, const char *buf)
{
	struct cache_entry *p = NULL;

	if (cache == NULL)
		return;

#ifdef CACHE_STATS
	cache_pushes++;
#endif

	assert(addr != INVALID_ADDR);
	assert(buf != NULL);

	if (cache->count == 0) {
		assert(cache->first == NULL);
		assert(cache->last == NULL);
		p = &cache->table[0];
		p->prev = NULL;
		p->next = NULL;
		cache->first = p;
		cache->last = p;
		cache->count++;
	} else if (cache->count < cache->maxcount) {
		assert(cache->first != NULL);
		assert(cache->last != NULL);
		assert(cache->count == 1 || cache->first != cache->last);
		p = &cache->table[cache->count];
		p->prev = NULL;
		p->next = cache->first;
		cache->first->prev = p;
		cache->first = p;
		cache->count++;
	} else if (cache->count == cache->maxcount) {
		assert(cache->first != NULL);
		assert(cache->last != NULL);
		assert(cache->first != cache->last);
		p = cache->last;
		cache->last = cache->last->prev;
		cache->last->next = NULL;
		p->prev = NULL;
		p->next = cache->first;
		cache->first->prev = p;
		cache->first = p;
	}
	p->addr = addr;
	memcpy(p->buf, buf, cache->size);
}

bool cache_update(struct cache *cache, blkaddr_t addr, size_t offset,
		const char *buf, size_t len)
{
	size_t i;

	if (cache == NULL)
		return false;

#ifdef CACHE_STATS
	cache_updates++;
#endif

	assert(addr != INVALID_ADDR);
	assert(buf != NULL);

	for (i = 0; i < cache->count; i++) {
		if (addr == cache->table[i].addr) {
			memcpy(cache->table[i].buf + offset, buf, len);
			return true;
		}
	}
	return false;
}

#ifdef CACHE_STATS
void cache_print_stats(void)
{
	printf("Searches (successful) (percent): %lu (%lu) (%lf%%)\n",
			cache_searches,
			cache_successful_searches,
			(double)cache_successful_searches / cache_searches);
	printf("Pushes: %lu\n", cache_pushes);
	printf("Updates: %lu\n", cache_updates);
}
#endif

