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
 * Bison-source-file for query language parser.
 */

%{
#include "arraylist.h"
#include "db.h"
#include "ddl.h"
#include "dml.h"
#include "constants.h"
#include "err.h"
#include "expr.h"
#include "mem.h"
#include "sort.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NEW(s)		struct s *s = gmalloc(sizeof(struct s), id);\
			memset(s, 0, sizeof(struct s))

/* XXX The definition of `id', `statement_result' and the global variables
 * `currrent', `ids' and `statement_results' are part of a workaround.
 * It works around the fact that stored procedures are likely to execute
 * database statements. This results in nested calls to dql_parse() (one for
 * the SP and one for the query executed by the SP).
 * The best and also thread-safe way would be if 
 * (1) ql_scan_string()	could take custom arguments (the memory id!)
 * (2) qlparse() 	could return the stmt_result instead of setting it.
 */

#define id			(ids[current])
#define statement_result	(statement_results[current])

#define qlerror(msg)		(ERR(E_SYNTAX_ERROR), _qlerror(msg))

int current = -1;
mid_t ids[3] = {-1, -1, -1 };
struct stmt_result *statement_results[3] = { NULL, NULL, NULL };

struct stmt_result *dql_parse(const char *stmt);
void dql_cleanup(void);
static void _qlerror(const char *msg);

extern struct qlbuffer_state *ql_scan_string(const char *str);
extern int qllex(void);
extern void dql_scanner_cleanup(void);

%}

%union {
	db_float_t		float_val;
	db_double_t		double_val;
	db_int_t		int_val;
	db_uint_t		uint_val;
	db_long_t		long_val;
	db_ulong_t		ulong_val;
	char			*string_ptr;
	struct expr		*expr;
	struct attr		*attr;
	struct value		*value;
	struct alist		*list;

	struct ddl_stmt		*ddl_stmt;
	struct type		*type;
	struct attr_dcl		*attr_dcl;
	struct crt_tbl		*crt_tbl;
	struct drp_tbl		*drp_tbl;
	struct crt_view		*crt_view;
	struct drp_view		*drp_view;
	struct crt_ix		*crt_ix;
	struct drp_ix		*drp_ix;

	struct dml_query	*dml_query;
	struct srcrl		*srcrl;
	struct selection	*selection;
	struct projection	*projection;
	struct runion		*runion;
	struct join		*join;
	struct sort		*sort;

	struct dml_sp		*dml_sp;

	struct dml_modi		*dml_modi;
	struct insertion	*insertion;
	struct deletion		*deletion;
	struct update		*update;

	struct stmt_result	*stmt_result;
}

%token <float_val> TOK_FLOAT
%token <double_val> TOK_DOUBLE
%token <int_val> TOK_INT
%token <uint_val> TOK_UINT
%token <long_val> TOK_LONG
%token <ulong_val> TOK_ULONG
%token <string_ptr> TOK_STRING TOK_BYTES TOK_SYMBOL

%token TOK_TYPE_INT TOK_TYPE_UINT TOK_TYPE_LONG TOK_TYPE_ULONG
%token TOK_TYPE_FLOAT TOK_TYPE_DOUBLE
%token TOK_TYPE_STRING TOK_TYPE_BYTES
%token TOK_CREATE TOK_DROP
%token TOK_TABLE TOK_INDEX TOK_VIEW
%token TOK_SELECT TOK_PROJECT TOK_UPDATE TOK_UNION TOK_DELETE TOK_INSERT
%token TOK_JOIN TOK_SORT
%token TOK_WILDCARD TOK_FROM TOK_WHERE TOK_AS TOK_ON TOK_OVER TOK_BY TOK_ASC
%token TOK_DESC TOK_SET
%token TOK_VALUES TOK_INTO
%token TOK_PRIMARY_KEY TOK_FOREIGN_KEY
%token TOK_AND TOK_OR
%token TOK_EQ TOK_LEQ TOK_GEQ TOK_LT TOK_GT TOK_NEQ
%token TOK_EOQ

/* AND has higher precedence than OR, e.g. x AND y OR z equals (x AND y) or z.
 * This correspondents to the boolean interpretation AND ^= multiplication,
 * OR ^= addition. */
%left TOK_OR
%left TOK_AND

%type <string_ptr> tbl_name view_name ix_name attr_name
%type <int_val> field_size
%type <type> type
%type <expr> expr
%type <int_val> comp
%type <attr> attr
%type <value> value

%type <ddl_stmt> ddl_stmt
%type <attr_dcl> attr_dcl
%type <list> attr_dcllist
%type <crt_tbl> crt_tbl
%type <drp_tbl> drp_tbl
%type <crt_view> crt_view
%type <drp_view> drp_view
%type <crt_ix> crt_ix
%type <drp_ix> drp_ix

%type <dml_query> dml_query
%type <srcrl> srcrl
%type <expr> selection_where
%type <selection> selection
%type <list> attrlist
%type <list> projection_over
%type <projection> projection
%type <runion> runion
%type <expr> join_on
%type <join> join
%type <sort> sort
%type <list> order_by
%type <int_val> order

%type <dml_sp> dml_sp

%type <dml_modi> dml_modi
%type <list> valuelist
%type <insertion> insertion
%type <expr> deletion_where
%type <deletion> deletion
%type <list> attrvaluelist
%type <expr> update_where
%type <update> update

%type <stmt_result> stmt
%start stmt

%%

stmt : ddl_stmt TOK_EOQ
	{
		NEW(stmt_result);
		stmt_result->type = DDL_STMT;
		stmt_result->success = ddl_exec($1);
		$$ = stmt_result;
		statement_result = stmt_result;
	}
	| dml_modi TOK_EOQ
	{
		tpcnt_t cnt;
		NEW(stmt_result);
		stmt_result->type = DML_MODI;
		stmt_result->success = dml_modi($1, &cnt);
		stmt_result->val.aftpcnt = cnt;
		$$ = stmt_result;
		statement_result = stmt_result;
	}
	| dml_sp TOK_EOQ
	{
		NEW(stmt_result);
		stmt_result->type = DML_SP;
		stmt_result->success = dml_sp($1, &stmt_result->val.spval);
		$$ = stmt_result;
		statement_result = stmt_result;
	}
	| dml_query TOK_EOQ
	{
		struct xrel *rl;
		NEW(stmt_result);
		stmt_result->type = DML_QUERY;
		rl = dml_query($1);
		stmt_result->success = (rl != NULL);
		stmt_result->val.rl = rl;
		$$ = stmt_result;
		statement_result = stmt_result;
	}
	;

ddl_stmt : crt_tbl
	{
		NEW(ddl_stmt);
		ddl_stmt->type = CREATE_TABLE;
		ddl_stmt->ptr.crt_tbl = $1;
		$$ = ddl_stmt;
	} 
	| drp_tbl
	{
		NEW(ddl_stmt);
		ddl_stmt->type = DROP_TABLE;
		ddl_stmt->ptr.drp_tbl = $1;
		$$ = ddl_stmt;
	}
	| crt_view
	{
		NEW(ddl_stmt);
		ddl_stmt->type = CREATE_VIEW;
		ddl_stmt->ptr.crt_view = $1;
		$$ = ddl_stmt;
	}
	| drp_view
	{
		NEW(ddl_stmt);
		ddl_stmt->type = DROP_VIEW;
		ddl_stmt->ptr.drp_view = $1;
		$$ = ddl_stmt;
	}
	| crt_ix
	{
		NEW(ddl_stmt);
		ddl_stmt->type = CREATE_INDEX;
		ddl_stmt->ptr.crt_ix = $1;
		$$ = ddl_stmt;
	}
	| drp_ix
	{
		NEW(ddl_stmt);
		ddl_stmt->type = DROP_INDEX;
		ddl_stmt->ptr.drp_ix = $1;
		$$ = ddl_stmt;
	}
	;

field_size : TOK_INT
	{
		$$ = $1;
	}
	;

type : TOK_TYPE_BYTES '(' field_size ')'
	{
		NEW(type);
		type->domain = BYTES;
		type->size = $3;
		$$ = type;
	}
	| TOK_TYPE_STRING '(' field_size ')'
	{
		NEW(type);
		type->domain = STRING;
		type->size = $3;
		$$ = type;
	}
	| TOK_TYPE_INT
	{
		NEW(type);
		type->domain = INT;
		type->size = sizeof(db_int_t);
		$$ = type;
	}
	| TOK_TYPE_UINT
	{
		NEW(type);
		type->domain = UINT;
		type->size = sizeof(db_uint_t);
		$$ = type;
	}
	| TOK_TYPE_LONG
	{
		NEW(type);
		type->domain = LONG;
		type->size = sizeof(db_long_t);
		$$ = type;
	}
	| TOK_TYPE_ULONG
	{
		NEW(type);
		type->domain = ULONG;
		type->size = sizeof(db_ulong_t);
		$$ = type;
	}
	| TOK_TYPE_FLOAT
	{
		NEW(type);
		type->domain = FLOAT;
		type->size = sizeof(db_float_t);
		$$ = type;
	}
	| TOK_TYPE_DOUBLE
	{
		NEW(type);
		type->domain = DOUBLE;
		type->size = sizeof(db_double_t);
		$$ = type;
	}
	;

tbl_name : TOK_SYMBOL
	{
		$$ = $1;
	}
	| '(' tbl_name ')'
	{
		$$ = $2;
	}
	;

view_name : '$' TOK_SYMBOL
	{
		$$ = $2;
	}
	;

ix_name : TOK_SYMBOL
	{
		$$ = $1;
	}
	;

attr_name : TOK_SYMBOL
	{
		$$ = $1;
	}
	;

attr : tbl_name '.' attr_name
	{
		NEW(attr);
		attr->tbl_name = $1;
		attr->attr_name = $3;
		$$ = attr;
	}
	/*| attr_name
	{
		NEW(attr);
		attr->tbl_name = NULL;
		attr->attr_name = $1;
		$$ = attr;
	}*/
	;

value : TOK_STRING
	{
		NEW(value);
		value->domain = STRING;
		value->ptr.pstring = $1;
		$$ = value;
	}
	| TOK_BYTES
	{
		NEW(value);
		value->domain = BYTES;
		value->ptr.pstring = $1;
		$$ = value;
	}
	| TOK_FLOAT
	{
		NEW(value);
		value->domain = FLOAT;
		value->ptr.vfloat = $1;
		$$ = value;
	}
	| TOK_DOUBLE
	{
		NEW(value);
		value->domain = DOUBLE;
		value->ptr.vdouble = $1;
		$$ = value;
	}
	| TOK_INT
	{
		NEW(value);
		value->domain = INT;
		value->ptr.vint = $1;
		$$ = value;
	}
	| TOK_UINT
	{
		NEW(value);
		value->domain = UINT;
		value->ptr.vuint = $1;
		$$ = value;
	}
	| TOK_LONG
	{
		NEW(value);
		value->domain = LONG;
		value->ptr.vlong = $1;
		$$ = value;
	}
	| TOK_ULONG
	{
		NEW(value);
		value->domain = ULONG;
		value->ptr.vulong = $1;
		$$ = value;
	}
	;

attr_dcl : attr_name type
	{
		NEW(attr_dcl);
		attr_dcl->attr_name = $1;
		attr_dcl->type = $2;
		attr_dcl->primary_index = false;
		attr_dcl->fk_tbl_name = NULL;
		attr_dcl->fk_attr_name = NULL;
		$$ = attr_dcl;
	}
	| attr_name type TOK_PRIMARY_KEY
	{
		NEW(attr_dcl);
		attr_dcl->attr_name = $1;
		attr_dcl->type = $2;
		attr_dcl->primary_index = true;
		attr_dcl->fk_tbl_name = NULL;
		attr_dcl->fk_attr_name = NULL;
		$$ = attr_dcl;
	}
	| attr_name type TOK_FOREIGN_KEY '(' tbl_name ',' attr_name ')'
	{
		NEW(attr_dcl);
		attr_dcl->attr_name = $1;
		attr_dcl->type = $2;
		attr_dcl->primary_index = false;
		attr_dcl->fk_tbl_name = $5;
		attr_dcl->fk_attr_name = $7;
		$$ = attr_dcl;
	}
	;

attr_dcllist : attr_dcllist ',' attr_dcl
	{
		al_append($1, $3);
		$$ = $1;
	}
	| attr_dcl
	{
		struct alist *list = al_init_gc(10, id);
		al_append(list, $1);
		$$ = list;
	}
	;

crt_tbl : TOK_CREATE TOK_TABLE tbl_name '(' attr_dcllist ')'
	{
		NEW(crt_tbl);
		crt_tbl->tbl_name = $3;
		crt_tbl->attr_dcls = (struct attr_dcl **)$5->table;
		crt_tbl->cnt = $5->used;
		gfree($5, id);
		$$ = crt_tbl;
	}
	;

drp_tbl : TOK_DROP TOK_TABLE tbl_name
	{
		NEW(drp_tbl);
		drp_tbl->tbl_name = $3;
		$$ = drp_tbl;
	}
	;

crt_view : TOK_CREATE TOK_VIEW view_name TOK_AS dml_query
	{
		NEW(crt_view);
		crt_view->view_name = $3;
		crt_view->query = $5;
		$$ = crt_view;
	}
	;

drp_view : TOK_DROP TOK_VIEW view_name
	{
		NEW(drp_view);
		drp_view->view_name = $3;
		$$ = drp_view;
	}
	;

crt_ix : TOK_CREATE TOK_INDEX TOK_ON tbl_name '(' attr_name ')'
	{
		NEW(crt_ix);
		crt_ix->tbl_name = $4;
		crt_ix->attr_name = $6;
		$$ = crt_ix;
	}
	;

drp_ix : TOK_DROP TOK_INDEX ix_name '(' attr_name ')'
	{
		NEW(drp_ix);
		drp_ix->tbl_name = $3;
		drp_ix->attr_name = $5;
		$$ = drp_ix;
	}
	;

dml_query : selection
	{
		NEW(dml_query);
		dml_query->type = SELECTION;
		dml_query->ptr.selection = $1;
		$$ = dml_query;
	}
	| projection
	{
		NEW(dml_query);
		dml_query->type = PROJECTION;
		dml_query->ptr.projection = $1;
		$$ = dml_query;
	}
	| runion
	{
		NEW(dml_query);
		dml_query->type = UNION;
		dml_query->ptr.runion = $1;
		$$ = dml_query;
	}
	| join
	{
		NEW(dml_query);
		dml_query->type = JOIN;
		dml_query->ptr.join = $1;
		$$ = dml_query;
	}
	| sort
	{
		NEW(dml_query);
		dml_query->type = SORT;
		dml_query->ptr.sort = $1;
		$$ = dml_query;
	}
	;

srcrl : '(' srcrl ')'
	{
		$$ = $2;
	}
	| tbl_name
	{
		NEW(srcrl);
		srcrl->type = SRC_TABLE;
		srcrl->ptr.tbl_name = $1;
		$$ = srcrl;
	}
	| view_name
	{
		NEW(srcrl);
		srcrl->type = SRC_VIEW;
		srcrl->ptr.view_name = $1;
		$$ = srcrl;
	}
	| dml_query
	{
		NEW(srcrl);
		srcrl->type = SRC_QUERY;
		srcrl->ptr.dml_query = $1;
		$$ = srcrl;
	}
	;

selection_where : /* nothing */
	{
		$$ = NULL;
	}
	| TOK_WHERE expr
	{
		$$ = $2;
	}
	;

selection : TOK_SELECT TOK_FROM srcrl selection_where
	{
		NEW(selection);
		selection->parent.type = $3->type;
		switch (selection->parent.type) {
			case SRC_TABLE:
				selection->parent.ptr.tbl_name
					= $3->ptr.tbl_name;
				break;
			case SRC_VIEW:
				selection->parent.ptr.view_name
					= $3->ptr.view_name;
				break;
			case SRC_QUERY:
				selection->parent.ptr.dml_query
					= $3->ptr.dml_query;
				break;
		}
		selection->expr_tree = $4;
		$$ = selection;
	}
	;

attrlist : attrlist ',' attr
	{
		al_append($1, $3);
		$$ = $1;
	}
	| attr
	{
		$$ = al_init_gc(10, id);
		al_append($$, $1);
	}
	;

projection_over : TOK_OVER attrlist
	{
		$$ = $2;
	}
	;

projection : TOK_PROJECT srcrl projection_over
	{
		NEW(projection);
		projection->parent.type = $2->type;
		switch (projection->parent.type) {
			case SRC_TABLE:
				projection->parent.ptr.tbl_name
					= $2->ptr.tbl_name;
				break;
			case SRC_VIEW:
				projection->parent.ptr.view_name
					= $2->ptr.view_name;
				break;
			case SRC_QUERY:
				projection->parent.ptr.dml_query
					= $2->ptr.dml_query;
				break;
		}
		projection->attrs = (struct attr **)$3->table;
		projection->atcnt = $3->used;
		$$ = projection;
	}
	;

runion : TOK_UNION srcrl ',' srcrl
	{
		NEW(runion);
		runion->parents[0].type = $2->type;
		switch (runion->parents[0].type) {
			case SRC_TABLE:
				runion->parents[0].ptr.tbl_name
					= $2->ptr.tbl_name;
				break;
			case SRC_VIEW:
				runion->parents[0].ptr.view_name
					= $2->ptr.view_name;
				break;
			case SRC_QUERY:
				runion->parents[0].ptr.dml_query
					= $2->ptr.dml_query;
				break;
		}

		runion->parents[1].type = $4->type;
		switch (runion->parents[1].type) {
			case SRC_TABLE:
				runion->parents[1].ptr.tbl_name
					= $4->ptr.tbl_name;
				break;
			case SRC_VIEW:
				runion->parents[1].ptr.view_name
					= $4->ptr.view_name;
				break;
			case SRC_QUERY:
				runion->parents[1].ptr.dml_query
					= $4->ptr.dml_query;
				break;
		}
		$$ = runion;
	}
	;

join_on : /* nothing */
	{
		$$ = NULL;
	}
	| TOK_ON expr
	{
		$$ = $2;
	}
	;

join : TOK_JOIN srcrl ',' srcrl join_on
	{
		NEW(join);
		join->parents[0].type = $2->type;
		switch (join->parents[0].type) {
			case SRC_TABLE:
				join->parents[0].ptr.tbl_name
					= $2->ptr.tbl_name;
				break;
			case SRC_VIEW:
				join->parents[0].ptr.view_name
					= $2->ptr.view_name;
				break;
			case SRC_QUERY:
				join->parents[0].ptr.dml_query
					= $2->ptr.dml_query;
				break;
		}

		join->parents[1].type = $4->type;
		switch (join->parents[1].type) {
			case SRC_TABLE:
				join->parents[1].ptr.tbl_name
					= $4->ptr.tbl_name;
				break;
			case SRC_VIEW:
				join->parents[1].ptr.view_name
					= $4->ptr.view_name;
				break;
			case SRC_QUERY:
				join->parents[1].ptr.dml_query
					= $4->ptr.dml_query;
				break;
		}

		join->expr_tree = $5;
		$$ = join;
	}
	;

order : TOK_ASC
	{
		$$ = ASCENDING;
	}
	| TOK_DESC
	{
		$$ = DESCENDING;
	}
	| /* nothing */
	{
		$$ = ASCENDING;
	}
	;

order_by : order_by ',' attr order
	{
		al_append($1, $3);
		al_append($1, (void *)$4);
		$$ = $1;
	}
	| attr order
	{
		$$ = al_init_gc(10, id);
		al_append($$, $1);
		al_append($$, (void *)$2);
	}
	;

sort : TOK_SORT srcrl TOK_BY order_by
	{
		int i;
		NEW(sort);

		sort->parent = *$2;
		sort->atcnt = $4->used / 2;
		sort->attrs = gmalloc(sort->atcnt * sizeof(struct attr *), id);
		sort->orders = gmalloc(sort->atcnt * sizeof(int), id);
		for (i = 0; i < sort->atcnt; i++) {
			sort->attrs[i] = (struct attr *)al_get($4, 2*i);
			sort->orders[i] = (int)al_get($4, 2*i+1);
		}
		$$ = sort;
	}
	;

dml_sp : TOK_SYMBOL '(' valuelist ')'
	{
		NEW(dml_sp);
		dml_sp->name = $1;
		dml_sp->argc = $3->used;
		dml_sp->argv = (struct value **)$3->table;
		$$ = dml_sp;
	}

dml_modi : insertion
	{
		NEW(dml_modi);
		dml_modi->type = INSERTION;
		dml_modi->ptr.insertion = $1;
		$$ = dml_modi;
	}
	| deletion
	{
		NEW(dml_modi);
		dml_modi->type = DELETION;
		dml_modi->ptr.deletion = $1;
		$$ = dml_modi;
	}
	| update
	{
		NEW(dml_modi);
		dml_modi->type = UPDATE;
		dml_modi->ptr.update = $1;
		$$ = dml_modi;
	}
	;

valuelist : valuelist ',' value
	{
		al_append($1, $3);
		$$ = $1;
	}
	| value
	{
		$$ = al_init_gc(10, id);
		al_append($$, $1);
	}
	;

insertion : TOK_INSERT TOK_INTO tbl_name '(' attrlist ')'
	  	TOK_VALUES '(' valuelist ')'
	{
		NEW(insertion);
		insertion->tbl_name = $3;
		insertion->atcnt = $5->used;
		insertion->attrs = (struct attr **)$5->table;
		insertion->valcnt = $9->used;
		insertion->values = (struct value **)$9->table;
		$$ = insertion;
	}
	;

deletion_where : /* nothing */
	{
		$$ = NULL;
	}
	| TOK_WHERE expr
	{
		$$ = $2;
	}
	;

deletion : TOK_DELETE tbl_name deletion_where
	{
		NEW(deletion);
		deletion->tbl_name = $2;
		deletion->expr_tree = $3;
		$$ = deletion;
	}
	;

attrvaluelist : attrvaluelist ',' attr TOK_EQ value
	{
		al_append($1, $3);
		al_append($1, $5);
		$$ = $1;
	}
	| attr TOK_EQ value
	{
		$$ = al_init_gc(10, id);
		al_append($$, $1);
		al_append($$, $3);
	}
	;

update_where : /* nothing */
	{
		$$ = NULL;
	}
	| TOK_WHERE expr
	{
		$$ = $2;
	}
	;

update : TOK_UPDATE tbl_name TOK_SET attrvaluelist update_where
	{
		int i;
		NEW(update);
		update->tbl_name = $2;
		update->cnt = $4->used / 2;
		update->attrs = gmalloc(update->cnt * sizeof(struct attr *),
		    id);
		update->values = gmalloc(update->cnt * sizeof(struct value *),
		    id);
		for (i = 0; i < update->cnt; i++) {
			update->attrs[i] = al_get($4, 2*i);
			update->values[i] = al_get($4, 2*i + 1);
		}
		update->expr_tree = $5;
		$$ = update;
	}
	;

expr	: '(' expr ')'
	{
		$$ = $2;
	}
	| expr TOK_AND expr
	{
		NEW(expr);
		expr->type = INNER;
		expr->op = AND;
		expr->stype[0] = SON_EXPR;
		expr->sons[0].expr = $1;
		expr->stype[1] = SON_EXPR;
		expr->sons[1].expr = $3;
		$$ = expr;
	}
	| expr TOK_OR expr
	{
		NEW(expr);
		expr->type = INNER;
		expr->op = OR;
		expr->stype[0] = SON_EXPR;
		expr->sons[0].expr = $1;
		expr->stype[1] = SON_EXPR;
		expr->sons[1].expr = $3;
		$$ = expr;
	}
	| attr comp value
	{
		NEW(expr);
		expr->type = LEAF;
		expr->op = $2;
		expr->stype[0] = SON_ATTR;
		expr->sons[0].attr = $1;
		expr->stype[1] = SON_VALUE;
		expr->sons[1].value = $3;
		$$ = expr;
	}
	| attr comp attr
	{
		NEW(expr);
		expr->type = LEAF;
		expr->op = $2;
		expr->stype[0] = SON_ATTR;
		expr->sons[0].attr = $1;
		expr->stype[1] = SON_ATTR;
		expr->sons[1].attr = $3;
		$$ = expr;
	}
	;

comp	: TOK_EQ 
     	{
		$$ = EQ;
	}
   	| TOK_GEQ
    	{
		$$ = GEQ;
	}
	| TOK_LEQ
     	{
		$$ = LEQ;
	}
	| TOK_LT
     	{
		$$ = LT;
	}
	| TOK_GT
     	{
		$$ = GT;
	}
	| TOK_NEQ
     	{
		$$ = NEQ;
	}
	;

%%

void _dql_cleanup(void)
{
	if (id != -1) {
		dql_scanner_cleanup();
		gc(id);
		id = -1;
	}
}

void dql_cleanup(void)
{
	int i;

	for (i = 0; i < 3; i++) {
		current = i;
		_dql_cleanup();
	}
}

struct stmt_result *dql_parse(const char *stmt)
{
	struct stmt_result *r;

	current++;
	assert(current < 3);

	_dql_cleanup();
	id = gnew();
	statement_result = NULL;
	ql_scan_string(stmt);
	qlparse();
	r = statement_result;
	current--;
	return r;
}

static void _qlerror(const char *msg)
{
	if (statement_result != NULL)
		statement_result->success = false;
}

