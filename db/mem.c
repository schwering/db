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

#include "mem.h"
#include "hashset.h"
#include "str.h"
#include <assert.h>
#include <stdio.h>

#ifdef MALLOC_TRACE
#undef xmalloc
#undef xrealloc
#undef xcalloc
#endif

#ifdef MEMDEBUG

#include "hashtable.h"
#include "sort.h"
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>

#undef gmalloc
#undef gcalloc
#undef grealloc
#undef gfree

#undef xmalloc
#undef xrealloc
#undef xcalloc
#undef free

#define FILE_LEN	32
#define FUNCTION_LEN	32

struct memrec {
	void *ptr;
	size_t size;
	char file[FILE_LEN+1];
	char function[FUNCTION_LEN+1];
	int line;
};

static struct hashtable *table = NULL;
static const char *peak_function = NULL;
static const char *peak_file = NULL;
static int peak_line = -1;
static size_t mem_peak = 0;
static size_t mem_total = 0;
static size_t mem_cur = 0;
static int malloc_calls = 0;
static int calloc_calls = 0;
static int realloc_calls = 0;
static int free_calls = 0;
static int invalid_free_calls = 0;

#endif

static int ptr_hashf(void *p)
{
	return (int)p;
}

static bool ptr_equalsf(void *p, void *q)
{
	return p == q;
}

int chunk_cnt = 0;
struct hashset **chunks = NULL;

#ifdef MEMDEBUG
void *wgmalloc(size_t size, mid_t id, const char *file,
		const char *function, int line)
#else
void *gmalloc(size_t size, mid_t id)
#endif
{
	void *ptr;

	assert(id >= 0 && id < chunk_cnt);
	assert(chunks[id] != NULL);

#ifdef MEMDEBUG
	ptr = wxmalloc(size, file, function, line);
#else
	ptr = xmalloc(size);
#endif
	if (ptr == NULL)
		return NULL;
	hashset_insert(chunks[id], ptr);
	return ptr;
}

#ifdef MEMDEBUG
void *wgcalloc(size_t nmemb, size_t size, mid_t id, const char *file,
		const char *function, int line)
#else
void *gcalloc(size_t nmemb, size_t size, mid_t id)
#endif
{
	void *ptr;

	assert(id >= 0 && id < chunk_cnt);
	assert(chunks[id] != NULL);

#ifdef MEMDEBUG
	ptr = wxcalloc(nmemb, size, file, function, line);
#else
	ptr = xcalloc(nmemb, size);
#endif
	if (ptr == NULL)
		return NULL;
	hashset_insert(chunks[id], ptr);
	return ptr;
}

#ifdef MEMDEBUG
void *wgrealloc(void *ptr, size_t size, mid_t id, const char *file,
		const char *function, int line)
#else
void *grealloc(void *ptr, size_t size, mid_t id)
#endif
{
	void *nptr;

	assert(id >= 0 && id < chunk_cnt);
	assert(chunks[id] != NULL);

#ifdef MEMDEBUG
	nptr = wxrealloc(ptr, size, file, function, line);
#else
	nptr = xrealloc(ptr, size);
#endif
	if (nptr == NULL)
		return NULL;
	if (ptr != NULL)
		hashset_delete(chunks[id], ptr);
	if (ptr != nptr)
		hashset_insert(chunks[id], nptr);
	return nptr;
}

#ifdef MEMDEBUG
void wgfree(void *ptr, mid_t id, const char *file, const char *function,
		int line)
#else
void gfree(void *ptr, mid_t id)
#endif
{
	assert(id >= 0 && id < chunk_cnt);

#ifdef MEMDEBUG
	wfree(ptr, file, function, line);
#else
	free(ptr);
#endif
	hashset_delete(chunks[id], ptr);
}

#ifdef MEMDEBUG
void wgc(mid_t id, const char *file, const char *function, int line)
#else
void gc(mid_t id)
#endif
{
	void **tab;
	int i;

	assert(id >= 0 && id < chunk_cnt);

	tab = hashset_entries(chunks[id]);
	for (i = 0; i < chunks[id]->used; i++)
		free(tab[i]);
	free(tab);
	hashset_free(chunks[id]);
	chunks[id] = NULL;
	if (id == chunk_cnt-1) {
		while (chunk_cnt > 0 && chunks[chunk_cnt-1] == NULL)
			chunk_cnt--;
		/* if chunk_cnt == 0, realloc() is equivalent to free() */
#ifdef MEMDEBUG
		chunks = wxrealloc(chunks, chunk_cnt * sizeof(struct hashset *),
				file, function, line);
#else
		chunks = xrealloc(chunks, chunk_cnt * sizeof(struct hashset *));
#endif
	}
#ifdef MEMDEBUG
	fprintf(stderr, "%s(): id=%d from %s:%d %s()\n", __FUNCTION__, id,
			file, line, function);
#endif
}

#ifdef MEMDEBUG
mid_t wgnew(const char *file, const char *function, int line)
#else
mid_t gnew(void)
#endif
{
	mid_t id;
	struct hashset **tmp_chunks;

	for (id = 0; id < chunk_cnt; id++) {
		if (chunks[id] == NULL) {
			chunks[id] = hashset_init(7, ptr_hashf, ptr_equalsf);
			return id;
		}
	}

	chunk_cnt++;
	/* if chunks == NULL, realloc() is equivalent to malloc() */
	tmp_chunks = realloc(chunks, chunk_cnt * sizeof(struct hashset *));
	if (!tmp_chunks) {
		free(chunks);
		return -1;
	}
	chunks = tmp_chunks;
	chunks[chunk_cnt-1] = hashset_init(7, ptr_hashf, ptr_equalsf);
#ifdef MEMDEBUG
	fprintf(stderr, "%s(): id=%d from %s:%d %s()\n", __FUNCTION__, id,
			file, line, function);
#endif
	return (mid_t)(chunk_cnt-1);
}



#ifdef MEMDEBUG
static bool memrec_equalsf(void *p, void *q)
{
	return ((struct memrec *)p)->ptr == ((struct memrec *)p)->ptr;
}
#endif

#ifdef MEMDEBUG
static void init_table(void)
{
	if (table == NULL)
		table = table_init(97, ptr_hashf, memrec_equalsf);
}
#endif

#ifdef MEMDEBUG
static void free_table(void)
{
	if (table != NULL) {
		table_free(table);
		table = NULL;
	}
}
#endif

#ifdef MEMDEBUG
static struct memrec *init_rec(void *ptr, size_t size, const char *file,
		const char *function, int line)
{
	struct memrec *rec;

	rec = malloc(sizeof(struct memrec));
	assert(rec != NULL);

	rec->ptr = ptr;
	rec->size = size;

	if (file != NULL) {
		strntermcpy(rec->file, file, FILE_LEN+1);
		rec->file[FILE_LEN] = '\0';
	} else
		rec->file[0] = '\0';

	if (function != NULL) {
		strntermcpy(rec->function, function, FUNCTION_LEN+1);
		rec->function[FUNCTION_LEN] = '\0';
	} else
		rec->function[0] = '\0';

	rec->line = line;
	return rec;
}
#endif

#ifdef MEMDEBUG
void *wxmalloc(size_t size, const char *file, const char *function, int line)
{
	void *ptr;
	struct memrec *rec;

	if (table == NULL)
		init_table();

	ptr = malloc(size);
	malloc_calls++;
	mem_cur += size;
	mem_total += size;
	if (mem_cur > mem_peak) {
		mem_peak = mem_cur;
		peak_file = file;
		peak_line = line;
		peak_function = function;
	}

	rec = init_rec(ptr, size, file, function, line);
	table_insert(table, ptr, rec);
	return ptr;
}
#else
void *xmalloc(size_t size)
{
	void *val;

	if ((val = malloc(size)) == NULL) {
		fprintf(stderr, "%s(): Virtual memory exhausted.\n",
				__FUNCTION__);
		exit(1);
	}
	return val;
}
#endif

#ifdef MEMDEBUG
void *wxcalloc(size_t nmemb, size_t size, const char *file,
		const char *function, int line)
{
	void *ptr;
	struct memrec *rec;

	if (table == NULL)
		init_table();

	ptr = calloc(nmemb, size);
	calloc_calls++;
	mem_cur += nmemb * size;
	mem_total += nmemb * size;
	if (mem_cur > mem_peak) {
		mem_peak = mem_cur;
		peak_file = file;
		peak_line = line;
		peak_function = function;
	}

	rec = init_rec(ptr, nmemb * size, file, function, line);
	table_insert(table, ptr, rec);
	return ptr;
}
#else
void *xcalloc(size_t nmemb, size_t size)
{
	void *val;

	if ((val = calloc(nmemb, size)) == NULL) {
		fprintf(stderr, "%s(): Virtual memory exhausted.\n",
				__FUNCTION__);
		exit(1);
	}
	return val;
}
#endif

#ifdef MEMDEBUG
void *wxrealloc(void *ptr, size_t size, const char *file,
		const char *function, int line)
{
	void *nptr;
	struct memrec *rec;

	if (table == NULL)
		init_table();

	if (ptr == NULL)
		return wxmalloc(size, file, function, line);

	if (size == 0) {
		wfree(ptr, file, function, line);
		return NULL;
	}

	rec = table_delete(table, ptr);
	if (rec != NULL) {
		mem_cur -= rec->size;
		mem_total -= rec->size;
		free(rec);
	}

	nptr = realloc(ptr, size);
	realloc_calls++;
	mem_cur += size;
	mem_total += size;
	if (mem_cur > mem_peak) {
		mem_peak = mem_cur;
		peak_file = file;
		peak_line = line;
		peak_function = function;
	}

	rec = init_rec(nptr, size, file, function, line);
	table_insert(table, nptr, rec);
	return nptr;
}
#else
void *xrealloc(void *ptr, size_t size)
{
	void *val;

	if ((val = realloc(ptr, size)) == NULL && size > 0) {
		fprintf(stderr, "%s(): Virtual memory exhausted.\n",
				__FUNCTION__);
		exit(1);
	}
	return val;
}
#endif

#ifdef MEMDEBUG
void wfree(void *ptr, const char *file, const char *function, int line)
{
	struct memrec *rec;

	if (table == NULL) {
		printf("!!! Possibly invalid free() of 0x%x in %s() (%s:%d)\n",
				(unsigned int)ptr, function, file, line);
		invalid_free_calls++;
		return;
	}

	rec = table_delete(table, ptr);
	if (rec != NULL) {
		mem_cur -= rec->size;
		free(rec);
	} else {
		printf("!!! Possibly invalid free() of 0x%x in %s() (%s:%d)\n",
				(unsigned int)ptr, function, file, line);
		invalid_free_calls++;
	}

	free(ptr);
	free_calls++;

	if (table->used == 0)
		free_table();
}
#endif

#ifdef MEMDEBUG
static int memreccmp(const struct memrec *m, const struct memrec *n, size_t s)
{
	int cmpval;

	if ((cmpval = strncmp(m->file, n->file, FILE_LEN)) != 0)
		return cmpval;
	else
		return m->line - n->line;
}
#endif

#ifdef MEMDEBUG
void memprint(void)
{
	int i, cnt;
	struct memrec **recs;

	if (table == NULL) {
		printf("(No memory information available)\n");
		return;
	}

	recs = (struct memrec **)table_entries(table);
	for (cnt = 0; recs[cnt]; cnt++)
		;

	selection_sort(recs, cnt,
			(int (*)(const void *, const void *))memreccmp);

	for (i = 0; i < cnt; i++) {
		printf("#%d\t", i);
		printf("0x%x \t", (unsigned int)recs[i]->ptr);
		printf("%u\t", (unsigned int)recs[i]->size);
		printf("%s:%d\t", recs[i]->file, recs[i]->line);
		printf("%s()\n", recs[i]->function);
	}
	free(recs);

	printf("total allocated memory:        %u\n", (unsigned int)mem_total);
	printf("maximum allocated memory:      %u (in %s() (%s:%d))\n",
			(unsigned int)mem_peak, peak_function,
			peak_file, peak_line);
	printf("currently allocated memory:    %u\n", (unsigned int)mem_cur);
	printf("malloc() calls:                %d\n", malloc_calls);
	printf("calloc() calls:                %d\n", calloc_calls);
	printf("realloc() calls:               %d\n", realloc_calls);
	printf("free() calls:                  %d (invalid: %d)\n", free_calls,
			invalid_free_calls);
}
#endif

