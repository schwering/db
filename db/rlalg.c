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

#include "rlalg.h"
#include "err.h"
#include "ixmngt.h"
#include "mem.h"
#include "constants.h" /* INT, .., EQ, GEQ, ... */
#include "sort.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
	SREL_WRAPPER,
	UNION,
	PROJECTION,
	JOIN,
	SELECTION,
	SORT
};

static inline struct xrel *other_xrel(struct xrel *rl, struct xrel *r)
{
	assert(rl->rl_rls[0] == r || rl->rl_rls[1] == r);

	if (rl->rl_rls[0] == r)
		return rl->rl_rls[1];
	else
		return rl->rl_rls[0];
}

static struct xattr *better_xattr(struct xattr *a, struct xattr *b)
{
	if (a->at_ix != NULL && b->at_ix != NULL) {
		if (a->at_sattr->at_indexed == PRIMARY
				&& b->at_sattr->at_indexed != PRIMARY)
			return a;
		else if (a->at_sattr->at_indexed != PRIMARY
				&& b->at_sattr->at_indexed == PRIMARY)
			return b;
		else if (a->at_sattr->at_size < b->at_sattr->at_size)
			return a;
		else
			return b;
	} else if (a->at_ix != NULL && b->at_ix == NULL)
		return a;
	else if (a->at_ix == NULL && b->at_ix != NULL)
		return b;
	else if (a->at_sattr->at_size < b->at_sattr->at_size)
		return a;
	else
		return b;
}

static inline struct xattr *other_xattr(struct xexpr *e, struct xattr *a)
{
	assert(e != NULL);
	assert(a != NULL);
	assert(e->ex_type == ATTR_TO_ATTR);
	assert(e->ex_left_attr == a || e->ex_right_attr == a);

	return (e->ex_left_attr == a) ? e->ex_right_attr : e->ex_left_attr;
}

static bool best_aa_xexpr(struct xrel *rl, struct xrel *prl, 
		struct xattr **ix_attr, int *compar, struct xattr **other_attr)
{
	struct xexpr *best_e;
	struct xattr *best_a;
	int i;

	assert(rl != NULL);

	best_e = NULL;
	best_a = NULL;
	for (i = 0; i < rl->rl_excnt; i++) {
		struct xexpr *e;
		struct xattr *a;

		e = rl->rl_exprs[i];
		assert(e->ex_type == ATTR_TO_ATTR);

		if (prl != NULL) {
			a = (e->ex_left_attr->at_pxrl == prl) ? e->ex_left_attr
				: e->ex_right_attr;
		} else {
			a = better_xattr(e->ex_left_attr, e->ex_right_attr);
		}
		assert(a != NULL);

		if (e->ex_type == NEQ || a->at_ix == NULL
				|| (best_e != NULL && e->ex_type != EQ
					&& best_e->ex_compar == EQ))
			continue;
		if (best_e == NULL
				|| (e->ex_compar == EQ
					&& best_e->ex_compar != EQ)
				|| better_xattr(a, best_a) == a) {
			best_e = e;
			best_a = a;
		}
	}

	if (best_e != NULL) {
		assert(best_a != NULL);

		if (compar != NULL) {
			if (best_a == best_e->ex_left_attr)
				*compar = best_e->ex_compar;
			else /* switch comparator */
				switch (best_e->ex_compar) {
					case EQ:	*compar = EQ; break;
					case LEQ:	*compar = GT; break;
					case LT:	*compar = GEQ; break;
					case GEQ:	*compar = LT; break;
					case GT:	*compar = LEQ; break;
				}
		}
		if (ix_attr != NULL)
			*ix_attr = best_a;
		if (other_attr != NULL)
			*other_attr = other_xattr(best_e, best_a);
		return true;
	} else 
		return false;
}

static bool best_av_xexpr(struct xrel *rl, struct xattr **ix_attr,
		int *compar, char **val)
{
	struct xexpr *best_e;
	int i;

	assert(rl != NULL);

	best_e = NULL;
	for (i = 0; i < rl->rl_excnt; i++) {
		struct xexpr *e;
		struct xattr *a;

		e = rl->rl_exprs[i];
		assert(e->ex_type == ATTR_TO_VAL);

		a = e->ex_left_attr;
		assert(a != NULL);

		if (e->ex_type == NEQ
				|| a->at_ix == NULL
				|| (best_e != NULL && e->ex_type != EQ
					&& best_e->ex_compar == EQ))
			continue;
		if (best_e == NULL
				|| (e->ex_compar == EQ
					&& best_e->ex_compar != EQ)
				|| better_xattr(a, best_e->ex_left_attr) == a)
			best_e = e;
	}

	if (best_e != NULL) {
		if (compar != NULL)
			*compar = best_e->ex_compar;
		if (ix_attr != NULL)
			*ix_attr = best_e->ex_left_attr;
		if (val != NULL)
			*val = best_e->ex_right_val;
		return true;
	} else 
		return false;
}

static bool xrel_has_xattr(struct xrel *rl, struct xattr *attr)
{
	unsigned short i, j;

	j = 0;
	for (i = 0; i < rl->rl_atcnt; i++)
		if (rl->rl_attrs[i] == attr)
			j++;
	return j == 1;
}

static bool compliant_xattrs(struct xattr *a, struct xattr *b)
{
	struct sattr *x, *y;

	assert(a != NULL);
	assert(b != NULL);

	x = a->at_sattr;
	y = b->at_sattr;

	return x->at_domain == y->at_domain && x->at_size == y->at_size;
}

static bool compliant_xrels(struct xrel *r, struct xrel *s)
{
	unsigned short i;

	assert(r != NULL);
	assert(s != NULL);

	if (r->rl_size != s->rl_size)
		return false;
	if (r->rl_atcnt != s->rl_atcnt)
		return false;
	for (i = 0; i < r->rl_atcnt; i++)
		if (!compliant_xattrs(r->rl_attrs[i], s->rl_attrs[i]))
			return false;
	return true;
}

static void tpcpy(char *dest, struct xrel *destrl, const char *src, 
		struct xrel *srcrl)
{
	size_t offset;

	assert(destrl->rl_rls[0] == srcrl || destrl->rl_rls[1] == srcrl);

	if (destrl->rl_rls[0] == srcrl)
		offset = 0;
	else
		offset = ((struct xrel *)destrl->rl_rls[0])->rl_size;

	memcpy(dest + offset, src, srcrl->rl_size);
}

static bool xattr_check(const char *tuple, struct xattr *attr, int oper,
		const void *val)
{
	const void *v1, *v2;

	assert(tuple != NULL);
	assert(attr != NULL);
	assert(val != NULL);

	v1 = tuple + attr->at_offset;
	v2 = val;

	if (attr->at_sattr->at_domain == INT) {
		switch (oper) {
			case EQ:
				return *(db_int_t *)v1 == *(db_int_t *)v2;
			case NEQ:
				return *(db_int_t *)v1 != *(db_int_t *)v2;
			case LEQ:
				return *(db_int_t *)v1 <= *(db_int_t *)v2;
			case GEQ:
				return *(db_int_t *)v1 >= *(db_int_t *)v2;
			case LT:
				return *(db_int_t *)v1 < *(db_int_t *)v2;
			case GT:
				return *(db_int_t *)v1 > *(db_int_t *)v2;
		}
	} else if (attr->at_sattr->at_domain == UINT) {
		switch (oper) {
			case EQ:
				return *(db_uint_t *)v1 == *(db_uint_t *)v2;
			case NEQ:
				return *(db_uint_t *)v1 != *(db_uint_t *)v2;
			case LEQ:
				return *(db_uint_t *)v1 <= *(db_uint_t *)v2;
			case GEQ:
				return *(db_uint_t *)v1 >= *(db_uint_t *)v2;
			case LT:
				return *(db_uint_t *)v1 < *(db_uint_t *)v2;
			case GT:
				return *(db_uint_t *)v1 > *(db_uint_t *)v2;
		}
	} else if (attr->at_sattr->at_domain == LONG) {
		switch (oper) {
			case EQ:
				return *(db_long_t *)v1 == *(db_long_t *)v2;
			case NEQ:
				return *(db_long_t *)v1 != *(db_long_t *)v2;
			case LEQ:
				return *(db_long_t *)v1 <= *(db_long_t *)v2;
			case GEQ:
				return *(db_long_t *)v1 >= *(db_long_t *)v2;
			case LT:
				return *(db_long_t *)v1 < *(db_long_t *)v2;
			case GT:
				return *(db_long_t *)v1 > *(db_long_t *)v2;
		}
	} else if (attr->at_sattr->at_domain == ULONG) {
		switch (oper) {
			case EQ:
				return *(db_ulong_t *)v1 == *(db_ulong_t *)v2;
			case NEQ:
				return *(db_ulong_t *)v1 != *(db_ulong_t *)v2;
			case LEQ:
				return *(db_ulong_t *)v1 <= *(db_ulong_t *)v2;
			case GEQ:
				return *(db_ulong_t *)v1 >= *(db_ulong_t *)v2;
			case LT:
				return *(db_ulong_t *)v1 < *(db_ulong_t *)v2;
			case GT:
				return *(db_ulong_t *)v1 > *(db_ulong_t *)v2;
		}
	} else if (attr->at_sattr->at_domain == FLOAT) {
		switch (oper) {
			case EQ:
				return *(db_float_t *)v1 == *(db_float_t *)v2;
			case NEQ:
				return *(db_float_t *)v1 != *(db_float_t *)v2;
			case LEQ:
				return *(db_float_t *)v1 <= *(db_float_t *)v2;
			case GEQ:
				return *(db_float_t *)v1 >= *(db_float_t *)v2;
			case LT:
				return *(db_float_t *)v1 < *(db_float_t *)v2;
			case GT:
				return *(db_float_t *)v1 > *(db_float_t *)v2;
		}
	} else if (attr->at_sattr->at_domain == DOUBLE) {
		switch (oper) {
			case EQ:
				return *(db_double_t *)v1 == *(db_double_t *)v2;
			case NEQ:
				return *(db_double_t *)v1 != *(db_double_t *)v2;
			case LEQ:
				return *(db_double_t *)v1 <= *(db_double_t *)v2;
			case GEQ:
				return *(db_double_t *)v1 >= *(db_double_t *)v2;
			case LT:
				return *(db_double_t *)v1 < *(db_double_t *)v2;
			case GT:
				return *(db_double_t *)v1 > *(db_double_t *)v2;
		}
	} else if (attr->at_sattr->at_domain == STRING) {
		switch (oper) {
			case EQ:
				return strcmp(v1, v2) == 0;
			case NEQ:
				return strcmp(v1, v2) != 0;
			case LEQ:
				return strcmp(v1, v2) <= 0;
			case GEQ:
				return strcmp(v1, v2) >= 0;
			case LT:
				return strcmp(v1, v2) < 0;
			case GT:
				return strcmp(v1, v2) > 0;
		}
	} else if (attr->at_sattr->at_domain == BYTES) {
		size_t s = attr->at_sattr->at_size;
		switch (oper) {
			case EQ:
				return memcmp(v1, v2, s) == 0;
			case NEQ:
				return memcmp(v1, v2, s) != 0;
			case LEQ:
				return memcmp(v1, v2, s) <= 0;
			case GEQ:
				return memcmp(v1, v2, s) >= 0;
			case LT:
				return memcmp(v1, v2, s) < 0;
			case GT:
				return memcmp(v1, v2, s) > 0;
		}
	}
	return false;
}

static bool xexpr_check(const char *tuple, struct xexpr **exprs,
		unsigned short excnt)
{
	unsigned short i;

	for (i = 0; i < excnt; i++) {
		struct xexpr *e;

		e = exprs[i];
		assert(e != NULL);

		if (e->ex_type == ATTR_TO_VAL) {
			if (!xattr_check(tuple, e->ex_left_attr, e->ex_compar,
						e->ex_right_val))
				return false;
		} else if (e->ex_type == ATTR_TO_ATTR) {
			struct xattr *l, *r;

			l = e->ex_left_attr;
			r = e->ex_right_attr;
			assert(l != NULL);
			assert(r != NULL);
			assert(compliant_xattrs(l, r));
			if (!xattr_check(tuple, l, e->ex_compar,
						tuple + r->at_offset))
				return false;
		}
	}
	return true;
}

void xrel_free(struct xrel *rl)
{
	if (rl != NULL) {
		if (rl->rl_attrs != NULL) {
			unsigned short i;

			for (i = 0; i < rl->rl_atcnt; i++)
				free(rl->rl_attrs[i]);
			free(rl->rl_attrs);
		}
		if (rl->rl_exprs != NULL) {
			unsigned short i;

			for (i = 0; i < rl->rl_excnt; i++)
				free(rl->rl_exprs[i]);
			free(rl->rl_exprs);
		}
		if (rl->rl_srtattrs != NULL)
			free(rl->rl_srtattrs);
		if (rl->rl_srtorders != NULL)
			free(rl->rl_srtorders);
		switch (rl->rl_type) {
			case SREL_WRAPPER:
				break;
			case SELECTION:
				xrel_free(rl->rl_rls[0]);
				break;
			case PROJECTION:
				xrel_free(rl->rl_rls[0]);
				break;
			case JOIN:
				xrel_free(rl->rl_rls[0]);
				xrel_free(rl->rl_rls[1]);
				break;
			case UNION:
				xrel_free(rl->rl_rls[0]);
				xrel_free(rl->rl_rls[1]);
				break;
			case SORT:
				xrel_free(rl->rl_rls[0]);
				break;
			default:
				assert(false);
		}
		free(rl);
	}
}

void xrel_iter_free(struct xrel_iter *iter)
{
	if (iter != NULL) {
		if (iter->it_free_iter[0] != NULL)
			(iter->it_free_iter[0])(iter->it_iter[0]);
		if (iter->it_free_iter[1] != NULL)
			(iter->it_free_iter[1])(iter->it_iter[1]);
		if (iter->it_tpbuf != NULL)
			free(iter->it_tpbuf);
		if (iter->it_fp != NULL)
			fclose(iter->it_fp);
		free(iter);
	}
}

static const char *wrapper_next(struct xrel_iter *iter)
{
	struct srel_iter *srel_iter;

	assert(iter != NULL);
	assert(iter->it_rl != NULL);
	assert(iter->it_rl->rl_type == SREL_WRAPPER);
	assert(iter->it_iter[0] != NULL);

	srel_iter = iter->it_iter[0];
	return rl_next(srel_iter);
}

static void wrapper_reset(struct xrel_iter *iter)
{
	struct srel_iter *srel_iter;

	assert(iter != NULL);
	assert(iter->it_rl != NULL);
	assert(iter->it_rl->rl_type == SREL_WRAPPER);
	assert(iter->it_iter[0] != NULL);

	srel_iter = iter->it_iter[0];
	rl_iterator_reset(srel_iter);
}

static struct xrel_iter *wrapper_iterator(struct xrel *rl)
{
	struct xrel_iter *iter;
	struct srel_iter *srel_iter;

	assert(rl != NULL);
	assert(rl->rl_type == SREL_WRAPPER);

	iter = xmalloc(sizeof(struct xrel_iter));
	iter->it_rl = rl;
	iter->it_state = 0;
	iter->it_tpbuf = NULL;
	iter->it_fp = NULL;

	srel_iter = rl_iterator(rl->rl_rls[0]);
	assert(srel_iter != NULL);

	iter->it_iter[0] = srel_iter;
	iter->it_free_iter[0] = (void (*)(void *))srel_iter_free;

	iter->it_iter[1] = NULL;
	iter->it_free_iter[1] = NULL;

	iter->it_next = wrapper_next;
	iter->it_reset = wrapper_reset;
	return iter;
}

static const char *wrapper_ix_next(struct xrel_iter *iter)
{
	struct ix_iter *ix_iter;
	struct srel *srl;
	blkaddr_t addr;
	const char *tuple;
	blkaddr_t (*nextf)(struct ix_iter *);

	assert(iter != NULL);
	assert(iter->it_rl != NULL);
	assert(iter->it_rl->rl_type == SREL_WRAPPER);
	assert(iter->it_iter[0] != NULL);

	ix_iter = (struct ix_iter *)iter->it_iter[0];
	nextf = index_iterator_nextf(iter->it_compar);
	assert(nextf != NULL);
	if ((addr = nextf(ix_iter)) == INVALID_ADDR)
		return NULL;

	srl = (struct srel *)iter->it_rl->rl_rls[0];
	if ((tuple = rl_get(srl, addr)) == NULL)
		return NULL;
	memcpy(iter->it_tpbuf, tuple, iter->it_rl->rl_size);
	return iter->it_tpbuf;
}

static void wrapper_ix_reset(struct xrel_iter *iter)
{
	assert(iter != NULL);
	assert(iter->it_rl != NULL);
	assert(iter->it_rl->rl_type == SREL_WRAPPER);
	assert(iter->it_iter[0] != NULL);

	iter->it_state = 0;
	ix_reset(iter->it_iter[0]);
}

static struct xrel_iter *wrapper_ix_iterator(struct xrel *rl,
		struct xattr *attr, int compar, const char *val)
{
	struct xrel_iter *iter;
	struct ix_iter *ix_iter;

	assert(rl != NULL);
	assert(rl->rl_type == SREL_WRAPPER);
	assert(attr != NULL);
	assert(attr->at_srl == rl->rl_rls[0]);
	assert(attr->at_ix != NULL);

	iter = xmalloc(sizeof(struct xrel_iter));
	iter->it_rl = rl;
	iter->it_state = 0;
	iter->it_compar = compar;
	iter->it_tpbuf = xmalloc(rl->rl_size);
	iter->it_fp = NULL;

	ix_iter = search_in_index(attr->at_srl, attr->at_sattr, compar, val);
	assert(ix_iter != NULL);

	iter->it_iter[0] = ix_iter;
	iter->it_free_iter[0] = (void (*)(void *))ix_iter_free;

	iter->it_iter[1] = NULL;
	iter->it_free_iter[1] = NULL;

	iter->it_next = wrapper_ix_next;
	iter->it_reset = wrapper_ix_reset;
	return iter;
}

struct xrel *wrapper_init(struct srel *srl)
{
	struct xrel *rl;
	unsigned short i;
	size_t offset;

	assert(srl != NULL);

	rl = xmalloc(sizeof(struct xrel));
	rl->rl_type = SREL_WRAPPER;
	rl->rl_rls[0] = srl;
	rl->rl_rls[1] = NULL;

	rl->rl_atcnt = srl->rl_header.hd_atcnt;
	rl->rl_attrs = xmalloc(rl->rl_atcnt * sizeof(struct xattr *));
	offset = 0;
	for (i = 0; i < rl->rl_atcnt; i++) {
		rl->rl_attrs[i] = xmalloc(sizeof(struct xattr));
		rl->rl_attrs[i]->at_pxrl = NULL;
		rl->rl_attrs[i]->at_pxattr = NULL;
		rl->rl_attrs[i]->at_srl = srl;
		rl->rl_attrs[i]->at_sattr = &srl->rl_header.hd_attrs[i];
		rl->rl_attrs[i]->at_offset = offset;
		rl->rl_attrs[i]->at_ix = open_index(srl,
				&srl->rl_header.hd_attrs[i]);
		offset += srl->rl_header.hd_attrs[i].at_size;
	}
	rl->rl_size = offset;

	rl->rl_excnt = 0;
	rl->rl_exprs = NULL;

	rl->rl_srtcnt = 0;
	rl->rl_srtattrs = NULL;
	rl->rl_srtorders = NULL;

	rl->rl_iterator = wrapper_iterator;
	rl->rl_ix_iterator = wrapper_ix_iterator;
	return rl;
}

static const char *join_next_indexed(struct xrel_iter *iter)
{
	struct xrel_iter *iter0, *iter1;
	const char *tuple0, *tuple1;

next_tuple:
	assert(iter != NULL);
	assert(iter->it_rl != NULL);
	assert(iter->it_rl->rl_type == JOIN);

	iter0 = iter->it_iter[0]; /* the indexed one */
	iter1 = iter->it_iter[1];

	assert(iter->it_state == 0 || iter0 != NULL);
	assert(iter->it_state == 0 || iter0->it_rl != NULL);
	assert(iter1 != NULL);
	assert(iter1->it_rl != NULL);

	if (iter->it_state == 0) {
		struct xrel *prl;
		struct xattr *pattr;
		const char *val;
		int compar;

		if ((tuple1 = iter1->it_next(iter1)) == NULL)
			return NULL;
		tpcpy(iter->it_tpbuf, iter->it_rl, tuple1, iter1->it_rl);

		prl = iter->it_ixattr->at_pxrl;
		pattr = iter->it_ixattr->at_pxattr;
		val = iter->it_tpbuf + iter->it_scanattr->at_offset;
		compar = iter->it_compar;

		assert(prl == other_xrel(iter->it_rl, iter1->it_rl));
		assert(pattr != NULL);

		iter0 = prl->rl_ix_iterator(prl, pattr, compar, val);
		iter->it_iter[0] = iter0;
		iter->it_state = 1;
	}

	assert(iter->it_rl->rl_size == iter0->it_rl->rl_size
			+ iter1->it_rl->rl_size);

	if ((tuple0 = iter0->it_next(iter0)) == NULL) {
		iter->it_free_iter[0](iter0);
		iter->it_iter[0] = NULL;
		iter->it_state = 0;
		goto next_tuple;
	} else {
		tpcpy(iter->it_tpbuf, iter->it_rl, tuple0, iter0->it_rl);
		if (!xexpr_check(iter->it_tpbuf,
					iter->it_rl->rl_exprs,
					iter->it_rl->rl_excnt)) {
			goto next_tuple;
		} else
			return iter->it_tpbuf;
	}
}

static const char *join_next_fullscan(struct xrel_iter *iter)
{
	const char *tuple0, *tuple1;
	struct xrel_iter *iter0, *iter1;

next_tuple:
	assert(iter != NULL);
	assert(iter->it_rl != NULL);
	assert(iter->it_rl->rl_type == JOIN);

	iter0 = iter->it_iter[0];
	iter1 = iter->it_iter[1];

	assert(iter0 != NULL);
	assert(iter0->it_rl != NULL);
	assert(iter1 != NULL);
	assert(iter1->it_rl != NULL);

	if (iter->it_state == 0) {
		if ((tuple0 = iter0->it_next(iter0)) == NULL)
			return NULL;
		tpcpy(iter->it_tpbuf, iter->it_rl, tuple0, iter0->it_rl);
		iter->it_state = 1;
	}

	if ((tuple1 = iter1->it_next(iter1)) == NULL) {
		if ((tuple0 = iter0->it_next(iter0)) == NULL)
			return NULL;
		tpcpy(iter->it_tpbuf, iter->it_rl, tuple0, iter0->it_rl);
		iter1->it_reset(iter1);
		if ((tuple1 = iter1->it_next(iter1)) == NULL)
			return NULL;
	}
	tpcpy(iter->it_tpbuf, iter->it_rl, tuple1, iter1->it_rl);
	if (!xexpr_check(iter->it_tpbuf, iter->it_rl->rl_exprs,
				iter->it_rl->rl_excnt))
		goto next_tuple;
	else
		return iter->it_tpbuf;
}

static void join_reset_indexed(struct xrel_iter *iter)
{
	struct xrel_iter *xrel_iter;

	assert(iter != NULL);
	assert(iter->it_rl != NULL);
	assert(iter->it_rl->rl_type == JOIN);

	iter->it_state = 0;
	xrel_iter = (struct xrel_iter *)iter->it_iter[0];
	if (xrel_iter != NULL) {
		xrel_iter->it_free_iter[0](xrel_iter);
		iter->it_iter[0] = NULL;
	}
	xrel_iter = (struct xrel_iter *)iter->it_iter[1];
	xrel_iter->it_reset(xrel_iter);
}

static void join_reset_fullscan(struct xrel_iter *iter)
{
	struct xrel_iter *xrel_iter;

	assert(iter != NULL);
	assert(iter->it_rl != NULL);
	assert(iter->it_rl->rl_type == JOIN);

	iter->it_state = 0;
	xrel_iter = (struct xrel_iter *)iter->it_iter[0];
	xrel_iter->it_reset(xrel_iter);
	xrel_iter = (struct xrel_iter *)iter->it_iter[1];
	xrel_iter->it_reset(xrel_iter);
}

static struct xrel_iter *join_iterator(struct xrel *rl)
{
	struct xrel_iter *iter;
	struct xattr *ix_attr, *other_attr;
	int compar;

	assert(rl != NULL);
	assert(rl->rl_type == JOIN);

	iter = xmalloc(sizeof(struct xrel_iter));
	iter->it_rl = rl;
	iter->it_state = 0;
	iter->it_tpbuf = xmalloc(rl->rl_size);
	iter->it_fp = NULL;

	if (best_aa_xexpr(rl, NULL, &ix_attr, &compar, &other_attr)) {
		struct xrel *prl;

		assert(ix_attr->at_ix != NULL);

		iter->it_compar = compar;
		iter->it_scanattr = other_attr;
		iter->it_ixattr = ix_attr;

		iter->it_iter[0] = NULL; /* later from rl_ix_iterator() */
		iter->it_free_iter[0] = (void (*)(void *))xrel_iter_free;

		prl = other_attr->at_pxrl;
		iter->it_iter[1] = prl->rl_iterator(prl);
		iter->it_free_iter[1] = (void (*)(void *))xrel_iter_free;

		iter->it_next = join_next_indexed;
		iter->it_reset = join_reset_indexed;
	} else {
		struct xrel *prl;

		prl = (struct xrel *)rl->rl_rls[0];
		iter->it_iter[0] = prl->rl_iterator(prl);
		iter->it_free_iter[0] = (void (*)(void *))xrel_iter_free;

		prl = (struct xrel *)rl->rl_rls[1];
		iter->it_iter[1] = prl->rl_iterator(prl);
		iter->it_free_iter[1] = (void (*)(void *))xrel_iter_free;

		iter->it_next = join_next_fullscan;
		iter->it_reset = join_reset_fullscan;
	}
	return iter;
}

static struct xrel_iter *join_ix_iterator(struct xrel *rl,
		struct xattr *attr, int compar, const char *val)
{
	struct xrel_iter *iter;
	struct xrel *prl, *other_prl;
	struct xattr *pattr, *ix_attr, *scan_attr;
	int joincompar;

	assert(rl != NULL);
	assert(rl->rl_type == JOIN);
	assert(attr != NULL);
	assert(attr->at_pxrl == rl->rl_rls[0]
			|| attr->at_pxrl == rl->rl_rls[1]);
	assert(attr->at_ix != NULL);

	iter = xmalloc(sizeof(struct xrel_iter));
	iter->it_rl = rl;
	iter->it_state = 0;
	iter->it_tpbuf = xmalloc(rl->rl_size);
	iter->it_fp = NULL;

	prl = attr->at_pxrl;
	other_prl = other_xrel(rl, prl);
	pattr = attr->at_pxattr;
	if (best_aa_xexpr(rl, other_prl, &ix_attr, &joincompar, &scan_attr)) {
		assert(attr->at_pxrl == scan_attr->at_pxrl);

		iter->it_compar = joincompar;
		iter->it_scanattr = scan_attr;
		iter->it_ixattr = ix_attr;

		iter->it_iter[0] = NULL; /* later from rl_ix_iterator() */
		iter->it_free_iter[0] = (void (*)(void *))xrel_iter_free;

		iter->it_iter[1] = prl->rl_ix_iterator(prl, pattr, compar, val);
		iter->it_free_iter[1] = (void (*)(void *))xrel_iter_free;

		iter->it_next = join_next_indexed;
		iter->it_reset = join_reset_indexed;
	} else {
		iter->it_iter[0] = other_prl->rl_iterator(other_prl);
		iter->it_free_iter[0] = (void (*)(void *))xrel_iter_free;

		iter->it_iter[1] = prl->rl_ix_iterator(prl, pattr, compar, val);
		iter->it_free_iter[1] = (void (*)(void *))xrel_iter_free;

		iter->it_next = join_next_fullscan;
		iter->it_reset = join_reset_fullscan;
	}
	return iter;
}

struct xrel *join_init(struct xrel *r, struct xrel *s,
		struct xexpr **exprs, unsigned short excnt)
{
	struct xrel *rl;
	unsigned short i;
	size_t offset;

	assert(r != NULL);
	assert(s != NULL);
	assert(r != s);
	assert(excnt == 0 || exprs != NULL);

	rl = xmalloc(sizeof(struct xrel));
	rl->rl_type = JOIN;
	rl->rl_rls[0] = r;
	rl->rl_rls[1] = s;
	rl->rl_size = r->rl_size + s->rl_size;

	rl->rl_excnt = excnt;
	rl->rl_exprs = (rl->rl_excnt > 0)
		? xmalloc(rl->rl_excnt * sizeof(struct xexpr *))
		: NULL;
	for (i = 0; i < rl->rl_excnt; i++) {
		rl->rl_exprs[i] = xmalloc(sizeof(struct xexpr));
		memcpy(rl->rl_exprs[i], exprs[i], sizeof(struct xexpr));
		assert(rl->rl_exprs[i]->ex_type == ATTR_TO_ATTR);
	}

	rl->rl_atcnt = r->rl_atcnt + s->rl_atcnt;
	rl->rl_attrs = xmalloc(rl->rl_atcnt * sizeof(struct xattr *));
	for (i = 0, offset = 0; i < rl->rl_atcnt; i++) {
		struct xattr *attr;
		unsigned short j;

		rl->rl_attrs[i] = xmalloc(sizeof(struct xattr));
		if (i < r->rl_atcnt)
			attr = r->rl_attrs[i];
		else
			attr = s->rl_attrs[i - r->rl_atcnt];
		memcpy(rl->rl_attrs[i], attr, sizeof(struct xattr));
		rl->rl_attrs[i]->at_pxrl = (i < r->rl_atcnt) ? r : s;
		rl->rl_attrs[i]->at_pxattr = attr;
		rl->rl_attrs[i]->at_offset = offset;
		offset += attr->at_sattr->at_size;

		for (j = 0; j < excnt; j++) {
			struct xexpr *expr;

			expr = rl->rl_exprs[j];
			if (expr->ex_left_attr == attr)
				expr->ex_left_attr = rl->rl_attrs[i];
			if (expr->ex_right_attr == attr)
				expr->ex_right_attr = rl->rl_attrs[i];
		}
	}

	rl->rl_srtcnt = 0;
	rl->rl_srtattrs = NULL;
	rl->rl_srtorders = NULL;

	rl->rl_iterator = join_iterator;
	rl->rl_ix_iterator = join_ix_iterator;
	return rl;
}

static const char *selection_next(struct xrel_iter *iter)
{
	struct xrel_iter *iter0;
	struct xrel *rl;
	const char *tuple;

next_tuple:
	assert(iter != NULL);
	assert(iter->it_rl != NULL);
	assert(iter->it_rl->rl_type == SELECTION);
	assert(iter->it_iter[0] != NULL);

	rl = iter->it_rl;
	iter0 = iter->it_iter[0];

	if ((tuple = iter0->it_next(iter0)) == NULL)
		return NULL;
	else if (!xexpr_check(tuple, rl->rl_exprs, rl->rl_excnt))
		goto next_tuple;
	else
		return tuple;
}

static void selection_reset(struct xrel_iter *iter)
{
	struct xrel_iter *xrel_iter;

	assert(iter != NULL);
	assert(iter->it_rl != NULL);
	assert(iter->it_rl->rl_type == SELECTION);

	iter->it_state = 0;
	xrel_iter = (struct xrel_iter *)iter->it_iter[0];
	xrel_iter->it_reset(xrel_iter);
}

static struct xrel_iter *selection_iterator(struct xrel *rl)
{
	struct xrel_iter *iter;
	struct xattr *ix_attr;
	int compar;
	char *val;

	assert(rl != NULL);
	assert(rl->rl_type == SELECTION);

	iter = xmalloc(sizeof(struct xrel_iter));
	iter->it_rl = rl;
	iter->it_state = 0;
	iter->it_tpbuf = NULL;
	iter->it_fp = NULL;

	if (best_av_xexpr(rl, &ix_attr, &compar, &val)) {
		struct xrel *prl;
		struct xattr *pattr;

		prl = (struct xrel *)rl->rl_rls[0];
		pattr = ix_attr->at_pxattr;
		assert(pattr != NULL);
		iter->it_iter[0] = prl->rl_ix_iterator(prl, pattr, compar, val);
	} else {
		struct xrel *prl;

		prl = (struct xrel *)rl->rl_rls[0];
		iter->it_iter[0] = prl->rl_iterator(prl);
	}
	iter->it_free_iter[0] = (void (*)(void *))xrel_iter_free;

	iter->it_iter[1] = NULL;
	iter->it_free_iter[1] = NULL;

	iter->it_next = selection_next;
	iter->it_reset = selection_reset;
	return iter;
}

static struct xrel_iter *selection_ix_iterator(struct xrel *rl,
		struct xattr *attr, int compar, const char *val)
{
	struct xrel_iter *iter;
	struct xrel *prl;
	struct xattr *pattr;

	assert(rl != NULL);
	assert(rl->rl_type == SELECTION);
	assert(attr != NULL);
	assert(attr->at_pxrl == rl->rl_rls[0]);
	assert(attr->at_ix != NULL);

	iter = xmalloc(sizeof(struct xrel_iter));
	iter->it_rl = rl;
	iter->it_state = 0;
	iter->it_tpbuf = NULL;
	iter->it_fp = NULL;

	prl = attr->at_pxrl;
	pattr = attr->at_pxattr;
	assert(pattr != NULL);
	iter->it_iter[0] = prl->rl_ix_iterator(prl, pattr, compar, val);
	iter->it_free_iter[0] = (void (*)(void *))xrel_iter_free;

	iter->it_iter[1] = NULL;
	iter->it_free_iter[1] = NULL;

	iter->it_next = selection_next;
	iter->it_reset = selection_reset;
	return iter;
}

struct xrel *selection_init(struct xrel *r, struct xexpr **exprs,
		unsigned short excnt)
{
	struct xrel *rl;
	unsigned short i;

	assert(r != NULL);
	assert(excnt == 0 || exprs != NULL);

	rl = xmalloc(sizeof(struct xrel));
	rl->rl_type = SELECTION;
	rl->rl_rls[0] = r;
	rl->rl_rls[1] = NULL;
	rl->rl_size = r->rl_size;

	rl->rl_excnt = excnt;
	rl->rl_exprs = (rl->rl_excnt > 0)
		? xmalloc(rl->rl_excnt * sizeof(struct xexpr *))
		: NULL;
	for (i = 0; i < rl->rl_excnt; i++) {
		rl->rl_exprs[i] = xmalloc(sizeof(struct xexpr));
		memcpy(rl->rl_exprs[i], exprs[i], sizeof(struct xexpr));
		assert(rl->rl_exprs[i]->ex_type == ATTR_TO_VAL);
	}

	rl->rl_atcnt = r->rl_atcnt;
	rl->rl_attrs = xmalloc(rl->rl_atcnt * sizeof(struct xattr *));
	for (i = 0; i < rl->rl_atcnt; i++) {
		unsigned short j;
		struct xattr *attr;

		attr = r->rl_attrs[i];
		rl->rl_attrs[i] = xmalloc(sizeof(struct xattr));
		memcpy(rl->rl_attrs[i], attr, sizeof(struct xattr));
		rl->rl_attrs[i]->at_pxrl = r;
		rl->rl_attrs[i]->at_pxattr = attr;

		for (j = 0; j < excnt; j++) {
			struct xexpr *expr;

			expr = rl->rl_exprs[j];
			if (expr->ex_left_attr == attr)
				expr->ex_left_attr = rl->rl_attrs[i];
			if (expr->ex_right_attr == attr)
				expr->ex_right_attr = rl->rl_attrs[i];
		}
	}

	rl->rl_srtcnt = 0;
	rl->rl_srtattrs = NULL;
	rl->rl_srtorders = NULL;

	rl->rl_iterator = selection_iterator;
	rl->rl_ix_iterator = selection_ix_iterator;
	return rl;
}

static const char *projection_next(struct xrel_iter *iter)
{
	struct xrel_iter *iter0;
	struct xrel *rl, *prl;
	const char *tuple;
	unsigned short i, j;

	assert(iter != NULL);
	assert(iter->it_rl != NULL);
	assert(iter->it_rl->rl_type == PROJECTION);
	assert(iter->it_iter[0] != NULL);

	rl = iter->it_rl;
	iter0 = iter->it_iter[0];

	if ((tuple = iter0->it_next(iter0)) == NULL)
		return NULL;

	prl = (struct xrel *)rl->rl_rls[0];
	/* stupid procedure */
	for (i = 0; i < rl->rl_atcnt; i++) {
		for (j = 0; j < prl->rl_atcnt; j++)
			if (rl->rl_attrs[i]->at_sattr
					== prl->rl_attrs[j]->at_sattr)
				break;
		assert(j < prl->rl_atcnt);
		memcpy(iter->it_tpbuf + rl->rl_attrs[i]->at_offset,
			tuple + prl->rl_attrs[j]->at_offset,
			rl->rl_attrs[i]->at_sattr->at_size);
	}
	return iter->it_tpbuf;
}

static void projection_reset(struct xrel_iter *iter)
{
	struct xrel_iter *xrel_iter;

	assert(iter != NULL);
	assert(iter->it_rl != NULL);
	assert(iter->it_rl->rl_type == PROJECTION);

	iter->it_state = 0;
	xrel_iter = (struct xrel_iter *)iter->it_iter[0];
	xrel_iter->it_reset(xrel_iter);
}

static struct xrel_iter *projection_iterator(struct xrel *rl)
{
	struct xrel_iter *iter;
	struct xrel *r;

	assert(rl != NULL);
	assert(rl->rl_type == PROJECTION);

	iter = xmalloc(sizeof(struct xrel_iter));
	iter->it_rl = rl;
	iter->it_state = 0;
	iter->it_tpbuf = xmalloc(rl->rl_size);
	iter->it_fp = NULL;

	r = (struct xrel *)rl->rl_rls[0];

	iter->it_iter[0] = r->rl_iterator(r);
	iter->it_free_iter[0] = (void (*)(void *))xrel_iter_free;

	iter->it_iter[1] = NULL;
	iter->it_free_iter[1] = NULL;

	iter->it_next = projection_next;
	iter->it_reset = projection_reset;
	return iter;
}

static struct xrel_iter *projection_ix_iterator(struct xrel *rl,
		struct xattr *attr, int compar, const char *val)
{
	struct xrel_iter *iter;
	struct xrel *prl;
	struct xattr *pattr;

	assert(rl != NULL);
	assert(rl->rl_type == PROJECTION);
	assert(attr != NULL);
	assert(attr->at_pxrl == rl->rl_rls[0]);
	assert(attr->at_ix != NULL);

	iter = xmalloc(sizeof(struct xrel_iter));
	iter->it_rl = rl;
	iter->it_state = 0;
	iter->it_tpbuf = xmalloc(rl->rl_size);
	iter->it_fp = NULL;

	prl = attr->at_pxrl;
	pattr = attr->at_pxattr;
	assert(pattr != NULL);
	iter->it_iter[0] = prl->rl_ix_iterator(prl, pattr, compar, val);
	iter->it_free_iter[0] = (void (*)(void *))xrel_iter_free;

	iter->it_iter[1] = NULL;
	iter->it_free_iter[1] = NULL;

	iter->it_next = projection_next;
	iter->it_reset = projection_reset;
	return iter;
}

struct xrel *projection_init(struct xrel *r, struct xattr **attrs,
		unsigned short atcnt)
{
	struct xrel *rl;
	unsigned short i;
	size_t offset;

	assert(r != NULL);
	assert(atcnt == 0 || attrs != NULL);

	rl = xmalloc(sizeof(struct xrel));
	rl->rl_type = PROJECTION;
	rl->rl_rls[0] = r;
	rl->rl_rls[1] = NULL;
	rl->rl_size = r->rl_size;
	rl->rl_atcnt = atcnt;
	rl->rl_attrs = xmalloc(rl->rl_atcnt * sizeof(struct xattr *));
	offset = 0;
	for (i = 0; i < atcnt; i++) {
		struct xattr *attr;

		attr = attrs[i];
		assert(xrel_has_xattr(r, attr));
		rl->rl_attrs[i] = xmalloc(sizeof(struct xattr));
		memcpy(rl->rl_attrs[i], attr, sizeof(struct xattr));
		rl->rl_attrs[i]->at_pxrl = r;
		rl->rl_attrs[i]->at_pxattr = attr;
		rl->rl_attrs[i]->at_offset = offset;
		offset += rl->rl_attrs[i]->at_sattr->at_size;
	}
	rl->rl_size = offset;

	rl->rl_excnt = 0;
	rl->rl_exprs = NULL;

	rl->rl_srtcnt = 0;
	rl->rl_srtattrs = NULL;
	rl->rl_srtorders = NULL;

	rl->rl_iterator = projection_iterator;
	rl->rl_ix_iterator = projection_ix_iterator;
	return rl;
}

static const char *union_next(struct xrel_iter *iter)
{
	const char *tuple;
	struct xrel_iter *iter0, *iter1;

	assert(iter != NULL);
	assert(iter->it_rl != NULL);
	assert(iter->it_rl->rl_type == UNION);

	iter0 = iter->it_iter[0];
	iter1 = iter->it_iter[1];

	assert(iter0 != NULL);
	assert(iter0->it_rl != NULL);
	assert(iter1 != NULL);
	assert(iter1->it_rl != NULL);

	if (iter->it_state == 0) {
		if ((tuple = iter0->it_next(iter0)) != NULL)
			return tuple;
		else
			iter->it_state = 1;
	}

	return iter1->it_next(iter1);
}

static void union_reset(struct xrel_iter *iter)
{
	struct xrel_iter *xrel_iter;

	assert(iter != NULL);
	assert(iter->it_rl != NULL);
	assert(iter->it_rl->rl_type == UNION);

	iter->it_state = 0;
	xrel_iter = (struct xrel_iter *)iter->it_iter[0];
	xrel_iter->it_reset(xrel_iter);
	xrel_iter = (struct xrel_iter *)iter->it_iter[1];
	xrel_iter->it_reset(xrel_iter);
}

static struct xrel_iter *union_iterator(struct xrel *rl)
{
	struct xrel_iter *iter;
	struct xrel *r;

	assert(rl != NULL);
	assert(rl->rl_type == UNION);

	iter = xmalloc(sizeof(struct xrel_iter));
	iter->it_rl = rl;
	iter->it_state = 0;
	iter->it_tpbuf = NULL;
	iter->it_fp = NULL;

	r = (struct xrel *)rl->rl_rls[0];
	iter->it_iter[0] = r->rl_iterator(r);
	iter->it_free_iter[0] = (void (*)(void *))xrel_iter_free;

	r = (struct xrel *)rl->rl_rls[1];
	iter->it_iter[1] = r->rl_iterator(r);
	iter->it_free_iter[1] = (void (*)(void *))xrel_iter_free;

	iter->it_next = union_next;
	iter->it_reset = union_reset;
	return iter;
}

static struct xrel_iter *union_ix_iterator(struct xrel *rl,
		struct xattr *attr, int compar, const char *val)
{
	struct xrel_iter *iter;
	struct xrel *prl;
	struct xattr *pattr;
	unsigned short i;

	assert(rl != NULL);
	assert(rl->rl_type == UNION);
	assert(attr != NULL);
	assert(attr->at_pxrl == rl->rl_rls[0]
			|| attr->at_pxrl == rl->rl_rls[1]);
	assert(attr->at_ix != NULL);

	iter = xmalloc(sizeof(struct xrel_iter));
	iter->it_rl = rl;
	iter->it_state = 0;
	iter->it_tpbuf = NULL;
	iter->it_fp = NULL;

	for (i = 0; i < rl->rl_atcnt; i++)
		if (attr->at_sattr == rl->rl_attrs[i]->at_sattr)
			break;
	assert(i < rl->rl_atcnt);

	prl = attr->at_pxrl;
	pattr = attr->at_pxattr;
	assert(pattr != NULL);
	iter->it_iter[0] = prl->rl_ix_iterator(prl, pattr, compar, val);
	iter->it_free_iter[0] = (void (*)(void *))xrel_iter_free;

	prl = other_xrel(rl, prl);
	pattr = attr->at_pxattr;
	assert(pattr != NULL);
	iter->it_iter[1] = prl->rl_ix_iterator(prl, pattr, compar, val);
	iter->it_free_iter[1] = (void (*)(void *))xrel_iter_free;

	iter->it_next = union_next;
	iter->it_reset = union_reset;
	return iter;
}

struct xrel *union_init(struct xrel *r, struct xrel *s)
{
	struct xrel *rl;
	unsigned short i;
	size_t offset;

	assert(r != NULL);
	assert(s != NULL);
	assert(r != s);
	assert(compliant_xrels(r, s));

	rl = xmalloc(sizeof(struct xrel));
	rl->rl_type = UNION;
	rl->rl_rls[0] = r;
	rl->rl_rls[1] = s;
	rl->rl_size = r->rl_size;
	rl->rl_atcnt = r->rl_atcnt;
	rl->rl_attrs = xmalloc(rl->rl_atcnt * sizeof(struct xattr *));
	for (i = 0, offset = 0; i < rl->rl_atcnt; i++) {
		struct xattr *attr;

		attr = r->rl_attrs[i];
		rl->rl_attrs[i] = xmalloc(sizeof(struct xattr));
		memcpy(rl->rl_attrs[i], attr, sizeof(struct xattr));
		rl->rl_attrs[i]->at_pxrl = r;
		rl->rl_attrs[i]->at_pxattr = attr;
		rl->rl_attrs[i]->at_offset = offset;
		offset += rl->rl_attrs[i]->at_sattr->at_size;
	}

	rl->rl_excnt = 0;
	rl->rl_exprs = NULL;

	rl->rl_srtcnt = 0;
	rl->rl_srtattrs = NULL;
	rl->rl_srtorders = NULL;

	rl->rl_iterator = union_iterator;
	rl->rl_ix_iterator = union_ix_iterator;
	return rl;
}

static const char *sort_next(struct xrel_iter *iter)
{
	struct xrel *rl;
	FILE *fp;
	long pos;
	char *buf;

	assert(iter != NULL);
	assert(iter->it_rl != NULL);
	assert(iter->it_rl->rl_type == SORT);

	rl = iter->it_rl;
	fp = iter->it_fp;
	buf = iter->it_tpbuf;
	pos = (long)iter->it_state * (long)rl->rl_size;
	fseek(fp, pos, SEEK_SET);
	if (fread(buf, sizeof(char), rl->rl_size, fp) == rl->rl_size) {
		iter->it_state++;
		return buf;
	} else
		return NULL;
}

static void sort_reset(struct xrel_iter *iter)
{
	assert(iter != NULL);
	assert(iter->it_rl != NULL);
	assert(iter->it_rl->rl_type == SORT);

	iter->it_state = 0;
}

static struct xrel_iter *sort_iterator(struct xrel *rl)
{
	struct xrel *prl;
	struct xrel_iter *iter, *child_iter;
	FILE *fp;

	assert(rl != NULL);
	assert(rl->rl_type == SORT);

	prl = rl->rl_rls[0];
	child_iter = prl->rl_iterator(prl);
	fp = xrel_sort(prl, child_iter, rl->rl_srtattrs, rl->rl_srtorders,
			rl->rl_srtcnt);
	xrel_iter_free(child_iter);
	assert(fp != NULL);

	iter = xmalloc(sizeof(struct xrel_iter));
	iter->it_rl = rl;
	iter->it_state = 0;
	iter->it_tpbuf = xmalloc(rl->rl_size);
	iter->it_fp = fp;

	iter->it_iter[0] = NULL;
	iter->it_free_iter[0] = NULL;

	iter->it_iter[1] = NULL;
	iter->it_free_iter[1] = NULL;

	iter->it_next = sort_next;
	iter->it_reset = sort_reset;
	return iter;
}

static struct xrel_iter *sort_ix_iterator(struct xrel *rl,
		struct xattr *attr, int compar, const char *val)
{
	struct xrel *prl;
	struct xattr *pattr;
	struct xrel_iter *iter, *child_iter;
	FILE *fp;

	assert(rl != NULL);
	assert(rl->rl_type == SORT);
	assert(attr != NULL);
	assert(attr->at_pxrl == rl->rl_rls[0]);
	assert(attr->at_ix != NULL);

	prl = rl->rl_rls[0];
	pattr = attr->at_pxattr;
	assert(pattr != NULL);
	child_iter = prl->rl_ix_iterator(prl, pattr, compar, val);
	fp = xrel_sort(prl, child_iter, rl->rl_srtattrs, rl->rl_srtorders,
			rl->rl_srtcnt);
	xrel_iter_free(child_iter);
	assert(fp != NULL);

	iter = xmalloc(sizeof(struct xrel_iter));
	iter->it_rl = rl;
	iter->it_state = 0;
	iter->it_tpbuf = xmalloc(rl->rl_size);
	iter->it_fp = fp;

	iter->it_iter[0] = NULL;
	iter->it_free_iter[0] = NULL;

	iter->it_iter[1] = NULL;
	iter->it_free_iter[1] = NULL;

	iter->it_next = sort_next;
	iter->it_reset = sort_reset;
	return iter;
}

struct xrel *sort_init(struct xrel *r, struct xattr **srtattrs, int *srtorders, 
		unsigned short srtcnt)
{
	struct xrel *rl;
	unsigned short i;

	assert(r != NULL);
	assert(srtattrs != NULL);
	assert(srtorders != NULL);
	assert(srtcnt > 0);

	rl = xmalloc(sizeof(struct xrel));
	rl->rl_type = SORT;
	rl->rl_rls[0] = r;
	rl->rl_rls[1] = NULL;
	rl->rl_size = r->rl_size;
	rl->rl_atcnt = r->rl_atcnt;
	rl->rl_attrs = xmalloc(rl->rl_atcnt * sizeof(struct xattr *));
	for (i = 0; i < rl->rl_atcnt; i++) {
		struct xattr *attr;

		attr = r->rl_attrs[i];
		rl->rl_attrs[i] = xmalloc(sizeof(struct xattr));
		memcpy(rl->rl_attrs[i], attr, sizeof(struct xattr));
		rl->rl_attrs[i]->at_pxrl = r;
		rl->rl_attrs[i]->at_pxattr = attr;
	}

	rl->rl_excnt = 0;
	rl->rl_exprs = NULL;

	rl->rl_srtcnt = srtcnt;
	rl->rl_srtattrs = xmalloc(rl->rl_srtcnt * sizeof(struct xattr *));
	rl->rl_srtorders = xmalloc(rl->rl_srtcnt * sizeof(int));
	for (i = 0; i < rl->rl_srtcnt; i++) {
		unsigned short j;

		for (j = 0; j < rl->rl_atcnt; j++)
			if (srtattrs[i]->at_sattr == rl->rl_attrs[j]->at_sattr)
				rl->rl_srtattrs[i] = rl->rl_attrs[j];
		rl->rl_srtorders[i] = srtorders[i];
	}

	rl->rl_iterator = sort_iterator;
	rl->rl_ix_iterator = sort_ix_iterator;
	return rl;
}

