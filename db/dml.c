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

#include "dml.h"
#include "block.h"
#include "attr.h"
#include "constants.h"
#include "db.h"
#include "err.h"
#include "expr.h"
#include "ixmngt.h"
#include "mem.h"
#include "printer.h"
#include "rlmngt.h"
#include "rlalg.h"
#include "sp.h"
#include "verif.h"
#include "view.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static struct xattr *sattr_to_xattr(struct sattr *a, struct xrel *p0,
		struct xrel *p1)
{
	int i;

	assert(a != NULL);
	assert(p0 != NULL || p1 != NULL);

	for (i = 0; i < p0->rl_atcnt; i++)
		if (p0->rl_attrs[i]->at_sattr == a)
			return p0->rl_attrs[i];
	for (i = 0; i < p1->rl_atcnt; i++)
		if (p1->rl_attrs[i]->at_sattr == a)
			return p1->rl_attrs[i];
	return NULL;
}

static struct xexpr *expr_to_xexpr(struct expr *e, struct xrel *p0,
		struct xrel *p1)
{
	struct xexpr *x;

	assert(e->type == LEAF);
	assert(e->stype[0] == SON_SATTR);
	assert(e->stype[1] == SON_SATTR || e->stype[1] == SON_VALUE);

	x = xmalloc(sizeof(struct xexpr));
	x->ex_type = (e->stype[1] == SON_VALUE) ? ATTR_TO_VAL : ATTR_TO_ATTR;
	x->ex_compar = e->op;
	x->ex_left_attr = sattr_to_xattr(e->sons[0].sattr, p0, p1);
	if (x->ex_left_attr == NULL) {
		free(x);
		return NULL;
	}
	if (x->ex_type == ATTR_TO_VAL) {
		struct value *v;

		v = e->sons[1].value;
		switch (v->domain) {
			case INT:
				x->ex_right_val = &v->ptr.vint;
				break;
			case UINT:
				x->ex_right_val = &v->ptr.vuint;
				break;
			case LONG:
				x->ex_right_val = &v->ptr.vlong;
				break;
			case ULONG:
				x->ex_right_val = &v->ptr.vulong;
				break;
			case FLOAT:
				x->ex_right_val = &v->ptr.vfloat;
				break;
			case DOUBLE:
				x->ex_right_val = &v->ptr.vdouble;
				break;
			case STRING:
				x->ex_right_val = v->ptr.pstring;
				break;
			case BYTES:
				x->ex_right_val = v->ptr.pbytes;
				break;
			default:
				assert(false);
		}
		if (x->ex_right_val == NULL) {
			free(x);
			return NULL;
		}
		x->ex_right_attr = NULL;
	} else {
		x->ex_right_val = NULL;
		x->ex_right_attr = sattr_to_xattr(e->sons[1].sattr, p0, p1);
		if (x->ex_right_attr == NULL) {
			free(x);
			return NULL;
		}
	}
	return x;
}

static struct xexpr **exprs_to_xexprs(struct expr **exprs, int cnt,
		struct xrel *parent0, struct xrel *parent1)
{
	struct xexpr **xexprs;
	int i;

	xexprs = xmalloc(cnt * sizeof(struct xexpr *));
	for (i = 0; i < cnt; i++) {
		if ((xexprs[i] = expr_to_xexpr(exprs[i], parent0, parent1))
				== NULL) {
			while (--i >= 0)
				free(xexprs[i]);
			free(xexprs);
			return NULL;
		}
	}
	return xexprs;
}

static struct xattr *attr_to_xattr(struct attr *a, struct xrel *p)
{
	int i;

	assert(a != NULL);
	assert(a->attr_name != NULL);
	assert(p != NULL);

	for (i = 0; i < p->rl_atcnt; i++) {
		struct srel *srl;
		struct sattr *sattr;

		srl = p->rl_attrs[i]->at_srl;
		sattr = p->rl_attrs[i]->at_sattr;
		if ((a->tbl_name == NULL || !strcmp(a->tbl_name,
						srl->rl_header.hd_name))
				&& !strcmp(a->attr_name, sattr->at_name))
			return p->rl_attrs[i];
	}
	return NULL;
}

static struct xattr **attrs_to_xattrs(struct attr **attrs, int cnt,
		struct xrel *parent)
{
	struct xattr **xattrs;
	int i;

	xattrs = xmalloc(cnt * sizeof(struct xattr *));
	for (i = 0; i < cnt; i++) {
		if ((xattrs[i] = attr_to_xattr(attrs[i], parent)) == NULL) {
			free(xattrs);
			return NULL;
		}
	}
	return xattrs;
}

struct xrel *dml_query(struct dml_query *query)
{
	assert(query != NULL);

	if (!dml_query_verify(query))
		return NULL;

	switch (query->type) {
		case SELECTION:
			return dml_select(query->ptr.selection);
		case PROJECTION:
			return dml_project(query->ptr.projection);
		case UNION:
			return dml_union(query->ptr.runion);
		case JOIN:
			return dml_join(query->ptr.join);
		case SORT:
			return dml_sort(query->ptr.sort);
		default:
			return false;
	}
}

struct xrel *load_xrel(struct srcrl *srcrl)
{
	struct srel *srl;
	struct xrel *xrl;
	struct dml_query *query;

	assert(srcrl != NULL);

	switch (srcrl->type) {
		case SRC_TABLE:
			srl = open_relation(srcrl->ptr.tbl_name);
			if (srl == NULL)
				return NULL;
			xrl = wrapper_init(srl);
			return xrl;
		case SRC_VIEW:
			query = open_view(srcrl->ptr.view_name);
			if (query == NULL) {
				ERR(E_COULD_NOT_OPEN_VIEW);
				return NULL;
			}
			xrl = dml_query(query);
			return xrl;
		case SRC_QUERY:
			xrl = dml_query(srcrl->ptr.dml_query);
			return xrl;
		default:
			assert(false);
			return NULL;
	}
}

bool dml_sp(struct dml_sp *sp, struct value *result)
{
	assert(sp != NULL);
	assert(result != NULL);

	return sp_vrun(sp->name, sp->argc, sp->argv, result);
}

bool dml_modi(struct dml_modi *modi, tpcnt_t *cnt_ptr)
{
	assert(modi != NULL);

	if (!dml_modi_verify(modi))
		return false;

	if (cnt_ptr != NULL)
		*cnt_ptr = 0;

	switch (modi->type) {
		case INSERTION:
			if (dml_insert(modi->ptr.insertion)) {
				if (cnt_ptr != NULL)
					*cnt_ptr = 1;
				return true;
			} else 
				return false;
		case DELETION:
			return dml_delete(modi->ptr.deletion, cnt_ptr);
		case UPDATE:
			return dml_update(modi->ptr.update, cnt_ptr);
		default:
			return false;
	}

}

struct xrel *dml_select(struct selection *selection)
{
	struct xrel *rl;

	assert(selection != NULL);

	rl = load_xrel(&selection->parent);
	if (rl == NULL) {
		ERR(E_OPEN_RELATION_FAILED);
		return NULL;
	}

	if (selection->expr_tree != NULL) {
		struct expr ***dnf;
		struct xrel *union_rl;
		int i, j;

		if (!expr_init(selection->expr_tree)) {
			xrel_free(rl);
			ERR(E_EXPR_INIT_FAILED);
			return NULL;
		}

		dnf = formula_to_dnf(selection->expr_tree);

		union_rl = NULL;
		for (i = 0; dnf[i] != NULL; i++) {
			struct xrel *selection_rl;
			struct xexpr **xexprs;
			int cnt;

			/* reload rl to avoid double free()s later when 
			 * xrel_free() is called. reloading is not nice but
			 * works so far. */
			if (i > 0)
				rl = load_xrel(&selection->parent);

			/* count conjunctions */
			for (cnt = 0; dnf[i][cnt] != NULL; cnt++)
				;
			xexprs = exprs_to_xexprs(dnf[i], cnt, rl, NULL);
			selection_rl = selection_init(rl, xexprs, cnt);
			if (union_rl == NULL)
				union_rl = selection_rl;
			else
				union_rl = union_init(union_rl,
						selection_rl);

			for (j = 0; j < cnt; j++)
				free(xexprs[j]);
			free(xexprs);
		}

		for (i = 0; dnf[i]; i++) {
			for (j = 0; dnf[i][j]; j++)
				free(dnf[i][j]);
			free(dnf[i]);
		}
		free(dnf);

		return union_rl;
	} else
		return selection_init(rl, NULL, 0);
}

struct xrel *dml_project(struct projection *projection)
{
	struct xrel *rl, *result;
	struct xattr **xattrs;

	assert(projection != NULL);

	rl = load_xrel(&projection->parent);
	if (rl == NULL) {
		ERR(E_OPEN_RELATION_FAILED);
		return NULL;
	}

	xattrs = attrs_to_xattrs(projection->attrs, projection->atcnt, rl);
	result = projection_init(rl, xattrs, projection->atcnt);
	free(xattrs);
	return result;
}

struct xrel *dml_union(struct runion *runion)
{
	struct xrel *rls[2];

	assert(runion != NULL);

	rls[0] = load_xrel(&runion->parents[0]);
	if (rls[0] == NULL) {
		ERR(E_OPEN_RELATION_FAILED);
		return NULL;
	}

	rls[1] = load_xrel(&runion->parents[1]);
	if (rls[1] == NULL) {
		ERR(E_OPEN_RELATION_FAILED);
		xrel_free(rls[1]);
		return NULL;
	}

	return union_init(rls[0], rls[1]);
}

struct xrel *dml_join(struct join *join)
{
	struct xrel *rls[2];

	assert(join != NULL);

	rls[0] = load_xrel(&join->parents[0]);
	if (rls[0] == NULL) {
		ERR(E_OPEN_RELATION_FAILED);
		return NULL;
	}

	rls[1] = load_xrel(&join->parents[1]);
	if (rls[1] == NULL) {
		xrel_free(rls[0]);
		ERR(E_OPEN_RELATION_FAILED);
		return NULL;
	}

	if (join->expr_tree != NULL) { /* join condition specified */
		struct xrel *result;
		struct expr ***dnf;
		int i, j;

		if (!expr_init(join->expr_tree)) {
			xrel_free(rls[0]);
			xrel_free(rls[1]);
			ERR(E_EXPR_INIT_FAILED);
			return NULL;
		}

		dnf = formula_to_dnf(join->expr_tree);

		result = NULL;
		for (i = 0; dnf[i] != NULL; i++) {
			struct xrel *join_rl;
			struct xexpr **xexprs;
			int cnt;

			/* reload rl to avoid double free()s later when 
			 * xrel_free() is called. reloading is not nice but
			 * works so far. */
			if (i > 0) {
				rls[0] = load_xrel(&join->parents[0]);
				rls[1] = load_xrel(&join->parents[1]);
			}

			/* count conjunctions */
			for (cnt = 0; dnf[i][cnt] != NULL; cnt++)
				;
			xexprs = exprs_to_xexprs(dnf[i], cnt, rls[0], rls[1]);
			join_rl = join_init(rls[0], rls[1], xexprs, cnt);
			if (result == NULL)
				result = join_rl;
			else
				result = union_init(result, join_rl);

			for (j = 0; j < cnt; j++)
				free(xexprs[j]);
			free(xexprs);
		}

		for (i = 0; dnf[i]; i++) {
			for (j = 0; dnf[i][j]; j++)
				free(dnf[i][j]);
			free(dnf[i]);
		}
		free(dnf);

		return result;
	} else { /* natural join */
		struct xrel *result;
		struct xexpr **xexprs, *e;
		int i, j, cnt, size;
		struct xattr *xa0, *xa1;
		struct sattr *a0, *a1;

		cnt = 0;
		size = 10;
		xexprs = xmalloc(size * sizeof(struct xexpr *));

		/* determine common attributes (i.e. those attributes that 
		 * have the same domain and same name. */
		for (i = 0; i < rls[0]->rl_atcnt; i++) {
			xa0 = rls[0]->rl_attrs[i];
			a0 = xa0->at_sattr;
			for (j = 0; j < rls[1]->rl_atcnt; j++) {
				xa1 = rls[1]->rl_attrs[j];
				a1 = xa1->at_sattr;
				if (a0->at_domain == a1->at_domain
						&& !strncmp(a0->at_name,
							a1->at_name,
							AT_NAME_MAX)) {
					e = xmalloc(sizeof(struct xexpr));
					e->ex_type = ATTR_TO_ATTR;
					e->ex_left_attr = xa0;
					e->ex_compar = EQ;
					e->ex_right_attr = xa1;
					xexprs[cnt++] = e;
					if (cnt == size) {
						size++;
						xexprs = xrealloc(xexprs,
							size *
							sizeof(struct xexpr *));
					}
				}
			}
		}

		result = join_init(rls[0], rls[1], xexprs, cnt);

		for (j = 0; j < cnt; j++)
			free(xexprs[j]);
		free(xexprs);

		return result;
	}
}

struct xrel *dml_sort(struct sort *sort)
{
	struct xrel *rl, *result;
	struct xattr **xattrs;

	assert(sort != NULL);
	assert(sort->atcnt > 0);

	rl = load_xrel(&sort->parent);
	if (rl == NULL) {
		ERR(E_OPEN_RELATION_FAILED);
		return NULL;
	}

	xattrs = attrs_to_xattrs(sort->attrs, sort->atcnt, rl);
	result = sort_init(rl, xattrs, sort->orders, sort->atcnt);
	free(xattrs);
	return result;
}

struct index *try_open_index(struct srel *rl, struct expr **conj)
{
	struct sattr *sattr;

	assert(rl != NULL);

	if (conj == NULL)
		return NULL;
	if (conj[0] == NULL)
		return NULL;

	assert(conj[0]->type == LEAF);
	assert(conj[0]->stype[0] == SON_SATTR);

	sattr = conj[0]->sons[0].sattr;
	return open_index(rl, sattr);
}

bool dml_insert(struct insertion *insertion)
{
	struct srel *rl;

	assert(insertion != NULL);
	assert(insertion->atcnt == insertion->valcnt);
	assert(insertion->atcnt > 0);
	assert(insertion->atcnt <= ATTR_MAX);

	if ((rl = open_relation(insertion->tbl_name)) == NULL) {
		ERR(E_OPEN_RELATION_FAILED);
		return false;
	}

	if (rl != NULL) {
		char tuple[rl->rl_header.hd_tpsize];
		int i;

		assert(insertion->atcnt <= rl->rl_header.hd_atcnt);
		
		for (i = 0; i < rl->rl_header.hd_atcnt; i++) {
			struct sattr *sattr;
			struct value *value;
			int j;

			sattr = &rl->rl_header.hd_attrs[i];
			value = NULL;
			for (j = 0; j < insertion->atcnt; j++) {
				char *attr_name;

				attr_name = insertion->attrs[j]->attr_name;
				if (!strncmp(sattr->at_name, attr_name,
							AT_NAME_MAX)) {
					value = insertion->values[j];
					break;
				}
			}
			if (value == NULL) {
				ERR(E_ATTRIBUTE_NOT_INITIALIZED);
				return false;
			}

			set_sattr_val(tuple, sattr, value);
		}

		if (!insert_into_relation(rl, tuple)) {
			ERR(E_IO_ERROR);
			return false;
		} else
			return true;
	} else
		return false;
}

static bool delete_helper(struct srel *rl, struct expr **conj, tpcnt_t *tpcnt)
{
	struct index *ix;
	int conj_cnt;

	assert(rl != NULL);
	assert(tpcnt != NULL);

	for (conj_cnt = 0; conj != NULL && conj[conj_cnt] != NULL; conj_cnt++)
		;

	if ((ix = try_open_index(rl, conj)) != NULL) {
		struct ix_iter *iter;
		void *key;
		blkaddr_t (*nextf)(struct ix_iter *);
		blkaddr_t addr;
		struct value *value;

		value = conj[0]->sons[1].value;
		switch (value->domain) {
			case INT:
				key = &value->ptr.vint;
				break;
			case UINT:
				key = &value->ptr.vuint;
				break;
			case LONG:
				key = &value->ptr.vlong;
				break;
			case ULONG:
				key = &value->ptr.vulong;
				break;
			case FLOAT:
				key = &value->ptr.vfloat;
				break;
			case DOUBLE:
				key = &value->ptr.vdouble;
				break;
			case STRING:
				key = value->ptr.pstring;
				break;
			case BYTES:
				key = value->ptr.pbytes;
				break;
			default:
				assert(false);
				return false;
		}

delete_next:
		iter = search_in_index(rl, conj[0]->sons[0].sattr,
				conj[0]->op, key);
		assert(iter != NULL);
		nextf = index_iterator_nextf(conj[0]->op);
		while ((addr = nextf(iter)) != INVALID_ADDR) {
			const char *tuple;

			tuple = rl_get(rl, addr);
			assert(tuple != NULL);

			if (!expr_check(tuple, conj, conj_cnt))
				continue;

			if (!delete_from_relation(rl, addr, tuple, tpcnt)) {
				ERR(E_IO_ERROR);
				ix_iter_free(iter);
				return false;
			}
			/* the entry was deleted from the index, hence
			 * we need to search again in the index */
			ix_iter_free(iter);
			goto delete_next;
		}
		ix_iter_free(iter);
	} else {
		struct srel_iter *iter;
		const char *tuple;
		blkaddr_t addr;

		iter = rl_iterator(rl);
		assert(iter != NULL);
		while ((tuple = rl_next(iter)) != NULL) {
			if (!expr_check(tuple, conj, conj_cnt))
				continue;

			addr = iter->it_curaddr;
			if (!delete_from_relation(rl, addr, tuple, tpcnt)) {
				ERR(E_IO_ERROR);
				srel_iter_free(iter);
				return false;
			}
		}
		srel_iter_free(iter);
	}
	return true;
}

bool dml_delete(struct deletion *deletion, tpcnt_t *cnt_ptr)
{
	struct srel *rl;
	tpcnt_t tpcnt;
	bool retval;
	int i;
	struct expr ***dnf;

	assert(deletion != NULL);

	if (!expr_init(deletion->expr_tree)) {
		ERR(E_EXPR_INIT_FAILED);
		return false;
	}

	if (deletion->expr_tree != NULL)
		dnf = formula_to_dnf(deletion->expr_tree);
	else
		dnf = NULL;

	if ((rl = open_relation(deletion->tbl_name)) == NULL) {
		ERR(E_OPEN_RELATION_FAILED);
		return false;
	}

	tpcnt = 0;
	retval = true;
	if (dnf != NULL)
		for (i = 0; dnf[i]; i++)
			retval &= delete_helper(rl, dnf[i], &tpcnt);
	else 
		retval &= delete_helper(rl, NULL, &tpcnt);

	if (dnf != NULL) {
		int j;

		for (i = 0; dnf[i]; i++) {
			for (j = 0; dnf[i][j]; j++)
				free(dnf[i][j]);
			free(dnf[i]);
		}
		free(dnf);
	}

	if (cnt_ptr != NULL)
		*cnt_ptr = tpcnt;
	return retval;
}

static bool update_helper(struct srel *rl, struct sattr **sattrs,
		struct value **values, int cnt, struct expr **conj,
		tpcnt_t *tpcnt)
{
	struct index *ix;
	int conj_cnt;

	assert(rl != NULL);
	assert(sattrs != NULL);
	assert(values != NULL);
	assert(cnt > 0);
	assert(tpcnt != NULL);

	for (conj_cnt = 0; conj != NULL && conj[conj_cnt] != NULL; conj_cnt++)
		;

	if ((ix = try_open_index(rl, conj)) != NULL) {
		struct ix_iter *iter;
		void *key;
		blkaddr_t (*nextf)(struct ix_iter *);
		blkaddr_t addr;
		struct value *value;
		size_t ix_attr_offset, ix_attr_size;

		value = conj[0]->sons[1].value;
		switch (value->domain) {
			case INT:
				key = &value->ptr.vint;
				break;
			case UINT:
				key = &value->ptr.vuint;
				break;
			case LONG:
				key = &value->ptr.vlong;
				break;
			case ULONG:
				key = &value->ptr.vulong;
				break;
			case FLOAT:
				key = &value->ptr.vfloat;
				break;
			case DOUBLE:
				key = &value->ptr.vdouble;
				break;
			case STRING:
				key = value->ptr.pstring;
				break;
			case BYTES:
				key = value->ptr.pbytes;
				break;
			default:
				assert(false);
				return false;
		}

		ix_attr_offset = conj[0]->sons[0].sattr->at_offset;
		ix_attr_size = conj[0]->sons[0].sattr->at_size;

update_next:
		iter = search_in_index(rl, conj[0]->sons[0].sattr,
				conj[0]->op, key);
		assert(iter != NULL);
		nextf = index_iterator_nextf(conj[0]->op);
		while ((addr = nextf(iter)) != INVALID_ADDR) {
			char new_tuple[rl->rl_header.hd_tpsize];
			char old_tuple[rl->rl_header.hd_tpsize];
			const char *tmp_tuple;
			int i;

			tmp_tuple = rl_get(rl, addr);
			if (tmp_tuple == NULL) {
				ERR(E_INDEX_INCONSISTENT);
				ix_iter_free(iter);
				return false;
			}

			if (!expr_check(tmp_tuple, conj, conj_cnt))
				continue;

			memcpy(old_tuple, tmp_tuple, rl->rl_header.hd_tpsize);
			memcpy(new_tuple, tmp_tuple, rl->rl_header.hd_tpsize);
			for (i = 0; i < cnt; i++)
				set_sattr_val(new_tuple, sattrs[i], values[i]);

			if (!update_relation(rl, addr, old_tuple, new_tuple,
						tpcnt)) {
				ERR(E_IO_ERROR);
				ix_iter_free(iter);
				return false;
			}
			if (memcmp(old_tuple + ix_attr_offset,
						new_tuple + ix_attr_offset,
						ix_attr_size) != 0) {
				/* the indexed attribute we search for has 
				 * changed, hence we need to search again in
				 * the index */
				ix_iter_free(iter);
				goto update_next;
			}
		}
		ix_iter_free(iter);
	} else {
		struct srel_iter *iter;
		const char *old_tuple;
		char new_tuple[rl->rl_header.hd_tpsize];
		blkaddr_t addr;
		int i;

		iter = rl_iterator(rl);
		assert(iter != NULL);
		while ((old_tuple = rl_next(iter)) != NULL) {
			if (!expr_check(old_tuple, conj, conj_cnt))
				continue;

			memcpy(new_tuple, old_tuple, rl->rl_header.hd_tpsize);

			for (i = 0; i < cnt; i++)
				set_sattr_val(new_tuple, sattrs[i], values[i]);

			addr = iter->it_curaddr;
			if (!update_relation(rl, addr, old_tuple, new_tuple,
						tpcnt)) {
				ERR(E_IO_ERROR);
				srel_iter_free(iter);
				return false;
			}
		}
		srel_iter_free(iter);
	}
	return true;
}

bool dml_update(struct update *update, tpcnt_t *cnt_ptr)
{
	struct srel *rl;
	struct expr ***dnf;
	struct sattr **sattrs;
	tpcnt_t tpcnt;
	bool retval;
	int i;

	assert(update != NULL);
	assert(update->cnt > 0);

	if (!expr_init(update->expr_tree)) {
		ERR(E_EXPR_INIT_FAILED);
		return false;
	}

	if (update->expr_tree != NULL)
		dnf = formula_to_dnf(update->expr_tree);
	else
		dnf = NULL;

	if ((rl = open_relation(update->tbl_name)) == NULL) {
		ERR(E_OPEN_RELATION_FAILED);
		return false;
	}

	sattrs = xmalloc(update->cnt * sizeof(struct sattr *));
	tpcnt = 0;
	retval = true;
	for (i = 0; i < update->cnt; i++) {
		int j;

		sattrs[i] = NULL;
		for (j = 0; j < rl->rl_header.hd_atcnt; j++) {
			if (!strncmp(rl->rl_header.hd_attrs[j].at_name,
						update->attrs[i]->attr_name,
						AT_NAME_MAX)) {
				sattrs[i] = &rl->rl_header.hd_attrs[j];
			}
		}
		if (sattrs[i] == NULL) {
			ERR(E_ATTRIBUTE_NOT_FOUND);
			free(sattrs);
			return false;
		}
	}

	if (dnf != NULL)
		for (i = 0; dnf[i]; i++)
			retval &= update_helper(rl, sattrs, update->values,
					update->cnt, dnf[i], &tpcnt);
	else 
		retval = update_helper(rl, sattrs, update->values,
				update->cnt, NULL, &tpcnt);

	free(sattrs);

	if (dnf != NULL) {
		int j;

		for (i = 0; dnf[i]; i++) {
			for (j = 0; dnf[i][j]; j++)
				free(dnf[i][j]);
			free(dnf[i]);
		}
		free(dnf);
	}

	if (cnt_ptr != NULL)
		*cnt_ptr = tpcnt;
	return retval;
}

