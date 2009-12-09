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
 * Attribute utilities.
 */

#ifndef __ATTR_H__
#define __ATTR_H__

#include "dml.h"
#include "io.h"
#include <stdbool.h>

/* Comparison function for two attribute values. Used for the B+-Tree 
 * indexes. */
typedef int (*cmpf_t)(const char *, const char *, size_t);

/* Returns a comparison function for a attribute. NOTE: The returned
 * function is for tuple-comparisons. See ixcmpf_by_sattr(). */
cmpf_t cmpf_by_sattr(struct sattr *attr);

/* Returns a comparison function for a attribute. NOTE: The returned 
 * function is for index's use only! Because index comparision functions
 * distinguish between PRIMARY and SECONDARY indices, the behaviour of 
 * a secondary indexed attribute comparison function might be 
 * inapplicable for normal in-tuple comparison. See cmpf_by_sattr(). */
cmpf_t ixcmpf_by_sattr(struct sattr *attr);

/* Sets a value in a tuple. */
void set_sattr_val(char *tuple, struct sattr *sattr, struct value *value);

/* Looks up a sattr structure by a given srel structure and an attribute 
 * name. */
struct sattr *sattr_by_srl_and_attr_name(struct srel *srl, char *attr_name);

/* Looks up a sattr structure by a given attr structure, whose fields 
 * attr_name and tbl_name are initialized. */
struct sattr *sattr_by_attr(struct attr *attr);

#endif

