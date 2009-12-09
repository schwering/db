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
 * Expression evaluation utilities.
 * Expressions are normally stored as tree.
 * The formula_to_dnf() function implements the Quine-McCluskey algorithm that
 * converts a formula (given as tree) into disjunctive normal form.
 */

#ifndef __EXPR_H__
#define __EXPR_H__

#include "dml.h"
#include <stdbool.h>

#define INNER	1
#define LEAF	2

#define SON_EXPR	1
#define SON_ATTR	2
#define SON_SATTR	3
#define SON_VALUE	4

struct expr { /* vertex and root element of expression tree */
	int		type;		/* either INNER or LEAF */
	enum operator	op;		/* the operator */
	int		stype[2];	/* son type */
	union {
		struct expr *expr;
		struct attr *attr;
		struct sattr *sattr;
		struct value *value;
	} sons[2];		/* the left (0) and right (1) children */
};

/* Initialize the expression tree. This means replacing dml_attrs in the leafs 
 * with pointers to the relation's attrs. */
bool expr_init(struct expr *expr);

/* Converts a formula into a disjunctive normal form, which is returned as 
 * three dimensional array. The first dimension contains pointers to the 
 * conjunctions; the second dimension (the conjunctions) contain pointers to 
 * the expressions. The expressions are newly malloc()ed in this function, 
 * but NOT the expressions' sons! All arrays are NULL-terminated. */
struct expr ***formula_to_dnf(struct expr *root);

/* Returns true if tuple fulfills all expressions in exprs. */
bool expr_check(const char *tuple, struct expr **exprs, int excnt);

/* Frees the memory allocated for a tree, including the memory used by 
 * the dml_val->va_ptr object. */
void expr_tree_free(struct expr *expr);

#ifndef NDEBUG
/* Debugging functions for drawing DNFs respectively expression trees. */
void draw_dnf(char *name, struct expr ***dnf);
void draw_expr_tree(char *name, struct expr *root);
#endif

#endif

