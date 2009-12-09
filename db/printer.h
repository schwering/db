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
 * Provides a function that prints a relation to stdout.
 */

#ifndef __PRINTER_H__
#define __PRINTER_H__

#include "block.h"
#include "rlalg.h"
#include <stdio.h>

/* Prints an expressible relation with a nice layout to out. 
 * Returns the count of printed tuples. */
tpcnt_t xrel_fprint(FILE *out, struct xrel *rl);

/* Prints an expressible relation with a nice layout to stdout.
 * Returns the count of printed tuples. */
tpcnt_t xrel_print(struct xrel *rl);

#endif

