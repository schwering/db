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
 * Implementation for Data Manipulation Language.
 */

#ifndef __DML_H__
#define __DML_H__

#include "block.h"
#include "constants.h"
#include "expr.h"
#include <stdbool.h>

struct attr {
	char *tbl_name;
	char *attr_name;
};

struct value {
	enum domain domain;
	union {
		char *pstring;
		char *pbytes;
		db_int_t vint;
		db_uint_t vuint;
		db_long_t vlong;
		db_ulong_t vulong;
		db_float_t vfloat;
		db_double_t vdouble;
	} ptr;
};

#ifndef _S_STMT_RESULT_
#define _S_STMT_RESULT_
/* common in dml.h and ddl.h; change simultaneously if you do */
struct stmt_result {
	enum stmt_type {
		STMT_ERROR=-1,
		DDL_STMT,
		DML_QUERY,
		DML_MODI,
		DML_SP,
	} type;
	bool success;
	union {
		struct xrel *rl;
		struct value spval;
		tpcnt_t aftpcnt;
	} val;
};
#endif

/* Query Part */

struct dml_query {
	enum query_type {
		SELECTION,
		PROJECTION,
		UNION,
		JOIN,
		SORT
	} type;
	union {
		struct selection *selection;
		struct projection *projection;
		struct runion *runion;
		struct join *join;
		struct sort *sort;
	} ptr;
};

struct srcrl {
	enum src_type {
		SRC_TABLE,
		SRC_VIEW,
		SRC_QUERY
	} type;
	union {
		char *tbl_name;
		char *view_name;
		struct dml_query *dml_query;
	} ptr;
};

struct selection {
	struct srcrl parent;
	struct expr *expr_tree;
};

struct projection {
	struct srcrl parent;
	struct attr **attrs;
	int atcnt;
};

struct runion {
	struct srcrl parents[2];
};

struct join {
	struct srcrl parents[2];
	struct expr *expr_tree;
};

struct sort {
	struct srcrl parent;
	struct attr **attrs;
	int *orders;
	int atcnt;
};

/* Data Manipulation Language (Stored Procedures) */

struct dml_sp {
	char *name;
	struct value **argv;
	int argc;
};

/* Data Manipulation Language (Modification Part) */

struct dml_modi {
	enum modi_type {
		INSERTION,
		DELETION,
		UPDATE
	} type;
	union {
		struct insertion *insertion;
		struct deletion *deletion;
		struct update *update;
	} ptr;
};

struct insertion {
	char *tbl_name;
	struct attr **attrs;
	int atcnt;
	struct value **values;
	int valcnt;
};

struct deletion {
	char *tbl_name;
	struct expr *expr_tree;
};

struct update {
	char *tbl_name;
	struct attr **attrs;
	struct value **values;
	int cnt;
	struct expr *expr_tree;
};

/* The query family of DML commands consists of selection, projection,
 * union and join commands. */
struct xrel *dml_query(struct dml_query *query);
struct xrel *dml_select(struct selection *selection);
struct xrel *dml_project(struct projection *projection);
struct xrel *dml_union(struct runion *runion);
struct xrel *dml_join(struct join *join);
struct xrel *dml_sort(struct sort *sort);

/* Stored Procedurs. */
bool dml_sp(struct dml_sp *sp, struct value *result);

/* The modi family of DML (modification) commands consists of insertion,
 * update and deletion. The `cnt_ptr' pointer can point to an tpcnt_t in which
 * the count of affected tuples is stored. The pointer can be NULL. If
 * the `modi' is a insertion in dml_modi() and the insertion is successful,
 * `cnt_ptr' is set to 1. */
bool dml_modi(struct dml_modi *modi, tpcnt_t *cnt_ptr);
bool dml_insert(struct insertion *insertion);
bool dml_delete(struct deletion *deletion, tpcnt_t *cnt_ptr);
bool dml_update(struct update *update, tpcnt_t *cnt_ptr);

#endif

