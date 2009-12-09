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
 * Implementation of relational algebra. The `atomar' expressible relation 
 * is a wrapper of a stored relation. Expressible relations can be combined
 * using the relation operations `selection', `projection', `union' and 
 * `join'. The result of all these operations is again an expressible 
 * relation.
 */

#ifndef __RLALG_H__
#define __RLALG_H__

#include "btree.h"
#include "io.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#define ATTR_TO_VAL	1
#define ATTR_TO_ATTR	2

struct xrel { /* expressible relation */
	int		rl_type;	/* SREL_WRAPPER, CART_PROD, ... */
	void		*rl_rls[2];	/* the parent relation(s); normally
					 * xrels, only SREL_WRAPPERs have 
					 * srel structures as parents. */
	size_t		rl_size;	/* size of a tuple (not aligned) */
	unsigned short	rl_atcnt;	/* attribute count */
	struct xattr	**rl_attrs;	/* attributes of expressible relation */
	unsigned short	rl_excnt;	/* expression count */
	struct xexpr	**rl_exprs;	/* expressions (for SELECTIONs) */
	unsigned short	rl_srtcnt;	/* number order attributes */
	struct xattr	**rl_srtattrs;	/* attrs by which is ordered, subset
					 * of rl_attrs */
	int		*rl_srtorders;	/* the orders ASCENDING or DESCENDING */
	struct xrel_iter *(*rl_iterator)(struct xrel *); /* iterator */
	struct xrel_iter *(*rl_ix_iterator)(struct xrel *, struct xattr *,
				int compar, const char *); /* iterator on
							    * index of xattr */
};

struct xattr { /* attribute of an expressible relation rl */
	struct xrel	*at_pxrl;	/* pointer to parent xrel (if this 
					 * xattr a is an attribute of rl, 
					 * a->at_pxrl == rl->rl_rls[i] for 
					 * i=1,2) */
	struct xattr	*at_pxattr;	/* pointer to parent xattr (if this 
					 * xattr a is an attribute of rl,
					 * a->at_pxattr == rl->rl_rls[i]
					 * ->at_attrs[j] for i=1,2, j=1,...) */
	struct srel	*at_srl;	/* pointer to stored relation */
	struct sattr	*at_sattr;	/* pointer to stored relation's attr */
	size_t		at_offset;	/* offset in relation rl */
	struct index	*at_ix;		/* ponter to index structure or NULL */
};

struct xexpr { /* expression of an expressible relation */
	int		ex_type;	/* ATTR_TO_VAL for SELECTIONs,
					 * ATTR_TO_ATTR for CART_PRODs */
	struct xattr	*ex_left_attr;	/* left attribute */
	int		ex_compar;	/* EQ, GEQ, ... */
	struct xattr	*ex_right_attr;	/* right attribute (if ATTR_TO_ATTR) */
	void		*ex_right_val;	/* comparison value (if ATTR_TO_VAL) */
};

struct xrel_iter { /* iteratore over expressible relation */
	struct xrel	*it_rl;			/* expressible relation */
	int		it_state;		/* state (for internal use) */
	int		it_compar;		/* comparison relation */
	char		*it_tpbuf;		/* buffer (for internal use) */
	FILE		*it_fp;			/* buf-file (for SORT only) */
	struct xattr	*it_scanattr;		/* corresponding to ixattr
						 * (indexed iterators only) */
	struct xattr	*it_ixattr;		/* corresponding to scanattr
						 * (indexed iterators only) */
	void		*it_iter[2];		/* parent iterator(s): normally
						 * xrel_iters, only
						 * SREL_WRAPPERs have srel_iter
						 * or ix_iter iterators */
	void (*it_free_iter[2])(void *);	/* frees the parent iterators */
	const char *(*it_next)(struct xrel_iter *);/* next tuple or NULL */
	void (*it_reset)(struct xrel_iter *);	/* resets the iterator */
};

/* Frees the memory allocated by a xrel structure and all its son-relations. */
void xrel_free(struct xrel *rl);

/* Frees the memory allocated by an iterator of an expressible relation. */
void xrel_iter_free(struct xrel_iter *iter);

/* Initializes an xrel structure (expressible relation) that wraps an srel
 * structure (stored relation. */
struct xrel *wrapper_init(struct srel *srl);

/* Calculates the cartesian product of two relations r and s. If excnt != 0,
 * only those tuples are in the result relation that fulfill the expressions.
 * The expressions may only be of the type ATTR_TO_ATTR, which means that they
 * compare two attributes with another (more exactly speaking, two attributes'
 * values). */
struct xrel *join_init(struct xrel *r, struct xrel *s,
		struct xexpr **exprs, unsigned short excnt);

/* Creates a relation that contains selected tuples of the relation r. 
 * These tuples fulfill the expressions exprs. */
struct xrel *selection_init(struct xrel *r, struct xexpr **exprs,
		unsigned short excnt);

/* Creates a new relation based on relation r limited to the specified 
 * attributes attrs. Other attributes of r are skipped. */
struct xrel *projection_init(struct xrel *r, struct xattr **attrs,
		unsigned short atcnt);

/* Creates a concatenation of two expressible relations. */
struct xrel *union_init(struct xrel *r, struct xrel *s);

/* Creates a sorted relation out of an unsorted relation. */
struct xrel *sort_init(struct xrel *r, struct xattr **srtattrs, int *srtorders, 
		unsigned short srtcnt);

#endif

