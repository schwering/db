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

#include "expr.h"
#include "attr.h"
#include "dml.h"
#include "err.h"
#include "io.h"
#include "linkedlist.h"
#include "mem.h"
#include "rlmngt.h"
#include "str.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define MAX_EXPR_CNT	(sizeof(bitfield_t) * 8)

typedef unsigned long long bitfield_t;

struct implicant {
	bitfield_t vals;
	bitfield_t active;
	unsigned short weight;
};

/* the following functions are used to extract the leaves from a tree into
 * an array which contains no duplicate leaves. */

static int count_leaves(struct expr *expr)
{
	assert(expr != NULL);

	if (expr->type == INNER) {
		assert(expr->stype[0] == SON_EXPR
				&& expr->stype[1] == SON_EXPR);
		return count_leaves(expr->sons[0].expr)
			+ count_leaves(expr->sons[1].expr);
	} else
		return 1;
}

static int copy_leaves(struct expr **buf, int index, struct expr *expr)
{
	assert(buf != NULL);
	assert(expr != NULL);

	if (expr->type == INNER) {
		assert(expr->stype[0] == SON_EXPR
				&& expr->stype[1] == SON_EXPR);
		index = copy_leaves(buf, index, expr->sons[0].expr);
		index = copy_leaves(buf, index, expr->sons[1].expr);
		return index;
	} else {
		buf[index++] = expr;
		return index;
	}
}

static void replace_leaf(struct expr *oldptr, struct expr *newptr,
		struct expr *expr)
{
	assert(oldptr != NULL);
	assert(newptr != NULL);
	assert(expr != NULL);

	if (expr->type == INNER) {
		struct expr *son0, *son1;

		assert(expr->stype[0] == SON_EXPR
				&& expr->stype[1] == SON_EXPR);

		son0 = expr->sons[0].expr;
		son1 = expr->sons[1].expr;

		if (son0->type == INNER) {
			assert(son0->stype[0] == SON_EXPR
					&& son0->stype[1] == SON_EXPR);
			replace_leaf(oldptr, newptr, son0);
		} else if (son0 == oldptr) {
			expr_tree_free(oldptr);
			expr->sons[0].expr = newptr;
		}

		if (son1->type == INNER) {
			assert(son1->stype[0] == SON_EXPR
					&& son1->stype[1] == SON_EXPR);
			replace_leaf(oldptr, newptr, son1);
		} else if (son1 == oldptr) {
			expr_tree_free(oldptr);
			expr->sons[1].expr = newptr;
		}
	}
}

static bool value_equals(struct value *v, struct value *w)
{
	assert(v != NULL);
	assert(w != NULL);
	assert(v->domain == STRING
			|| v->domain == INT
			|| v->domain == FLOAT);

	/* TODO What about BYTEs?! */
	if (v->domain != w->domain)
		return false;

	switch (v->domain) {
		case INT:
			return v->ptr.vint == w->ptr.vint;
		case UINT:
			return v->ptr.vuint == w->ptr.vuint;
		case LONG:
			return v->ptr.vlong == w->ptr.vlong;
		case ULONG:
			return v->ptr.vulong == w->ptr.vulong;
		case FLOAT:
			return v->ptr.vfloat == w->ptr.vfloat;
		case DOUBLE:
			return v->ptr.vdouble == w->ptr.vdouble;
		case STRING:
			return !strcmp(v->ptr.pstring, w->ptr.pstring);
		case BYTES:
			assert(false);
			return false;
		default:
			return false;
	}
}

bool leaf_equals(struct expr *k, struct expr *l)
{
	assert(k != NULL);
	assert(k->type == LEAF);
	assert(k->stype[0] == SON_SATTR);
	assert(k->stype[1] == SON_VALUE || k->stype[1] == SON_SATTR);
	assert(l != NULL);
	assert(l->type == LEAF);
	assert(l->stype[0] == SON_SATTR);
	assert(l->stype[1] == SON_VALUE || l->stype[1] == SON_SATTR);
	
	if (k->stype[1] == SON_VALUE && k->stype[1] == SON_VALUE)
		return l->op == k->op
			&& !memcmp(l->sons[0].sattr, k->sons[0].sattr,
					sizeof(struct sattr))
			&& value_equals(l->sons[1].value, k->sons[1].value);
	else if (k->stype[1] == SON_SATTR && k->stype[1] == SON_SATTR)
		return l->op == k->op
			&& !memcmp(l->sons[0].sattr, k->sons[0].sattr,
					sizeof(struct sattr))
			&& !memcmp(l->sons[1].sattr, k->sons[1].sattr,
					sizeof(struct sattr));
	else
		return false;
}

static int find_elem(void **arr, int len, void *e)
{
	int i;

	assert(arr != NULL);
	assert(e != NULL);

	for (i = 0; i < len; i++)
		if (e == arr[i])
			return i;
	return -1;
}

static void remove_elem(void **arr, int *len, int i)
{
	int j;

	assert(arr != NULL);
	assert(*len >= 0);
	assert(0 <= i && i < *len);

	for (j = i+1; j < *len; j++)
		arr[j-1] = arr[j];
	(*len)--;
}

static int filter_dupes(struct expr *root, struct expr **leaves, int leaf_cnt)
{
	int i, j;

	assert(root != NULL);
	assert(leaves != NULL);

	for (i = 0; i < leaf_cnt; i++) {
		for (j = leaf_cnt - 1; j > i; j--) {
			if (leaf_equals(leaves[i], leaves[j])) {
				replace_leaf(leaves[j], leaves[i], root);
				remove_elem((void **)leaves, &leaf_cnt, j);
			}
		}
	}
	return leaf_cnt;
}

static bool check_model(struct expr *expr, struct expr **leaves, int leaf_cnt,
		struct implicant *impl)
{
	assert(expr != NULL);
	assert(leaves != NULL);

	if (expr->type == INNER) {
		assert(expr->stype[0] == SON_EXPR
				&& expr->stype[1] == SON_EXPR);

		switch (expr->op) {
		case AND:
			return check_model(expr->sons[0].expr, leaves,
					leaf_cnt, impl)
				&& check_model(expr->sons[1].expr, leaves,
						leaf_cnt, impl);
		case NAND:
			return !(check_model(expr->sons[0].expr, leaves,
						leaf_cnt, impl)
				&& check_model(expr->sons[1].expr, leaves,
					leaf_cnt, impl));
		case OR:
			return check_model(expr->sons[0].expr, leaves,
					leaf_cnt, impl)
				|| check_model(expr->sons[1].expr, leaves,
						leaf_cnt, impl);
		case NOR:
			return !(check_model(expr->sons[0].expr, leaves,
						leaf_cnt, impl)
				|| check_model(expr->sons[1].expr, leaves,
					leaf_cnt, impl));
		default:
			assert(false);
			return false;
		}
	} else {
		int index = find_elem((void **)leaves, leaf_cnt, expr);
		assert(index != -1);
		return (impl->vals >> index) & 1;
	}
}

#if 0
static void print_impl(struct implicant *impl)
{
	bitfield_t v, a;
	int i;

	assert(impl != NULL);

	v = impl->vals;
	a = impl->active;
	for (i = 0; i < 10; i++) {
		if ((a&1) == 1 && (v&1) == 1)
			putc('1', stdout);
		else if ((a&1) == 1 && (v&1) == 0)
			putc('0', stdout);
		else if ((a&1) == 0)
			putc('-', stdout);
		a >>= 1;
		v >>= 1;
	}

	putc('\n', stdout);
}
#endif

static unsigned short calc_weight(struct implicant *impl)
{
	bitfield_t v, a;
	int r;

	assert(impl != NULL);

	v = impl->vals;
	a = impl->active;
	r = 0;
	while (a != 0) {
		r += v & a & 1;
		v >>= 1;
		a >>= 1;
	}
	return r;
}

static struct llist *calc_min_impls(struct expr *root,
		struct expr **leaves, int leaf_cnt)
{
	struct implicant impl;
	struct llist *ll;

	assert(root != NULL);
	assert(leaves != NULL);

	ll = ll_init(sizeof(struct implicant));

	impl.vals = (1 << leaf_cnt) - 1;
	impl.active = (1 << leaf_cnt) - 1;
	do {
		if (check_model(root, leaves, leaf_cnt, &impl)) {
			impl.weight = calc_weight(&impl);
			ll_add(ll, &impl);
		}
		impl.vals--;
	} while (impl.vals > 0);
	return ll;
}

static bool merge_impls(struct implicant *i, struct implicant *j, 
		struct implicant *dest)
{
	bitfield_t v, w;

	assert(i != NULL);
	assert(j != NULL);
	assert(dest != NULL);
	assert(i->weight + 1 == j->weight);

	v = i->vals & i->active;
	w = j->vals & j->active;
	
	if (!((v&w) == v && (v|w) == w))
		return false;
	
	dest->active = i->active & ~(v ^ w);
	dest->vals = v;
	dest->weight = i->weight;

	assert(dest->weight == calc_weight(dest));
	return true;
}

static bool merge_groups(struct llist *g, struct llist *h)
{
	struct llentry *e, *f;
	struct implicant impl;
	bool changed;

	assert(g != NULL);
	assert(h != NULL);

	changed = false;
	for (e = g->first; e != NULL; e = e->next) {
		for (f = h->first; f != NULL; f = f->next) {
			if (merge_impls(e->val, f->val, &impl)) {
				ll_add(g, &impl);
				ll_markdel(g, e);
				ll_markdel(h, f);
				changed = true;
			}
		}
	}

	return changed;
}

static struct llist *calc_prime_impls(struct llist *min_impls)
{
	unsigned short max_weight;
	struct llentry *e;
	struct implicant *impl;
	struct llist **groups, *ll;
	bool changed;
	int i;

	max_weight = 0;
	for (e = min_impls->first; e != NULL; e = e->next) {
		impl = e->val;
		if (impl->weight > max_weight)
			max_weight = impl->weight;
	}

	/* prepare groups (each group consists of implicants with the same
	 * weight) */
	groups = xmalloc((max_weight + 1) * sizeof(struct llist *));
	for (i = 0; i <= max_weight; i++)
		groups[i] = ll_init(sizeof(struct implicant));

	/* add implicants to their respective group */
	for (e = min_impls->first; e != NULL; e = e->next) {
		impl = e->val;
		ll_add(groups[impl->weight], impl);
	}

	/* merge neighbor groups as long as anything changes */
	do {
		changed = false;
		for (i = 0; i < max_weight; i++) {
			changed |= merge_groups(groups[i], groups[i+1]);
			ll_delmarked(groups[i]);
		}
		ll_delmarked(groups[i]);
	} while (changed);

	/* gather prime implicants in a single list */
	ll = ll_init(sizeof(struct implicant));
	for (i = 0; i <= max_weight; i++) {
		for (e = groups[i]->first; e != NULL; e = e->next) {
			impl = e->val;
			ll_add(ll, impl);
		}
		ll_free(groups[i]);
	}
	free(groups);
	return ll;
}

static bool implicates(struct implicant *min_impl, struct implicant *prime_impl)
{
	bitfield_t vm, am, vp, ap;

	assert(min_impl != NULL);
	assert(prime_impl != NULL);

	vm = min_impl->vals;
	am = min_impl->active;
	vp = prime_impl->vals;
	ap = prime_impl->active;

	return (am | ap) == am && (vm & ap) == vp;
}

static bool rowleq(bool **matrix, bool *cols, int rowcnt, int colcnt,
		int i1, int i2)
{
	bool b;
	int j;

	assert(i1 < rowcnt);
	assert(i2 < rowcnt);

	b = true;
	for (j = 0; j < colcnt; j++)
		if (cols[j])
			b &= matrix[i1][j] <= matrix[i2][j];
	return b;
}

static bool colleq(bool **matrix, bool *rows, int rowcnt, int colcnt,
		int j1, int j2)
{
	bool b;
	int i;

	assert(j1 < colcnt);
	assert(j2 < colcnt);

	b = true;
	for (i = 0; i < rowcnt; i++)
		if (rows[i])
			b &= matrix[i][j1] <= matrix[i][j2];
	return b;
}

static bool all_cols_covered(bool *cols, int colcnt)
{
	int j;

	for (j = 0; j < colcnt; j++)
		if (cols[j])
			return false;
	return true;
}

static int covered_cols_by_row(bool **matrix, bool *cols, int rowcnt,
		int colcnt, int i)
{
	int j, cnt;

	assert(i < rowcnt);

	cnt = 0;
	for (j = 0; j < colcnt; j++)
		if (cols[j] && matrix[i][j])
			cnt++;
	return cnt;
}

static void minimize_prime_impls(struct llist *min_impls, 
		struct llist *prime_impls)
{
	bool **matrix, *rows, *cols, changed, *selected;
	struct llentry *e, *f;
	size_t i, j;

	assert(min_impls != NULL);
	assert(prime_impls != NULL);


	/* allocate memory for matrix etc. */
	matrix = xmalloc(prime_impls->cnt * sizeof(bool *));
	for (i = 0; i < prime_impls->cnt; i++)
		matrix[i] = xmalloc(min_impls->cnt * sizeof(bool));
	rows = xmalloc(prime_impls->cnt * sizeof(bool));
	cols = xmalloc(min_impls->cnt * sizeof(bool));


	/* initialize matrix etc. */
	for (e = prime_impls->first, i = 0; e != NULL; e = e->next, i++) {
		assert(i < prime_impls->cnt);
		rows[i] = true;
		for (f = min_impls->first, j = 0; f != NULL; f = f->next, j++) {
			assert(j < min_impls->cnt);
			cols[j] = true;
			matrix[i][j] = implicates(f->val, e->val);
		}
	}


	/* remove rows or columns as long as a pair of rows R_1, R_2
	 * with R_1 <= R_2 exists (then remove R_1) or a pair of columns
	 * C_1, C_2 with C_1 <= C_2 exists (then remove C_2) */
	do {
		changed = false;
		for (i = 0; i < prime_impls->cnt; i++) {
			if (!rows[i])
				continue;
			for (j = 0; j < prime_impls->cnt; j++) {
				if (i == j || !rows[j])
					continue;
				if (rowleq(matrix, cols, prime_impls->cnt, 
							min_impls->cnt,
							i, j)) {
					rows[i] = false;
					changed = true;
				}
			}
		}
		for (i = 0; i < min_impls->cnt; i++) {
			if (!cols[i])
				continue;
			for (j = 0; j < min_impls->cnt; j++) {
				if (i == j || !cols[j])
					continue;
				if (colleq(matrix, rows, prime_impls->cnt, 
							min_impls->cnt,
							i, j)) {
					cols[j] = false;
					changed = true;
				}
			}
		}
	} while (changed);


	/* greedy algorithm to approximate an optimal selection of 
	 * rows (i.e. prime implicants): 
	 * select (i.e. mark as selected) the row that covers the most 
	 * uncovered columns */
	selected = xmalloc(prime_impls->cnt * sizeof(bool));
	for (i = 0; i < prime_impls->cnt; i++)
		selected[i] = false;
	do {
		int best, covered, tmp;

		best = -1;
		covered = -1;
		for (i = 0; i < prime_impls->cnt; i++) {
			if (!rows[i] || selected[i])
				continue;
			tmp = covered_cols_by_row(matrix, cols,
					prime_impls->cnt, min_impls->cnt, i);
			if (tmp > covered) {
				best = i;
				covered = tmp;
			}
		}
		assert(best >= 0);
		assert(covered >= 0);

		selected[best] = true; /* mark `best' as selected */
		for (j = 0; j < min_impls->cnt; j++)
			if (matrix[best][j])
				cols[j] = false;
	} while (!all_cols_covered(cols, min_impls->cnt));


	/* remove the prime implicants that are marked so */
	for (e = prime_impls->first, i = 0; e != NULL; e = f, i++) {
		f = e->next;
		if (!rows[i] || !selected[i])
			ll_del(prime_impls, e);
		free(matrix[i]);
	}
	free(matrix);
	free(rows);
	free(cols);
	free(selected);
}

static struct expr **create_conjunction(struct expr **leaves, 
		struct implicant *impl)
{
	struct expr **conj;
	int i, j, len, cnt;
	bitfield_t v, a;

	assert(leaves != NULL);

	len = 0;
	cnt = 0;
	a = impl->active;
	while (a != 0) {
		len++;
		if ((a & 1) != 0)
			cnt++;
		a >>= 1;
	}

	conj = xmalloc((cnt+1) * sizeof(struct expr *));

	v = impl->vals;
	a = impl->active;
	for (i = 0, j = 0; i < len; i++, a >>= 1, v >>= 1) {
		struct expr *e;
		bool val;

		if ((a & 1) == 0)
			continue;
		val = v & 1;

		e = xmalloc(sizeof(struct expr));
		memcpy(e, leaves[i], sizeof(struct expr));
		if (val == false) {
			switch (e->op) {
				case EQ:	e->op = NEQ;	break;
				case NEQ:	e->op = EQ;	break;
				case GEQ:	e->op = LT;	break;
				case LT:	e->op = GEQ;	break;
				case LEQ:	e->op = GT;	break;
				case GT:	e->op = LEQ;	break;
				case NAND:
				case NOR:
				case AND:
				case OR:
				default:	assert(false);
			}
		}
		conj[j++] = e;
	}
	conj[j] = NULL;
	return conj;
}

struct expr ***formula_to_dnf(struct expr *root)
{
	struct expr **leaves, ***dnf;
	int leaf_cnt;
	struct llist *min_impls, *prime_impls;
	struct llentry *e;
	int i;

	assert(root != NULL);

	/* create array of leaves */
	leaf_cnt = count_leaves(root);
	leaves = xmalloc(leaf_cnt * sizeof(struct expr *));
	copy_leaves(leaves, 0, root);
	leaf_cnt = filter_dupes(root, leaves, leaf_cnt);

	min_impls = calc_min_impls(root, leaves, leaf_cnt);
#if 0
	printf("MIN IMPLS:\n");
	for (e = min_impls->first, i = 0; e != NULL; e = e->next, i++)
		printf("\t %d ", i),print_impl(e->val);
#endif

	prime_impls = calc_prime_impls(min_impls);
#if 0
	printf("PRIME IMPLS:\n");
	for (e = prime_impls->first, i = 0; e != NULL; e = e->next, i++)
		printf("\t %d ", i),print_impl(e->val);
#endif

	minimize_prime_impls(min_impls, prime_impls);
#if 0
	printf("MINIMIZED PRIME IMPLS:\n");
	for (e = prime_impls->first, i = 0; e != NULL; e = e->next, i++)
		printf("\t %d ", i),print_impl(e->val);
#endif

	dnf = xmalloc((prime_impls->cnt + 1) * sizeof(struct expr **));
	for (e = prime_impls->first, i = 0; e != NULL; e = e->next, i++) {
		dnf[i] = create_conjunction(leaves, e->val);
	}
	dnf[i] = NULL;

	free(leaves);
	ll_free(min_impls);
	ll_free(prime_impls);
	return dnf;
}

bool expr_init(struct expr *expr)
{
	if (expr == NULL)
		return true;

	if (expr->type == INNER) {
		assert(expr->stype[0] == SON_EXPR
				&& expr->stype[1] == SON_EXPR);

		return expr_init(expr->sons[0].expr)
			&& expr_init(expr->sons[1].expr);
	} else if (expr->type == LEAF) {
		bool retval;
		int i;

		retval = true;
		for (i = 0; i < 2; i++) {
			struct attr *attr;
			struct srel *srl;
			struct sattr *sattr;
			char *attr_name;

			if (expr->stype[i] != SON_ATTR)
				continue;

			attr = expr->sons[i].attr;
			attr_name = attr->attr_name;
			srl = open_relation(attr->tbl_name);
			if (srl == NULL)
				retval &= false;
			sattr = sattr_by_srl_and_attr_name(srl, attr_name);

			if (sattr != NULL) {
				expr->stype[i] = SON_SATTR;
				expr->sons[i].sattr = sattr;
				retval &= true;
			} else
				retval &= false;
		}
		return retval;
	}
	return false; /* avoid non-void-function compiler warning */
}

static bool sattr_check(const char *tuple, struct sattr *attr, int oper,
		struct value *val)
{
	const void *v1;

	assert(tuple != NULL);
	assert(attr != NULL);
	assert(val != NULL);
	assert(attr->at_domain == val->domain);

	v1 = tuple + attr->at_offset;

	if (attr->at_domain == INT) {
		db_int_t v2 = val->ptr.vint;
		switch (oper) {
			case EQ:	return *(db_int_t *)v1 == v2;
			case NEQ:	return *(db_int_t *)v1 != v2;
			case LEQ:	return *(db_int_t *)v1 <= v2;
			case GEQ:	return *(db_int_t *)v1 >= v2;
			case LT:	return *(db_int_t *)v1 < v2;
			case GT:	return *(db_int_t *)v1 > v2;
		}
	} else if (attr->at_domain == UINT) {
		db_uint_t v2 = val->ptr.vuint;
		switch (oper) {
			case EQ:	return *(db_uint_t *)v1 == v2;
			case NEQ:	return *(db_uint_t *)v1 != v2;
			case LEQ:	return *(db_uint_t *)v1 <= v2;
			case GEQ:	return *(db_uint_t *)v1 >= v2;
			case LT:	return *(db_uint_t *)v1 < v2;
			case GT:	return *(db_uint_t *)v1 > v2;
		}
	} else if (attr->at_domain == LONG) {
		db_long_t v2 = val->ptr.vlong;
		switch (oper) {
			case EQ:	return *(db_long_t *)v1 == v2;
			case NEQ:	return *(db_long_t *)v1 != v2;
			case LEQ:	return *(db_long_t *)v1 <= v2;
			case GEQ:	return *(db_long_t *)v1 >= v2;
			case LT:	return *(db_long_t *)v1 < v2;
			case GT:	return *(db_long_t *)v1 > v2;
		}
	} else if (attr->at_domain == ULONG) {
		db_ulong_t v2 = val->ptr.vulong;
		switch (oper) {
			case EQ:	return *(db_ulong_t *)v1 == v2;
			case NEQ:	return *(db_ulong_t *)v1 != v2;
			case LEQ:	return *(db_ulong_t *)v1 <= v2;
			case GEQ:	return *(db_ulong_t *)v1 >= v2;
			case LT:	return *(db_ulong_t *)v1 < v2;
			case GT:	return *(db_ulong_t *)v1 > v2;
		}
	} else if (attr->at_domain == FLOAT) {
		db_float_t v2 = val->ptr.vfloat;
		switch (oper) {
			case EQ:	return *(db_float_t *)v1 == v2;
			case NEQ:	return *(db_float_t *)v1 != v2;
			case LEQ:	return *(db_float_t *)v1 <= v2;
			case GEQ:	return *(db_float_t *)v1 >= v2;
			case LT:	return *(db_float_t *)v1 < v2;
			case GT:	return *(db_float_t *)v1 > v2;
		}
	} else if (attr->at_domain == DOUBLE) {
		db_double_t v2 = val->ptr.vdouble;
		switch (oper) {
			case EQ:	return *(db_double_t *)v1 == v2;
			case NEQ:	return *(db_double_t *)v1 != v2;
			case LEQ:	return *(db_double_t *)v1 <= v2;
			case GEQ:	return *(db_double_t *)v1 >= v2;
			case LT:	return *(db_double_t *)v1 < v2;
			case GT:	return *(db_double_t *)v1 > v2;
		}
	} else if (attr->at_domain == STRING) {
		char *v2 = val->ptr.pstring;
		size_t s = attr->at_size;
		switch (oper) {
			case EQ:	return strncmp(v1, v2, s) == 0;
			case NEQ:	return strncmp(v1, v2, s) != 0;
			case LEQ:	return strncmp(v1, v2, s) <= 0;
			case GEQ:	return strncmp(v1, v2, s) >= 0;
			case LT:	return strncmp(v1, v2, s) < 0;
			case GT:	return strncmp(v1, v2, s) > 0;
		}
	} else if (attr->at_domain == BYTES) {
		char *v2 = val->ptr.pbytes;
		size_t s = attr->at_size;
		switch (oper) {
			case EQ:	return memcmp(v1, v2, s) == 0;
			case NEQ:	return memcmp(v1, v2, s) != 0;
			case LEQ:	return memcmp(v1, v2, s) <= 0;
			case GEQ:	return memcmp(v1, v2, s) >= 0;
			case LT:	return memcmp(v1, v2, s) < 0;
			case GT:	return memcmp(v1, v2, s) > 0;
		}
	}
	return false;
}

bool expr_check(const char *tuple, struct expr **exprs, int excnt)
{
	int i;

	for (i = 0; i < excnt; i++) {
		struct expr *e;
		struct sattr *a;
		struct value *v;

		e = exprs[i];
		assert(e != NULL);
		assert(e->type == LEAF); 
		assert(e->stype[0] == SON_SATTR); 
		assert(e->stype[1] == SON_VALUE); 

		a = e->sons[0].sattr;
		v = e->sons[1].value;

		if (!sattr_check(tuple, a, e->op, v))
			return false;
	}
	return true;
}

void expr_tree_free(struct expr *expr)
{
	int i;

	if (expr != NULL) {
		for (i = 0; i < 2; i++) {
			struct value *value;

			switch (expr->stype[i]) {
				case SON_EXPR:
					expr_tree_free(expr->sons[i].expr);
					break;
				case SON_ATTR:
					free(expr->sons[i].attr->tbl_name);
					free(expr->sons[i].attr->attr_name);
					free(expr->sons[i].attr);
					break;
				case SON_SATTR:
					/* do nothing! */
					break;
				case SON_VALUE:
					value = expr->sons[i].value;
					if (value->domain == STRING)
						free(value->ptr.pstring);
					else if (value->domain == BYTES)
						free(value->ptr.pbytes);
					free(value);
					break;
				default:
					assert(false);
			}
		}
		free(expr);
	}
}

#ifndef NDEBUG

static char *expr_to_str(struct expr *expr)
{
	static char buf[1024];
	char *s;

	assert(expr != NULL);

	switch (expr->op) {
		case AND:	return "AND";
		case OR:	return "OR";
		case NAND:	return "NAND";
		case NOR:	return "NOR";
		case EQ:	s = "="; break;
		case NEQ:	s = "!="; break;
		case LEQ:	s = "<="; break;
		case GEQ:	s = ">="; break;
		case GT:	s = ">"; break;
		case LT:	s = "<"; break;
		default:	s = "invalid";
	}
	if (expr->type == LEAF) {
		int i;

		for (i = 0; i < 2; i++) {
			if (expr->stype[i] == SON_ATTR)
				strcpy(buf, expr->sons[i].attr->attr_name);
			else if (expr->stype[i] == SON_SATTR)
				strcpy(buf, expr->sons[i].sattr->at_name);
			else if (expr->stype[i] == SON_VALUE) {
				struct value *v;

				v = expr->sons[i].value;
				if (v->domain == INT)
					sprintf(buf+strlen(buf), DB_INT_FMT,
							v->ptr.vint);
				else if (v->domain == FLOAT)
					sprintf(buf+strlen(buf), DB_FLOAT_FMT,
							v->ptr.vfloat);
				else if (v->domain == STRING)
					sprintf(buf+strlen(buf), "%s",
							v->ptr.pstring);
			}
		}
	} else
		sprintf(buf, "%s", s);
	return buf;
}

void draw_dnf(char *name, struct expr ***dnf)
{
	int i, j;
	FILE *stream;
	
	stream = fopen(name, "w");

	assert(stream != NULL);
	assert(dnf != NULL);

	fprintf(stream, "digraph {\n");

	fprintf(stream, "%d[label=\"OR\"]\n", (int)dnf);
	for (i = 0; dnf[i]; i++) {
		fprintf(stream, "%d[label=\"AND\"]\n", (int)dnf[i]);
		fprintf(stream, "%d -> %d\n", (int)dnf, (int)dnf[i]);
		for (j = 0; dnf[i][j]; j++) {
			fprintf(stream, "%d[label=\"%s\"]\n", (int)dnf[i][j],
					expr_to_str(dnf[i][j]));
			fprintf(stream, "%d -> %d\n", (int)dnf[i],
					(int)dnf[i][j]);
		}
	}

	fprintf(stream, "}\n");
	fclose(stream);
}

static void draw_expr(struct expr *expr, FILE *stream)
{
	assert(expr != NULL);
	assert(stream != NULL);

	fprintf(stream, "%d[label=\"%s\"]\n", (int)expr, expr_to_str(expr));
	if (expr->type == INNER) {
		draw_expr(expr->sons[0].expr, stream);
		fprintf(stream, "%d -> %d\n", (int)expr,
				(int)expr->sons[0].expr);
		draw_expr(expr->sons[1].expr, stream);
		fprintf(stream, "%d -> %d\n", (int)expr,
				(int)expr->sons[1].expr);
	}
}

void draw_expr_tree(char *name, struct expr *root)
{
	FILE *stream;
	
	stream = fopen(name, "w");

	assert(stream != NULL);
	assert(root != NULL);

	fprintf(stream, "digraph {\n");
	draw_expr(root, stream);
	fprintf(stream, "}\n");
	fclose(stream);
}

#endif

