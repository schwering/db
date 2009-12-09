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
 * Implementation for Data Definition Language.
 */

#ifndef __DDL_H__
#define __DDL_H__

#include "dml.h"
#include <stdbool.h>
#include <stddef.h>

#ifndef _S_STMT_RESULT_
#define _S_STMT_RESULT_
/* common in dml.h and ddl.h; change simultaneously if you do */
struct stmt_result {
	enum stmt_type {
		STMT_ERROR=-1,
		DDL_STMT,
		DML_QUERY,
		DML_MODI,
		DML_AGGFN
	} type;
	bool success;
	union {
		struct xrel *rl;
		long double aggval;
		tpcnt_t aftpcnt;
	} val;
};
#endif

enum ddl_type {
	CREATE_TABLE,
	DROP_TABLE,
	CREATE_VIEW,
	DROP_VIEW,
	CREATE_INDEX,
	DROP_INDEX
};

struct ddl_stmt {
	enum ddl_type type;
	union {
		struct crt_tbl *crt_tbl;
		struct drp_tbl *drp_tbl;
		struct crt_view *crt_view;
		struct drp_view *drp_view;
		struct crt_ix *crt_ix;
		struct drp_ix *drp_ix;
	} ptr;
};

struct type {
	enum domain domain;
	size_t size;
};

struct attr_dcl {
	char *attr_name;
	struct type *type;
	bool primary_index;
	char *fk_tbl_name;
	char *fk_attr_name;
};

struct crt_tbl {
	char *tbl_name;
	struct attr_dcl **attr_dcls;
	int cnt;
};

struct drp_tbl {
	char *tbl_name;
};

struct crt_view {
	char *view_name;
	struct dml_query *query;
};

struct drp_view {
	char *view_name;
};

struct crt_ix {
	char *tbl_name;
	char *attr_name;
};

struct drp_ix {
	char *tbl_name;
	char *attr_name;
};

bool ddl_exec(struct ddl_stmt *stmt);
bool ddl_create_table(struct crt_tbl *crt_tbl);
bool ddl_drop_table(struct drp_tbl *drp_tbl);
bool ddl_create_view(struct crt_view *crt_view);
bool ddl_drop_view(struct drp_view *drp_view);
bool ddl_create_index(struct crt_ix *crt_ix);
bool ddl_drop_index(struct drp_ix *drp_ix);

void ddl_stmt_free(struct ddl_stmt *ptr);
void crt_tbl_free(struct crt_tbl *ptr);
void drp_tbl_free(struct drp_tbl *ptr);
void crt_view_free(struct crt_view *ptr);
void drp_view_free(struct drp_view *ptr);
void crt_ix_free(struct crt_ix *ptr);
void drp_ix_free(struct drp_ix *ptr);

#endif

