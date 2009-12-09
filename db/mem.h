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
 * Wrappers for malloc(), calloc(), realloc(), free() functions.
 * These functions are very useful to search memory leaks.
 */

#ifndef __MEM_H__
#define __MEM_H__

#include <stdlib.h>

#if defined(MEMDEBUG) && defined(MALLOC_TRACE)
#error "either MEMDEBUG or MALLOC_TRACE, but not both"
#endif

void *xmalloc(size_t size);
void *xcalloc(size_t nmemb, size_t size);
void *xrealloc(void *ptr, size_t size);

typedef int mid_t;
mid_t gnew(void);

#ifdef MEMDEBUG

#define gnew()			wgnew(__FILE__,__FUNCTION__,__LINE__)
#define gmalloc(size,id)	wgmalloc(size,id,__FILE__,__FUNCTION__,\
				__LINE__)
#define gcalloc(nmemb,size,id)	wgcalloc(nmemb,size,id,__FILE__,__FUNCTION__,\
				__LINE__)
#define grealloc(ptr,size,id)	wgrealloc(ptr,size,id,__FILE__,__FUNCTION__,\
				__LINE__)
#define gfree(ptr,id)		wgfree(ptr,id,__FILE__,__FUNCTION__,__LINE__)
#define gc(id)			wgc(id,__FILE__,__FUNCTION__,__LINE__)

#define xmalloc(size)		wxmalloc(size, __FILE__, __FUNCTION__,\
				__LINE__)
#define xcalloc(nmemb,size)	wxcalloc(nmemb, size, __FILE__, __FUNCTION__,\
				__LINE__)
#define xrealloc(ptr,size)	wxrealloc(ptr, size, __FILE__, __FUNCTION__,\
				__LINE__)
#define free(ptr)		wfree(ptr, __FILE__, __FUNCTION__, __LINE__)

mid_t wgnew(const char *file, const char *function, int line);
void *wgmalloc(size_t size, mid_t id, const char *file, const char *function,
		int line);
void *wgcalloc(size_t nmemb, size_t size, mid_t id, const char *file,
		const char *function, int line);
void *wgrealloc(void *ptr, size_t size, mid_t id, const char *file,
		const char *function, int line);
void wgfree(void *ptr, mid_t id, const char *file, const char *function,
		int line);
void wgc(mid_t id, const char *file, const char *function, int line);

void *wxmalloc(size_t size, const char *file, const char *function, int line);
void *wxcalloc(size_t nmemb, size_t size, const char *file,
		const char *function, int line);
void *wxrealloc(void *ptr, size_t size, const char *file,
		const char *function, int line);
void wfree(void *ptr, const char *file, const char *function, int line);
void memprint(void);

#else

void *gmalloc(size_t size, mid_t id);
void *gcalloc(size_t nmemb, size_t size, mid_t id);
void *grealloc(void *ptr, size_t size, mid_t id);
void gfree(void *ptr, mid_t id);
void gc(mid_t id);

#endif

#endif

