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
 * Sorting algorithms.
 * This file contains very simple in-memory sorting algorithms for sorting
 * arrays of about 10 elements (e.g. expressions).
 * The xrel_sort() function impelements external sorting in shape of 
 * balanced two-way merging. xrel_sort() is used to sort an entire 
 * relation. While sorting tuples, xrel_sort() also filters duplicate 
 * tuples.
 *
 * The used balanced two-way merging is described in
 * Donald Knuth. The Art of Computer Programming, Volume 3: Sorting and 
 * Searching. Section 5.4: External Sorting, pp. 248 - 251
 */

#ifndef __SORT_H__
#define __SORT_H__

#include "rlalg.h"
#include <stdio.h>

#define ASCENDING	1
#define DESCENDING	2

/* Sorts a relation. */
FILE *xrel_sort(struct xrel *rl, struct xrel_iter *iter,
		struct xattr **attrs, int *orders, int atcnt);

/* Selection sort. */
void selection_sort(void **arr, int len,
		int (*cmp)(const void *p, const void *q));

/* Bubble sort. */
void bubble_sort(void **arr, int len,
		int (*cmp)(const void *p, const void *q));

#endif

