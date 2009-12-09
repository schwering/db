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

#include "verif.h"
#include "attr.h"
#include "err.h"
#include "hashset.h"
#include "hashtable.h"
#include "io.h"
#include "ixmngt.h"
#include "rlmngt.h"
#include "str.h"
#include "sort.h"
#include "view.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v)	{if (!(v)) return ERR(E_SEMANTIC_ERROR), false;}
#define CHECK_F(v)	{if (!(v)) return gc(id),ERR(E_SEMANTIC_ERROR),false;}

static bool dml_query_verify_helper(struct dml_query *q, mid_t id);

static bool crt_tbl_verify(struct crt_tbl *ptr)
{
	struct attr_dcl *dcl;
	struct srel *rrl;
	struct sattr *rattr;
	int i, j;

	assert(ptr != NULL);

	CHECK(ptr->tbl_name != NULL);
	CHECK(strlen(ptr->tbl_name) <= RL_NAME_MAX);
	CHECK(open_relation(ptr->tbl_name) == NULL);
	errclear(); /* open_relation() has noticed an error */
	CHECK(ptr->cnt > 0);
	CHECK(ptr->cnt <= ATTR_MAX);
	for (i = 0; i < ptr->cnt; i++) {
		dcl = ptr->attr_dcls[i];

		CHECK(strlen(dcl->attr_name) <= AT_NAME_MAX);
		CHECK((dcl->fk_attr_name == NULL && dcl->fk_tbl_name == NULL)
		       || (strlen(dcl->fk_tbl_name) <= RL_NAME_MAX
			       && strlen(dcl->fk_attr_name) <= AT_NAME_MAX));

		for (j = 0; j < i; j++)
			CHECK(strncmp(dcl->attr_name,
						ptr->attr_dcls[j]->attr_name,
						AT_NAME_MAX) != 0);

		if (dcl->fk_tbl_name != NULL && dcl->fk_attr_name != NULL) {
			rrl = open_relation(dcl->fk_tbl_name);
			CHECK(rrl != NULL);
			rattr = NULL;
			for (j = 0; j < rrl->rl_header.hd_atcnt; j++) {
				if (!strncmp(rrl->rl_header.hd_attrs[j].at_name,
							dcl->fk_attr_name,
							AT_NAME_MAX)) {
					rattr = &rrl->rl_header.hd_attrs[j];
					break;
				}
			}
			CHECK(rattr != NULL);
			CHECK(rattr->at_indexed == PRIMARY);
			CHECK(dcl->type->domain == rattr->at_domain);
			CHECK(dcl->type->size == rattr->at_size);
		}
	}
	return true;
}

static bool drp_tbl_verify(struct drp_tbl *ptr)
{
	struct srel *rl;

	assert(ptr != NULL);

	CHECK(ptr->tbl_name != NULL);
	CHECK(strlen(ptr->tbl_name) <= RL_NAME_MAX);
	rl = open_relation(ptr->tbl_name);
	CHECK(rl != NULL);
	return true;
}

static bool crt_view_verify(struct crt_view *ptr)
{
	assert(ptr != NULL);

	CHECK(ptr->view_name != NULL);
	CHECK(open_view(ptr->view_name) == NULL);
	CHECK(dml_query_verify(ptr->query));
	return true;
}

static bool drp_view_verify(struct drp_view *ptr)
{
	struct dml_query *view;

	assert(ptr != NULL);

	CHECK(ptr->view_name != NULL);
	view = open_view(ptr->view_name);
	CHECK(view != NULL);
	return true;
}

static bool crt_ix_verify(struct crt_ix *ptr)
{
	struct srel *rl;
	struct sattr *sattr;
	int i;

	assert(ptr != NULL);

	CHECK(ptr->tbl_name != NULL);
	CHECK(strlen(ptr->tbl_name) <= RL_NAME_MAX);
	CHECK(ptr->attr_name != NULL);
	CHECK(strlen(ptr->attr_name) <= AT_NAME_MAX);
	rl = open_relation(ptr->tbl_name);
	CHECK(rl != NULL);
	sattr = NULL;
	for (i = 0; i < rl->rl_header.hd_atcnt; i++)
		if (!strncmp(rl->rl_header.hd_attrs[i].at_name,
					ptr->attr_name, AT_NAME_MAX))
			sattr = &rl->rl_header.hd_attrs[i];
	CHECK(sattr != NULL);
	CHECK(sattr->at_indexed == NOT_INDEXED);
	return true;
}

static bool drp_ix_verify(struct drp_ix *ptr)
{
	struct srel *rl;
	struct sattr *sattr;
	struct index *ix;
	int i;

	assert(ptr != NULL);

	CHECK(ptr->tbl_name != NULL);
	CHECK(strlen(ptr->tbl_name) <= RL_NAME_MAX);
	CHECK(ptr->attr_name != NULL);
	CHECK(strlen(ptr->attr_name) <= AT_NAME_MAX);
	rl = open_relation(ptr->tbl_name);
	CHECK(rl != NULL);
	sattr = NULL;
	for (i = 0; i < rl->rl_header.hd_atcnt; i++)
		if (!strncmp(rl->rl_header.hd_attrs[i].at_name,
					ptr->attr_name, AT_NAME_MAX))
			sattr = &rl->rl_header.hd_attrs[i];
	CHECK(sattr != NULL);
	CHECK(sattr->at_indexed != NOT_INDEXED);
	ix = open_index(rl, sattr);
	CHECK(ix != NULL);
	return true;
}

bool ddl_stmt_verify(struct ddl_stmt *ptr)
{
	assert(ptr != NULL);

	switch (ptr->type) {
		case CREATE_TABLE:
			CHECK(crt_tbl_verify(ptr->ptr.crt_tbl));
			break;
		case DROP_TABLE:
			CHECK(drp_tbl_verify(ptr->ptr.drp_tbl));
			break;
		case CREATE_VIEW:
			CHECK(crt_view_verify(ptr->ptr.crt_view));
			break;
		case DROP_VIEW:
			CHECK(drp_view_verify(ptr->ptr.drp_view));
			break;
		case CREATE_INDEX:
			CHECK(crt_ix_verify(ptr->ptr.crt_ix));
			break;
		case DROP_INDEX:
			CHECK(drp_ix_verify(ptr->ptr.drp_ix));
			break;
	}
	return true;
}

static int srel_load_attrs(char *tbl_name, struct attr ***attrs_ptr, mid_t id)
{
	struct srel *rl;
	struct sattr *sattr;
	struct attr **attrs, *attr;
	int i, len;

	assert(tbl_name != NULL);
	assert(attrs_ptr != NULL);

	rl = open_relation(tbl_name);
	if (rl == NULL)
		return -1;
	
	len = strlen(rl->rl_header.hd_name);
	attrs = gmalloc(rl->rl_header.hd_atcnt * sizeof(struct attr *), id);
	for (i = 0; i < rl->rl_header.hd_atcnt; i++) {
		sattr = &rl->rl_header.hd_attrs[i];
		attr = gmalloc(sizeof(struct attr), id);
		attr->tbl_name = gmalloc(len + 1, id);
		strcpy(attr->tbl_name, rl->rl_header.hd_name);
		attr->attr_name = gmalloc(strlen(sattr->at_name) + 1, id);
		strcpy(attr->attr_name, sattr->at_name);
		attrs[i] = attr;
	}
	*attrs_ptr = attrs;
	return rl->rl_header.hd_atcnt;
}

static int srcrl_load_attrs(struct srcrl *srcrl, struct attr ***attrs_ptr,
		mid_t id);

static int query_load_attrs(struct dml_query *q, struct attr ***attrs_ptr,
		mid_t id)
{
	struct attr **attrs0, **attrs1, **attrs;
	int atcnt0, atcnt1, atcnt, i;

	assert(q != NULL);
	assert(attrs_ptr != NULL);

	if (!dml_query_verify(q))
		return -1;

	switch (q->type) {
		case SELECTION:
			return srcrl_load_attrs(&q->ptr.selection->parent,
					attrs_ptr, id);
		case PROJECTION:
			atcnt = q->ptr.projection->atcnt;
			attrs = gmalloc(atcnt * sizeof(struct attr *), id);
			for (i = 0; i < atcnt; i++) {
				struct attr *a, *b;

				b = q->ptr.projection->attrs[i];
				a = gmalloc(sizeof(struct attr), id);
				a->tbl_name = copy_gc(b->tbl_name, 
						strsize(b->tbl_name), id);
				a->attr_name = copy_gc(b->attr_name, 
						strsize(b->attr_name), id);
				attrs[i] = a;
			}
			*attrs_ptr = attrs;
			return atcnt;
		case UNION:
			return srcrl_load_attrs(&q->ptr.runion->parents[0],
					attrs_ptr, id);
		case JOIN:
			atcnt0 = srcrl_load_attrs(&q->ptr.join->parents[0],
					&attrs0, id);
			atcnt1 = srcrl_load_attrs(&q->ptr.join->parents[1],
					&attrs1, id);
			attrs = gmalloc((atcnt0+atcnt1)*sizeof(struct attr *),
					id);
			for (i = 0; i < atcnt0; i++)
				attrs[i] = attrs0[i];
			for (i = 0; i < atcnt1; i++)
				attrs[atcnt0+i] = attrs1[i];
			*attrs_ptr = attrs;
			return atcnt0 + atcnt1;
		case SORT:
			return srcrl_load_attrs(&q->ptr.sort->parent,
					attrs_ptr, id);
	}
	return -1;
}

static int view_load_attrs(char *view_name, struct attr ***attrs_ptr, mid_t id)
{
	struct dml_query *view;

	assert(view_name != NULL);
	assert(attrs_ptr != NULL);

	view = open_view(view_name);
	if (view == NULL)
		return -1;
	return query_load_attrs(view, attrs_ptr, id);
}

static int srcrl_load_attrs(struct srcrl *srcrl, struct attr ***attrs_ptr,
		mid_t id)
{
	assert(srcrl != NULL);
	assert(attrs_ptr != NULL);

	switch (srcrl->type) {
		case SRC_TABLE:
			return srel_load_attrs(srcrl->ptr.tbl_name, attrs_ptr,
					id);
		case SRC_VIEW:
			return view_load_attrs(srcrl->ptr.view_name, attrs_ptr,
					id);
		case SRC_QUERY:
			return query_load_attrs(srcrl->ptr.dml_query, attrs_ptr,
					id);
	}
	return -1;
}

static bool attr_in_attrs(struct attr *attr, struct attr **attrs, int atcnt)
{
	int i;

	for (i = 0; i < atcnt; i++) {
		if (!strncmp(attr->tbl_name, attrs[i]->tbl_name, RL_NAME_MAX)
				&& !strncmp(attr->attr_name,
					attrs[i]->attr_name, AT_NAME_MAX))
			return true;
	}
	return false;
}

static bool expr_tree_verify_values(struct expr *tree,
		struct attr **attrs, int atcnt)
{
	if (tree == NULL)
		return true;

	if (tree->type == INNER) {
		assert(tree->stype[0] == SON_EXPR);
		assert(tree->stype[1] == SON_EXPR);
		return expr_tree_verify_values(tree->sons[0].expr, attrs, atcnt)
			&& expr_tree_verify_values(tree->sons[1].expr, attrs,
					atcnt);
	} else if (tree->type == LEAF) {
		bool b1, b2;
		struct sattr *sattr;
		struct attr *attr;
		struct value *value;

		b1 = tree->stype[0] == SON_ATTR && tree->stype[1] == SON_VALUE;
		b2 = tree->stype[1] == SON_ATTR && tree->stype[0] == SON_VALUE;
		CHECK(b1 || b2);
		if (b1) {
			attr = tree->sons[0].attr;
			CHECK(attr_in_attrs(attr, attrs, atcnt));
			sattr = sattr_by_attr(attr);
			value = tree->sons[1].value;
			CHECK(sattr != NULL);
			CHECK(sattr->at_domain == value->domain);
			return true;
		} else if (b2) {
			attr = tree->sons[0].attr;
			CHECK(attr_in_attrs(attr, attrs, atcnt));
			sattr = sattr_by_attr(attr);
			value = tree->sons[0].value;
			CHECK(sattr != NULL);
			CHECK(sattr->at_domain == value->domain);
			return true;
		}
	}
	return false;
}

static bool expr_tree_verify_attrs(struct expr *tree,
		struct attr **attrs0, int atcnt0,
		struct attr **attrs1, int atcnt1)
{
	if (tree == NULL)
		return true;

	if (tree->type == INNER) {
		assert(tree->stype[0] == SON_EXPR);
		assert(tree->stype[1] == SON_EXPR);
		return expr_tree_verify_attrs(tree->sons[0].expr,
				attrs0, atcnt0, attrs1, atcnt1)
			&& expr_tree_verify_attrs(tree->sons[1].expr, 
				attrs0, atcnt0, attrs1, atcnt1);
	} else if (tree->type == LEAF) {
		struct attr *attr0, *attr1;
		struct sattr *sattr0, *sattr1;

		CHECK(tree->stype[0] == SON_ATTR);
		CHECK(tree->stype[1] == SON_ATTR);
		attr0 = tree->sons[0].attr;
		attr1 = tree->sons[1].attr;
		CHECK(strcmp(attr0->tbl_name, attr1->tbl_name) != 0);
		CHECK(attr_in_attrs(attr0, attrs0, atcnt0)
				|| attr_in_attrs(attr0, attrs1, atcnt1));
		CHECK(attr_in_attrs(attr1, attrs0, atcnt0)
				|| attr_in_attrs(attr1, attrs1, atcnt1));
		sattr0 = sattr_by_attr(attr0);
		sattr1 = sattr_by_attr(attr1);
		CHECK(sattr0 != NULL);
		CHECK(sattr1 != NULL);
		CHECK(sattr0->at_domain == sattr1->at_domain);
		return true;
	}
	return false;
}

static bool attrs_contained(struct attr **big, int bigcnt,
		struct attr **little, int littlecnt)
{
	int i;

	for (i = 0; i < littlecnt; i++) {
		bool found;
		int j;
		char *tbl_name, *attr_name;
		struct attr *attr;

		tbl_name = little[i]->tbl_name;
		attr_name = little[i]->attr_name;
		found = false;
		for (j = 0; j < bigcnt; j++) {
			attr = big[j];
			if (!strncmp(tbl_name, attr->tbl_name, RL_NAME_MAX)
					&& !strncmp(attr_name, attr->attr_name,
						AT_NAME_MAX))
				found = true;
		}
		CHECK(found);
	}
	return true;
}

static bool attrs_equal(struct attr **attrs0, int atcnt0,
		struct attr **attrs1, int atcnt1)
{
	int i;

	CHECK(atcnt0 == atcnt1);
	for (i = 0; i < atcnt0; i++) {
		char *t0, *t1, *a0, *a1;

		t0 = attrs0[i]->tbl_name;
		a0 = attrs0[i]->attr_name;
		t1 = attrs1[i]->tbl_name;
		a1 = attrs1[i]->attr_name;
		CHECK(!strncmp(t0, t1, RL_NAME_MAX)
				&& !strncmp(a0, a1, AT_NAME_MAX));
	}
	return true;
}

static bool attrs_disjunct(struct attr **attrs0, int atcnt0,
		struct attr **attrs1, int atcnt1)
{
	int i;

	for (i = 0; i < atcnt0; i++) {
		char *t0, *t1, *a0, *a1;
		int j;

		t0 = attrs0[i]->tbl_name;
		a0 = attrs0[i]->attr_name;
		for (j = 0; j < atcnt1; j++) {
			t1 = attrs1[j]->tbl_name;
			a1 = attrs1[j]->attr_name;
			CHECK(strncmp(t0, t1, RL_NAME_MAX) != 0
					|| strncmp(a0, a1, AT_NAME_MAX) != 0);
		}
	}
	return true;
}

static bool selection_verify(struct selection *s, mid_t id)
{
	struct attr **attrs;
	int atcnt;

	atcnt = srcrl_load_attrs(&s->parent, &attrs, id);
	CHECK(atcnt > 0);
	CHECK(expr_tree_verify_values(s->expr_tree, attrs, atcnt));
	return true;
}

static bool projection_verify(struct projection *p, mid_t id)
{
	struct attr **attrs;
	int atcnt;

	atcnt = srcrl_load_attrs(&p->parent, &attrs, id);
	CHECK(atcnt > 0);
	CHECK(attrs_contained(attrs, atcnt, p->attrs, p->atcnt));
	return true;
}

static bool runion_verify(struct runion *u, mid_t id)
{
	struct attr **attrs0, **attrs1;
	int atcnt0, atcnt1;

	atcnt0 = srcrl_load_attrs(&u->parents[0], &attrs0, id);
	atcnt1 = srcrl_load_attrs(&u->parents[1], &attrs1, id);
	CHECK(atcnt0 > 0);
	CHECK(atcnt1 > 0);
	CHECK(attrs_equal(attrs0, atcnt0, attrs1, atcnt1));
	return true;
}

static bool join_verify(struct join *j, mid_t id)
{
	struct attr **attrs0, **attrs1;
	int atcnt0, atcnt1;

	atcnt0 = srcrl_load_attrs(&j->parents[0], &attrs0, id);
	atcnt1 = srcrl_load_attrs(&j->parents[1], &attrs1, id);
	CHECK(atcnt0 > 0);
	CHECK(atcnt1 > 0);
	CHECK(attrs_disjunct(attrs0, atcnt0, attrs1, atcnt1));
	CHECK(expr_tree_verify_attrs(j->expr_tree, attrs0, atcnt0,
				attrs1, atcnt1));
	return true;
}

static bool sort_verify(struct sort *s, mid_t id)
{
	struct attr **attrs;
	int atcnt, i;

	atcnt = srcrl_load_attrs(&s->parent, &attrs, id);
	for (i = 0; i < s->atcnt; i++) {
		CHECK(attr_in_attrs(s->attrs[i], attrs, atcnt));
		CHECK(s->orders[i] == ASCENDING || s->orders[i] == DESCENDING);
	}
	return true;
}

static bool dml_query_verify_helper(struct dml_query *q, mid_t id)
{
	assert(q != NULL);

	switch (q->type) {
		case SELECTION:
			CHECK(selection_verify(q->ptr.selection, id));
			break;
		case PROJECTION:
			CHECK(projection_verify(q->ptr.projection, id));
			break;
		case UNION:
			CHECK(runion_verify(q->ptr.runion, id));
			break;
		case JOIN:
			CHECK(join_verify(q->ptr.join, id));
			break;
		case SORT:
			CHECK(sort_verify(q->ptr.sort, id));
			break;
	}
	return true;
}

bool dml_query_verify(struct dml_query *q)
{
	mid_t id;

	assert(q != NULL);

	id = gnew();
	CHECK_F(dml_query_verify_helper(q, id));
	gc(id);
	return true;
}

bool dml_sp_verify(struct dml_sp *sp)
{
	assert(sp != NULL);

	CHECK(strlen(sp->name) > 0);
	return true;
}

static bool insertion_verify(struct insertion *i)
{
	struct srel *rl;
	int j, k;
	bool attrs[ATTR_MAX];

	assert(i != NULL);

	CHECK(i->tbl_name != NULL);
	CHECK(strlen(i->tbl_name) <= RL_NAME_MAX);
	rl = open_relation(i->tbl_name);
	CHECK(rl != NULL);

	CHECK(i->atcnt == rl->rl_header.hd_atcnt);
	CHECK(i->atcnt == i->valcnt);

	for (j = 0; j < i->atcnt; j++) {
		CHECK(!strncmp(i->attrs[j]->tbl_name, i->tbl_name,
					RL_NAME_MAX));
		for (k = 0; k < rl->rl_header.hd_atcnt; k++) {
			if (!strncmp(rl->rl_header.hd_attrs[k].at_name,
						i->attrs[j]->attr_name, 
						AT_NAME_MAX)) {
				attrs[k] = true;
				break;
			}
		}
		CHECK(rl->rl_header.hd_attrs[k].at_domain
				== i->values[j]->domain);
	}

	for (k = 0; k < rl->rl_header.hd_atcnt; k++)
		CHECK(attrs[k]);
	return true;
}

static bool expr_tree_verify_srel(struct expr *tree, struct srel *rl)
{
	if (tree == NULL)
		return true;

	if (tree->type == INNER) {
		assert(tree->stype[0] == SON_EXPR);
		assert(tree->stype[1] == SON_EXPR);
		return expr_tree_verify_srel(tree->sons[0].expr, rl)
			&& expr_tree_verify_srel(tree->sons[1].expr, rl);
	} else if (tree->type == LEAF) {
		struct sattr *sattr;
		bool b1, b2;
		int i;

		b1 = tree->stype[0] == SON_ATTR && tree->stype[1] == SON_VALUE;
		b2 = tree->stype[1] == SON_ATTR && tree->stype[0] == SON_VALUE;
		CHECK(b1 || b2);
		if (b1) {
			sattr = NULL;
			for (i = 0; i < rl->rl_header.hd_atcnt; i++) {
				if (!strncmp(rl->rl_header.hd_attrs[i].at_name,
						tree->sons[0].attr->attr_name,
						AT_NAME_MAX))
					sattr = &rl->rl_header.hd_attrs[i];
			}
			CHECK(sattr != NULL);
			CHECK(sattr->at_domain == tree->sons[1].value->domain);
			return true;
		} else if (b2) {
			sattr = NULL;
			for (i = 0; i < rl->rl_header.hd_atcnt; i++) {
				if (!strncmp(rl->rl_header.hd_attrs[i].at_name,
						tree->sons[0].attr->attr_name,
						AT_NAME_MAX))
					sattr = &rl->rl_header.hd_attrs[i];
			}
			CHECK(sattr != NULL);
			CHECK(sattr->at_domain == tree->sons[1].value->domain);
			return true;
		}
	}
	return false;
}

static bool deletion_verify(struct deletion *d)
{
	struct srel *rl;

	assert(d != NULL);

	CHECK(d->tbl_name != NULL);
	CHECK(strlen(d->tbl_name) <= RL_NAME_MAX);
	rl = open_relation(d->tbl_name);
	CHECK(rl != NULL);
	if (d->expr_tree != NULL)
		CHECK(expr_tree_verify_srel(d->expr_tree, rl));
	return true;
}

static bool update_verify(struct update *u)
{
	struct srel *rl;
	int j, k;

	assert(u != NULL);

	CHECK(u->tbl_name != NULL);
	CHECK(strlen(u->tbl_name) <= RL_NAME_MAX);
	rl = open_relation(u->tbl_name);
	CHECK(rl != NULL);

	CHECK(u->cnt > 0);

	for (j = 0; j < u->cnt; j++) {
		CHECK(!strncmp(u->attrs[j]->tbl_name, u->tbl_name,
					RL_NAME_MAX));
		for (k = 0; k < rl->rl_header.hd_atcnt; k++)
			if (!strncmp(rl->rl_header.hd_attrs[k].at_name,
						u->attrs[j]->attr_name, 
						AT_NAME_MAX))
				break;
		CHECK(rl->rl_header.hd_attrs[k].at_domain
				== u->values[j]->domain);
	}

	if (u->expr_tree != NULL)
		CHECK(expr_tree_verify_srel(u->expr_tree, rl));
	return true;
}

bool dml_modi_verify(struct dml_modi *ptr)
{
	assert(ptr != NULL);

	switch (ptr->type) {
		case INSERTION:
			CHECK(insertion_verify(ptr->ptr.insertion));
			break;
		case DELETION:
			CHECK(deletion_verify(ptr->ptr.deletion));
			break;
		case UPDATE:
			CHECK(update_verify(ptr->ptr.update));
			break;
	}
	return true;
}

