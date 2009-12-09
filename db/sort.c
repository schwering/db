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

#include "sort.h"
#include "attr.h"
#include "block.h"
#include "err.h"
#include "mem.h"
#include "rlalg.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

/* the size of the first runs; this amount of tuples is read from the relation,
 * sorted and then dumped into the temporary `tape' files which then are 
 * merged multiple times */
#define FIRST_RUN_SIZE		3

/* the count of temporary `tape' files used; this should be an even number,
 * initally there are FILES_MAX/2 source and FILES_MAX/2 destination files
 * created which then alternate their roles after each merge_all_runs()
 * invokation */
#define FILES_MAX		4

#define READ(fp, ptr, size)	((bool)(fread(ptr, sizeof(char), size, fp)\
					== size))
#define WRITE(fp, ptr, size)	((bool)(fwrite(ptr, sizeof(char), size, fp)\
					== size))

enum {
	S_EMPTY,
	S_FULL
};

struct sort_ctx {
	struct xrel	*sc_rl;
	struct xattr	**sc_attrs;
	int		*sc_orders;
	size_t		sc_atcnt;
};

struct file {
	FILE	*f_fp;
	char	*f_buf;
	char	f_bufstatus;
	size_t	f_runindex;
	tpcnt_t	f_tpindex;
	tpcnt_t	f_tpcnt;
};

static inline void swap(void **arr, int i, int j)
{
	void *t;

	t = arr[i];
	arr[i] = arr[j];
	arr[j] = t;
}

static int tpcmp(const char *tp1, const char *tp2, const struct sort_ctx *ctx)
{
	cmpf_t cmpf;
	size_t size;
	const char *v1, *v2;
	size_t i;
	int r;

	assert(tp1 != NULL);
	assert(tp2 != NULL);
	assert(ctx != NULL);
	assert(ctx->sc_attrs != NULL);
	assert(ctx->sc_orders != NULL);
	assert(ctx->sc_atcnt > 0);

	for (i = 0; i < ctx->sc_atcnt; i++) {
		cmpf = cmpf_by_sattr(ctx->sc_attrs[i]->at_sattr);
		v1 = tp1 + ctx->sc_attrs[i]->at_offset;
		v2 = tp2 + ctx->sc_attrs[i]->at_offset;
		size = ctx->sc_attrs[i]->at_sattr->at_size;

		if ((r = cmpf(v1, v2, size)) != 0)
			return ctx->sc_orders[i] == ASCENDING ? r : -r;
	}
	/* from the perspective of simple sorting, we now could return zero.
	 * but xrel_sort() is also inteded to filter duplicates. therefore 
	 * we should also consider those attributes we don't want to sort by,
	 * because the effect is that two completely (!) equal tuples are
	 * next to another. */
	return memcmp(tp1, tp2, ctx->sc_rl->rl_size);
}

static void sort_tps(char **tuples, size_t cnt, const struct sort_ctx *ctx)
{
	size_t i, j, m;

	for (i = 0; i < cnt; i++) {
		for (m = i, j = i+1; j < cnt; j++)
			if (tpcmp(tuples[j], tuples[m], ctx) < 0)
				m = j;
		swap((void **)tuples, i, m);
	}
}

static bool write_run(struct file *file, struct xrel_iter *iter, char **buf,
		size_t runsize, const struct sort_ctx *ctx)
{
	const char *tp;
	size_t i;

	tp = NULL;
	for (i = 0; i < runsize && (tp = iter->it_next(iter)) != NULL; i++)
		memcpy(buf[i], tp, ctx->sc_rl->rl_size);
	assert(tp != NULL || iter->it_next(iter) == NULL);
	if ((runsize = i) == 0)
		return false;

	sort_tps(buf, runsize, ctx);

	for (i = 0; i < runsize; i++) {
		if (i > 0 && memcmp(buf[i-1], buf[i], ctx->sc_rl->rl_size) == 0)
			continue; /* skip dupe */
		if (!WRITE(file->f_fp, buf[i], ctx->sc_rl->rl_size))
			return false;
		file->f_tpcnt++;
	}
	return true;
}

static char *get_min(struct file **src, int srccnt,
		size_t runsize, const struct sort_ctx *ctx)
{
	int i, m;

	m = -1;
	for (i = 0; i < srccnt; i++) {
		if (src[i]->f_tpindex == src[i]->f_tpcnt) /* whole file read */
			continue;

		if (src[i]->f_runindex == runsize) /* whole run read */
			continue;

		if (src[i]->f_bufstatus == S_EMPTY) {
			if (READ(src[i]->f_fp, src[i]->f_buf,
						ctx->sc_rl->rl_size)) {
				src[i]->f_bufstatus = S_FULL;
			}
		}

		if (src[i]->f_bufstatus == S_EMPTY) /* file at end */
			continue;

		if (m == -1 || tpcmp(src[i]->f_buf, src[m]->f_buf, ctx) < 0)
			m = i;
		else if (memcmp(src[i]->f_buf,src[m]->f_buf,
					ctx->sc_rl->rl_size) == 0)
			src[i]->f_bufstatus = S_EMPTY; /* skip dupe */
	}

	if (m != -1) {
		src[m]->f_bufstatus = S_EMPTY;
		src[m]->f_runindex++;
		src[m]->f_tpindex++;
		return src[m]->f_buf;
	} else
		return NULL;
}

static bool merge_runs(struct file **src, int srccnt, struct file *dst,
		size_t runsize, const struct sort_ctx *ctx)
{
	char *tp;
	bool retval;
	int i;

	for (i = 0; i < srccnt; i++) {
		src[i]->f_bufstatus = S_EMPTY;
		src[i]->f_runindex = 0;
	}

	retval = false;
	while ((tp = get_min(src, srccnt, runsize, ctx)) != NULL) {
		if (!WRITE(dst->f_fp, tp, ctx->sc_rl->rl_size))
			return false;
		dst->f_tpcnt++;
		retval = true;
	}
	return retval;
}

static size_t merge_all_runs(struct file **src, size_t srccnt,
		struct file **dst, size_t dstcnt,
		size_t runsize, const struct sort_ctx *ctx)
{
	size_t i;

	for (i = 0; i < srccnt; i++) {
		rewind(src[i]->f_fp);
		src[i]->f_tpindex = 0;
	}
	for (i = 0; i < dstcnt; i++) {
		rewind(dst[i]->f_fp);
		dst[i]->f_tpcnt = 0;
	}

	i = 0;
	while (merge_runs(src, srccnt, dst[i % srccnt], runsize, ctx))
		i++;
	return (i < srccnt) ? i : srccnt;
}

FILE *xrel_sort(struct xrel *rl, struct xrel_iter *iter,
		struct xattr **attrs, int *orders, int atcnt)
{
	struct file **src, **dst;
	size_t i, cnt, srccnt, dstcnt, runsize;
	char **buf;
	struct sort_ctx ctx;
	FILE *fp;

	assert(rl != NULL);

	ctx.sc_rl = rl;
	ctx.sc_attrs = attrs;
	ctx.sc_orders = orders;
	ctx.sc_atcnt = atcnt;

	runsize = FIRST_RUN_SIZE;

	src = xmalloc(FILES_MAX / 2 * sizeof(struct file *));
	srccnt = FILES_MAX / 2;
	dst = xmalloc(FILES_MAX / 2 * sizeof(struct file *));
	dstcnt = FILES_MAX / 2;

	for (i = 0; i < srccnt; i++) {
		src[i] = xmalloc(sizeof(struct file));
		src[i]->f_fp = tmpfile();
		src[i]->f_tpcnt = 0;
	}
	for (i = 0; i < dstcnt; i++) {
		dst[i] = xmalloc(sizeof(struct file));
		dst[i]->f_fp = tmpfile();
		dst[i]->f_tpcnt = 0;
	}

	/* distribute relation in runs over files */
	buf = xmalloc(runsize * sizeof(char *));
	for (i = 0; i < runsize; i++)
		buf[i] = xmalloc(ctx.sc_rl->rl_size);

	i = 0;
	while (write_run(src[i], iter, buf, runsize, &ctx))
		i = (i+1) % srccnt;

	for (i = 0; i < runsize; i++)
		free(buf[i]);
	free(buf);

	/* merge the runs */
	for (i = 0; i < srccnt; i++)
		src[i]->f_buf = xmalloc(ctx.sc_rl->rl_size);
	for (i = 0; i < dstcnt; i++)
		dst[i]->f_buf = xmalloc(ctx.sc_rl->rl_size);

	while ((cnt = merge_all_runs(src, srccnt, dst, dstcnt, runsize, &ctx))
			> 1) {
		struct file **tmp;

		dstcnt = srccnt;
		srccnt = cnt;
		tmp = dst;
		dst = src;
		src = tmp;
		runsize *= 2;
	}

	fp = NULL;
	for (i = 0; i < FILES_MAX / 2; i++) {
		fclose(src[i]->f_fp);
		free(src[i]->f_buf);
		free(src[i]);
	}
	free(src);
	for (i = 0; i < FILES_MAX / 2; i++) {
		if (i == 0)
			fp = dst[i]->f_fp;
		else
			fclose(dst[i]->f_fp);
		free(dst[i]->f_buf);
		free(dst[i]);
	}
	free(dst);
	rewind(fp);
	return fp;
}

void selection_sort(void **arr, int len,
		int (*cmp)(const void *p, const void *q))
{
	int i, j, m;

	for (i = 0; i < len; i++) {
		for (m = i, j = i+1; j < len; j++)
			if (cmp(arr[j], arr[m]) < 0)
				m = j;
		swap(arr, i, m);
	}
}

void bubble_sort(void **arr, int len,
		int (*cmp)(const void *p, const void *q))
{
	int i, j;

	for (i = len-1; i >= 0; i--)
		for (j = 1; j <= i; j++) {
			if (cmp(arr[j-1], arr[j]) > 0)
				swap(arr, j-1, j);
		}
}

