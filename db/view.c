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

#include "view.h"
#include "constants.h"
#include "dml.h"
#include "mem.h"
#include "str.h"
#include "hashtable.h"
#include <assert.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct view_wrapper {
	struct dml_query *view;
	mid_t id;
};

static struct hashtable *table = NULL;

static void init_table(void)
{
	if (table == NULL) {
		table = table_init(7, (int (*)(void *))strhash,
				(bool (*)(void *, void *))strequals);
		assert(table != NULL);
	}
}

static void free_table(void)
{
	if (table != NULL) {
		table_free(table);
		table = NULL;
	}
}

static int read_int(int fd)
{
	int i;
	size_t size;

	size = read(fd, &i, sizeof(int));
	assert(size == sizeof(int));
	return i;
}

static void write_int(int fd, int i)
{
	size_t size;

	size = write(fd, &i, sizeof(int));
	assert(size == sizeof(int));
}

static char *read_string(int fd, mid_t id)
{
	char *s;
	size_t len, size;

	size = read(fd, &len, sizeof(int));
	assert(size == sizeof(int));
	if (len == 0) {
		s = NULL;
	} else {
		s = gmalloc(len+1, id);
		size = read(fd, s, len+1);
		assert(size == len+1);
	}
	return s;
}

static void write_string(int fd, char *s)
{
	size_t len, size;

	if (s == NULL) {
		len = 0;
		size = write(fd, &len, sizeof(int));
		assert(size == sizeof(int));
	} else {
		len = strlen(s);
		size = write(fd, &len, sizeof(int));
		assert(size == sizeof(int));
		size = write(fd, s, len+1);
		assert(size == len+1);
	}
}

static void dml_query_write(int fd, struct dml_query *q);

static void srcrl_content_write(int fd, struct srcrl *s)
{
	assert(s != NULL);

	write_int(fd, s->type);
	switch (s->type) {
		case SRC_TABLE:
			write_string(fd, s->ptr.tbl_name);
			break;
		case SRC_VIEW:
			write_string(fd, s->ptr.view_name);
			break;
		case SRC_QUERY:
			dml_query_write(fd, s->ptr.dml_query);
			break;
	}
}

static void attr_write(int fd, struct attr *a)
{
	assert(a != NULL);

	write_string(fd, a->tbl_name);
	write_string(fd, a->attr_name);
}

static void value_write(int fd, struct value *v)
{
	size_t size;

	assert(v != NULL);

	write_int(fd, v->domain);
	switch (v->domain) {
		case INT:
			size = write(fd, &v->ptr.vint, sizeof(db_int_t));
			assert(size == sizeof(db_int_t));
			break;
		case UINT:
			size = write(fd, &v->ptr.vuint, sizeof(db_uint_t));
			assert(size == sizeof(db_uint_t));
			break;
		case LONG:
			size = write(fd, &v->ptr.vlong, sizeof(db_long_t));
			assert(size == sizeof(db_long_t));
			break;
		case ULONG:
			size = write(fd, &v->ptr.vulong, sizeof(db_ulong_t));
			assert(size == sizeof(db_ulong_t));
			break;
		case FLOAT:
			size = write(fd, &v->ptr.vfloat, sizeof(db_float_t));
			assert(size == sizeof(db_float_t));
			break;
		case DOUBLE:
			size = write(fd, &v->ptr.vdouble, sizeof(db_double_t));
			assert(size == sizeof(db_double_t));
			break;
		case STRING:
			write_string(fd, v->ptr.pstring);
			break;
		case BYTES:
			/* TODO what about bytes? */
			assert(false);
	}
}

static void expr_write(int fd, struct expr *e)
{
	int i;

	assert(e != NULL);

	write_int(fd, e->type);
	write_int(fd, e->op);
	for (i = 0; i < 2; i++) {
		write_int(fd, e->stype[i]);
		switch (e->stype[i]) {
			case SON_EXPR:
				expr_write(fd, e->sons[i].expr);
				break;
			case SON_ATTR:
				attr_write(fd, e->sons[i].attr);
				break;
			case SON_SATTR:
				assert(false);
				break;
			case SON_VALUE:
				value_write(fd, e->sons[i].value);
				break;
		}
	}
}

static void selection_write(int fd, struct selection *s)
{
	assert(s != NULL);

	srcrl_content_write(fd, &s->parent);
	if (s->expr_tree == NULL)
		write_int(fd, 0);
	else {
		write_int(fd, 1);
		expr_write(fd, s->expr_tree);
	}
}

static void projection_write(int fd, struct projection *p)
{
	int i;

	assert(p != NULL);

	srcrl_content_write(fd, &p->parent);
	write_int(fd, p->atcnt);
	for (i = 0; i < p->atcnt; i++)
		attr_write(fd, p->attrs[i]);
}

static void runion_write(int fd, struct runion *u)
{
	assert(u != NULL);

	srcrl_content_write(fd, &u->parents[0]);
	srcrl_content_write(fd, &u->parents[1]);
}

static void join_write(int fd, struct join *j)
{
	assert(j != NULL);

	srcrl_content_write(fd, &j->parents[0]);
	srcrl_content_write(fd, &j->parents[1]);
	if (j->expr_tree == NULL)
		write_int(fd, 0);
	else {
		write_int(fd, 1);
		expr_write(fd, j->expr_tree);
	}
}

static void sort_write(int fd, struct sort *s)
{
	int i;

	assert(s != NULL);

	srcrl_content_write(fd, &s->parent);
	write_int(fd, s->atcnt);
	for (i = 0; i < s->atcnt; i++) {
		attr_write(fd, s->attrs[i]);
		write_int(fd, s->orders[i]);
	}
}

static void dml_query_write(int fd, struct dml_query *q)
{
	assert(q != NULL);

	write_int(fd, q->type);
	switch (q->type) {
		case SELECTION:
			selection_write(fd, q->ptr.selection);
			break;
		case PROJECTION:
			projection_write(fd, q->ptr.projection);
			break;
		case UNION:
			runion_write(fd, q->ptr.runion);
			break;
		case JOIN:
			join_write(fd, q->ptr.join);
			break;
		case SORT:
			sort_write(fd, q->ptr.sort);
			break;
	}
}

static struct dml_query *dml_query_read(int fd, mid_t id);

static struct srcrl srcrl_content_read(int fd, mid_t id)
{
	struct srcrl s; /* no pointer */

	s.type = read_int(fd);
	switch (s.type) {
		case SRC_TABLE:
			s.ptr.tbl_name = read_string(fd, id);
			break;
		case SRC_VIEW:
			s.ptr.view_name = read_string(fd, id);
			break;
		case SRC_QUERY:
			s.ptr.dml_query = dml_query_read(fd, id);
			break;
	}
	return s;
}

static struct attr *attr_read(int fd, mid_t id)
{
	struct attr *b;

	b = gmalloc(sizeof(struct attr), id);
	b->tbl_name = read_string(fd, id);
	b->attr_name = read_string(fd, id);
	return b;
}

static struct value *value_read(int fd, mid_t id)
{
	struct value *v;
	size_t size;

	v = gmalloc(sizeof(struct value), id);
	v->domain = read_int(fd);
	switch (v->domain) {
		case INT:
			size = read(fd, &v->ptr.vint, sizeof(db_int_t));
			assert(size == sizeof(db_int_t));
			break;
		case UINT:
			size = read(fd, &v->ptr.vuint, sizeof(db_uint_t));
			assert(size == sizeof(db_uint_t));
			break;
		case LONG:
			size = read(fd, &v->ptr.vlong, sizeof(db_long_t));
			assert(size == sizeof(db_long_t));
			break;
		case ULONG:
			size = read(fd, &v->ptr.vulong, sizeof(db_ulong_t));
			assert(size == sizeof(db_ulong_t));
			break;
		case FLOAT:
			size = read(fd, &v->ptr.vfloat, sizeof(db_float_t));
			assert(size == sizeof(db_float_t));
			break;
		case DOUBLE:
			size = read(fd, &v->ptr.vdouble, sizeof(db_double_t));
			assert(size == sizeof(db_double_t));
			break;
		case STRING:
			v->ptr.pstring = read_string(fd, id);
			break;
		case BYTES:
			/* TODO what about bytes? */
			assert(false);
			break;
	}
	return v;
}

static struct expr *expr_read(int fd, mid_t id)
{
	struct expr *e;
	int i;

	e = gmalloc(sizeof(struct expr), id);
	e->type = read_int(fd);
	e->op = read_int(fd);
	for (i = 0; i < 2; i++) {
		e->stype[i] = read_int(fd);
		switch (e->stype[i]) {
			case SON_EXPR:
				e->sons[i].expr = expr_read(fd, id);
				break;
			case SON_ATTR:
				e->sons[i].attr = attr_read(fd, id);
				break;
			case SON_SATTR:
				assert(false);
				break;
			case SON_VALUE:
				e->sons[i].value = value_read(fd, id);
				break;
		}
	}
	return e;
}

static struct selection *selection_read(int fd, mid_t id)
{
	struct selection *s;

	s = gmalloc(sizeof(struct selection), id);
	s->parent = srcrl_content_read(fd, id);
	if (read_int(fd) != 0)
		s->expr_tree = expr_read(fd, id);
	else
		s->expr_tree = NULL;
	return s;
}

static struct projection *projection_read(int fd, mid_t id)
{
	struct projection *p;
	int i;

	p = gmalloc(sizeof(struct projection), id);
	p->parent = srcrl_content_read(fd, id);
	p->atcnt = read_int(fd);
	p->attrs = gmalloc(p->atcnt * sizeof(struct attr), id);
	for (i = 0; i < p->atcnt; i++)
		p->attrs[i] = attr_read(fd, id);
	return p;
}

static struct runion *runion_read(int fd, mid_t id)
{
	struct runion *u;
	
	u = gmalloc(sizeof(struct runion), id);
	u->parents[0] = srcrl_content_read(fd, id);
	u->parents[1] = srcrl_content_read(fd, id);
	return u;
}

static struct join *join_read(int fd, mid_t id)
{
	struct join *j;
	
	j = gmalloc(sizeof(struct join), id);
	j->parents[0] = srcrl_content_read(fd, id);
	j->parents[1] = srcrl_content_read(fd, id);
	if (read_int(fd) != 0)
		j->expr_tree = expr_read(fd, id);
	else
		j->expr_tree = NULL;
	return j;
}

static struct sort *sort_read(int fd, mid_t id)
{
	struct sort *s;
	int i;

	s = gmalloc(sizeof(struct sort), id);
	s->parent = srcrl_content_read(fd, id);
	s->atcnt = read_int(fd);
	s->attrs = gmalloc(s->atcnt * sizeof(struct attr), id);
	s->orders =  gmalloc(s->atcnt * sizeof(int), id);
	for (i = 0; i < s->atcnt; i++) {
		s->attrs[i] = attr_read(fd, id);
		s->orders[i] = read_int(fd);
	}
	return s;
}

static struct dml_query *dml_query_read(int fd, mid_t id)
{
	struct dml_query *q;

	q = gmalloc(sizeof(struct dml_query), id);
	q->type = read_int(fd);
	switch (q->type) {
		case SELECTION:
			q->ptr.selection = selection_read(fd, id);
			break;
		case PROJECTION:
			q->ptr.projection = projection_read(fd, id);
			break;
		case UNION:
			q->ptr.runion = runion_read(fd, id);
			break;
		case JOIN:
			q->ptr.join = join_read(fd, id);
			break;
		case SORT:
			q->ptr.sort = sort_read(fd, id);
			break;
	}
	return q;
}

static struct dml_query *dml_query_copy(struct dml_query *p, mid_t id);

static struct srcrl srcrl_content_copy(struct srcrl *r, mid_t id)
{
	struct srcrl s; /* no pointer */

	assert(r != NULL);

	s.type = r->type;
	switch (s.type) {
		case SRC_TABLE:
			s.ptr.tbl_name = copy_gc(r->ptr.tbl_name,
					strlen(r->ptr.tbl_name)+1, id);
			break;
		case SRC_VIEW:
			s.ptr.view_name = copy_gc(r->ptr.view_name,
					strlen(r->ptr.view_name)+1, id);
			break;
		case SRC_QUERY:
			s.ptr.dml_query = dml_query_copy(r->ptr.dml_query, id);
			break;
	}
	return s;
}

static struct attr *attr_copy(struct attr *a, mid_t id)
{
	struct attr *b;

	assert(a != NULL);

	b = gmalloc(sizeof(struct attr), id);
	b->tbl_name = copy_gc(a->tbl_name, strlen(a->tbl_name)+1, id);
	b->attr_name = copy_gc(a->attr_name, strlen(a->attr_name)+1, id);
	return b;
}

static struct value *value_copy(struct value *w, mid_t id)
{
	struct value *v;

	assert(w != NULL);

	v = gmalloc(sizeof(struct value), id);
	v->domain = w->domain;
	switch (v->domain) {
		case INT:
			v->ptr.vint = w->ptr.vint;
			break;
		case UINT:
			v->ptr.vuint = w->ptr.vuint;
			break;
		case LONG:
			v->ptr.vlong = w->ptr.vlong;
			break;
		case ULONG:
			v->ptr.vulong = w->ptr.vulong;
			break;
		case FLOAT:
			v->ptr.vfloat = w->ptr.vfloat;
			break;
		case DOUBLE:
			v->ptr.vdouble = w->ptr.vdouble;
			break;
		case STRING:
			v->ptr.pstring = copy_gc(w->ptr.pstring,
					strlen(w->ptr.pstring)+1, id);
			break;
		case BYTES:
			/* TODO what about bytes? */
			assert(false);
			break;
	}
	return v;
}

static struct expr *expr_copy(struct expr *d, mid_t id)
{
	struct expr *e;
	int i;

	assert(d != NULL);

	e = gmalloc(sizeof(struct expr), id);
	e->type = d->type;
	e->op = d->op;
	for (i = 0; i < 2; i++) {
		e->stype[i] = d->stype[i];
		switch (e->stype[i]) {
			case SON_EXPR:
				e->sons[i].expr = expr_copy(d->sons[i].expr,
						id);
				break;
			case SON_ATTR:
				e->sons[i].attr = attr_copy(d->sons[i].attr,
						id);
				break;
			case SON_SATTR:
				assert(false);
				break;
			case SON_VALUE:
				e->sons[i].value = value_copy(d->sons[i].value,
						id);
				break;
		}
	}
	return e;
}

static struct selection *selection_copy(struct selection *r, mid_t id)
{
	struct selection *s;

	assert(r != NULL);

	s = gmalloc(sizeof(struct selection), id);
	s->parent = srcrl_content_copy(&r->parent, id);
	if (r->expr_tree != NULL)
		s->expr_tree = expr_copy(r->expr_tree, id);
	else
		s->expr_tree = NULL;
	return s;
}

static struct projection *projection_copy(struct projection *q, mid_t id)
{
	struct projection *p;
	int i;

	assert(q != NULL);

	p = gmalloc(sizeof(struct projection), id);
	p->parent = srcrl_content_copy(&q->parent, id);
	p->atcnt = q->atcnt;
	p->attrs = gmalloc(p->atcnt * sizeof(struct attr), id);
	for (i = 0; i < p->atcnt; i++)
		p->attrs[i] = attr_copy(q->attrs[i], id);
	return p;
}

static struct runion *runion_copy(struct runion *v, mid_t id)
{
	struct runion *u;
	
	assert(v != NULL);

	u = gmalloc(sizeof(struct runion), id);
	u->parents[0] = srcrl_content_copy(&v->parents[0], id);
	u->parents[1] = srcrl_content_copy(&v->parents[1], id);
	return u;
}

static struct join *join_copy(struct join *i, mid_t id)
{
	struct join *j;
	
	assert(i != NULL);

	j = gmalloc(sizeof(struct join), id);
	j->parents[0] = srcrl_content_copy(&i->parents[0], id);
	j->parents[1] = srcrl_content_copy(&i->parents[1], id);
	if (i->expr_tree != NULL)
		j->expr_tree = expr_copy(i->expr_tree, id);
	else
		j->expr_tree = NULL;
	return j;
}

static struct sort *sort_copy(struct sort *s, mid_t id)
{
	struct sort *t;
	int i;
	
	assert(s != NULL);

	t = gmalloc(sizeof(struct sort), id);
	t->parent = srcrl_content_copy(&s->parent, id);
	t->atcnt = s->atcnt;
	t->attrs = gmalloc(t->atcnt * sizeof(struct attr), id);
	t->orders = gmalloc(t->atcnt * sizeof(int), id);
	for (i = 0; i < t->atcnt; i++) {
		t->attrs[i] = attr_copy(s->attrs[i], id);
		t->orders[i] = s->orders[i];
	}
	return t;
}

static struct dml_query *dml_query_copy(struct dml_query *p, mid_t id)
{
	struct dml_query *q;

	assert(p != NULL);

	q = gmalloc(sizeof(struct dml_query), id);
	q->type = p->type;
	switch (q->type) {
		case SELECTION:
			q->ptr.selection = selection_copy(p->ptr.selection, id);
			break;
		case PROJECTION:
			q->ptr.projection = projection_copy(p->ptr.projection,
					id);
			break;
		case UNION:
			q->ptr.runion = runion_copy(p->ptr.runion, id);
			break;
		case JOIN:
			q->ptr.join = join_copy(p->ptr.join, id);
			break;
		case SORT:
			q->ptr.sort = sort_copy(p->ptr.sort, id);
			break;
	}
	return q;
}

bool create_view(char *view_name, struct dml_query *view)
{
	char *fn, *name_cpy;
	struct view_wrapper *wrapper;
	struct dml_query *view_cpy;
	mid_t id;
	int fd;

	assert(view_name != NULL);
	assert(view != NULL);

	if (table == NULL)
		init_table();

	if (table_search(table, view_name) != NULL)
		return false;

	fn = cat(3, VW_BASEDIR, view_name, VW_SUFFIX);
	fd = open(fn, CREATE_FLAGS, FILE_MODE);
	free(fn);
	if (fd == -1)
		return false;

	id = gnew();

	name_cpy = copy_gc(view_name, strlen(view_name)+1, id);
	view_cpy = dml_query_copy(view, id);

	wrapper = gmalloc(sizeof(struct view_wrapper), id);
	wrapper->view = view_cpy;
	wrapper->id = id;

       	table_insert(table, name_cpy, wrapper);
	dml_query_write(fd, wrapper->view);
	close(fd);
	return true;
}

struct dml_query *open_view(char *view_name)
{
	char *fn;
	struct dml_query *view;
	struct view_wrapper *wrapper;
	int fd;
	mid_t id;

	assert(view_name != NULL);

	if (table != NULL && (wrapper = table_search(table, view_name)) != NULL)
		return wrapper->view;

	fn = cat(3, VW_BASEDIR, view_name, VW_SUFFIX);
	fd = open(fn, OPEN_RD_FLAGS, FILE_MODE);
	free(fn);
	if (fd == -1)
		return NULL;

	id = gnew();
	if ((view = dml_query_read(fd, id)) != NULL) {
		char *name_cpy;

		if (table == NULL)
			init_table();
		wrapper = gmalloc(sizeof(struct view_wrapper), id);
		wrapper->view = view;
		wrapper->id = id;
		name_cpy = copy_gc(view_name, strlen(view_name)+1, id);
		table_insert(table, name_cpy, wrapper);
		close(fd);
		return view;
	} else {
		close(fd);
		gc(id);
		return NULL;
	}
}

bool drop_view(char *view_name)
{
	char *fn;
	bool retval;
	struct view_wrapper *wrapper;

	assert(view_name != NULL);

	fn = cat(3, VW_BASEDIR, view_name, VW_SUFFIX);
	retval = unlink(fn) == 0;
	free(fn);

	if (table != NULL
			&& (wrapper = table_delete(table, view_name)) != NULL) {
		gc(wrapper->id);
		if (table->used == 0)
			free_table();
	}

	return retval;
}

