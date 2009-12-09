#include "db.h"
#include "block.h"
#include "constants.h"
#include "ddl.h"
#include "dml.h"
#include "mem.h"
#include "parser.h"
#include "printer.h"
#include "rlalg.h"
#include "rlmngt.h"
#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* stores a character `c' in `cur' which is a pointer to a character of `buf';
 * depending on the current `size' of `buf', `buf' is reallocated and `size'
 * incremented */
#define STORE(c)	{ if ((cur+1 - buf) >= size) {\
				ptrdiff_t diff = cur - buf;\
				char *buf2 = realloc(buf, (size += 10));\
				if (!buf2) free(buf);\
				buf = buf2;\
				cur = buf + diff;\
			}\
			*cur++ = (c); }

/* returns the stmt_result structure of a DB_RESULT */
#define R(r)		((struct stmt_result *)(r).ptr)

/* returns the xrel structure of a DB_ITERATOR */
#define RL(i)		((struct xrel *)(i).rl)
/* returns the xrel_iter structure of a DB_ITERATOR */
#define ITER(i)		((struct xrel_iter *)(i).iter)

/* from parser.y */
extern struct stmt_result *dql_parse(const char *stmt);
extern void dql_cleanup(void);

static void store_long(long n, char **bufp, char **curp, ssize_t *sizep)
{
	char *buf, *cur, tmp[32];
	ssize_t size;
	int i;

	buf = *bufp;
	cur = *curp;
	size = *sizep;

	sprintf(tmp, "%ld", n);
	for (i = 0; tmp[i]; i++)
		STORE(tmp[i]);

	*bufp = buf;
	*curp = cur;
	*sizep = size;
}

static void store_double(double d, char **bufp, char **curp, ssize_t *sizep)
{
	char *buf, *cur, tmp[32];
	ssize_t size;
	int i;

	buf = *bufp;
	cur = *curp;
	size = *sizep;

	sprintf(tmp, "%lf", d);
	for (i = 0; tmp[i]; i++)
		STORE(tmp[i]);

	*bufp = buf;
	*curp = cur;
	*sizep = size;
}

static void store_str(const char *s, char **bufp, char **curp, ssize_t *sizep)
{
	char *buf, *cur;
	ssize_t size;

	buf = *bufp;
	cur = *curp;
	size = *sizep;

	while (*s)
		STORE(*s++);

	*bufp = buf;
	*curp = cur;
	*sizep = size;
}

static void store_estr(const char *s, char **bufp, char **curp, ssize_t *sizep)
{
	char *buf, *cur;
	ssize_t size;

	buf = *bufp;
	cur = *curp;
	size = *sizep;

	for (; *s; s++) {
		if (*s == '\'')
			STORE('\\');
		STORE(*s);
	}

	*bufp = buf;
	*curp = cur;
	*sizep = size;
}

static void store_qestr(const char *s, char **bufp, char **curp, ssize_t *sizep)
{
	char *buf, *cur;
	ssize_t size;

	buf = *bufp;
	cur = *curp;
	size = *sizep;

	STORE('\'');
	store_estr(s, &buf, &cur, &size);
	STORE('\'');

	*bufp = buf;
	*curp = cur;
	*sizep = size;
}

static bool starts_with(const char *str, const char *little)
{
	assert(str != NULL);
	assert(little != NULL);

	while (*little && *str && *little == *str)
		little++, str++;
	return *little == '\0';
}

static char *make_stmt(const char *fmt, va_list ap)
{
	char *buf, *cur;
	ssize_t size;
	int cnt;

	size = 75;
	buf = xmalloc(size);
	cur = buf;
	cnt = 0;
	for (; *fmt; fmt++) {
		if (*fmt != '%') {
			STORE(*fmt);
		} else if (*fmt == '%' && *(fmt+1) == '%') {
			STORE(*fmt);
			fmt++;
		} else {
			int i;
			long l;
			double d;
			const char *s;

			fmt++;
			if (starts_with(fmt, "d")) {
				i = va_arg(ap, int);
				store_long((long)i, &buf, &cur, &size);
				cnt++;
			} else if (starts_with(fmt, "ld")) {
				l = va_arg(ap, long);
				store_long(l, &buf, &cur, &size);
				cnt++;
				fmt++;
			} else if (starts_with(fmt, "f")) {
				d = va_arg(ap, double);
				store_double(d, &buf, &cur, &size);
				cnt++;
			} else if (starts_with(fmt, "lf")) {
				d = va_arg(ap, double);
				store_double(d, &buf, &cur, &size);
				cnt++;
				fmt++;
			} else if (starts_with(fmt, "e")) {
				s = va_arg(ap, const char *);
				store_estr(s, &buf, &cur, &size);
				cnt++;
			} else if (starts_with(fmt, "E")
					|| starts_with(fmt, "S")) {
				s = va_arg(ap, const char *);
				store_qestr(s, &buf, &cur, &size);
				cnt++;
			} else if (starts_with(fmt, "s")) {
				s = va_arg(ap, const char *);
				store_str(s, &buf, &cur, &size);
				cnt++;
			} else {
				STORE(*fmt);
			}
		}
	}
	if ((cur - buf) > 0 && *(cur-1) != ';')
		STORE(';');
	*cur = '\0';
	return buf;
}

DB_RESULT db_parsef(const char *fmt, ...)
{
	char *stmt;
	va_list ap;
	struct stmt_result *stmt_result;
	DB_RESULT result;

	va_start(ap, fmt);
	stmt = make_stmt(fmt, ap);
	va_end(ap);

	stmt_result = dql_parse(stmt);
	free(stmt);
	result.ptr = stmt_result;
	return result;
}

DB_RESULT db_parse(const char *stmt)
{
	struct stmt_result *stmt_result;
	DB_RESULT result;

	stmt_result = dql_parse(stmt);
	result.ptr = stmt_result;
	return result;
}

void db_free_result(DB_RESULT result)
{
	if (db_success(result) && db_is_sp(result)
			&& R(result)->val.spval.domain == STRING)
		free(R(result)->val.spval.ptr.pstring);
	if (db_success(result) && db_is_query(result))
		xrel_free(R(result)->val.rl);
}

bool db_success(DB_RESULT result)
{
	return result.ptr != NULL && R(result)->success;
}

int db_type(DB_RESULT result)
{
	return db_success(result) ? R(result)->type : -1;
}

bool db_is_definition(DB_RESULT result)
{
	return db_type(result) == DDL_STMT;
}

bool db_is_modification(DB_RESULT result)
{
	return db_type(result) == DML_MODI;
}

bool db_is_sp(DB_RESULT result)
{
	return db_type(result) == DML_SP;
}

bool db_is_query(DB_RESULT result)
{
	return db_type(result) == DML_QUERY;
}

static void fprint_value(FILE *stream, struct value *val)
{
	assert(val != NULL);

	switch (val->domain) {
		case INT:
			fprintf(stream, DB_INT_FMT_NICE, val->ptr.vint);
			break;
		case UINT:
			fprintf(stream, DB_UINT_FMT_NICE, val->ptr.vuint);
			break;
		case LONG:
			fprintf(stream, DB_LONG_FMT_NICE, val->ptr.vlong);
			break;
		case ULONG:
			fprintf(stream, DB_ULONG_FMT_NICE, val->ptr.vulong);
			break;
		case FLOAT:
			fprintf(stream, DB_FLOAT_FMT_NICE, val->ptr.vfloat);
			break;
		case DOUBLE:
			fprintf(stream, DB_DOUBLE_FMT_NICE, val->ptr.vdouble);
			break;
		case STRING:
			fprintf(stream, "%s", val->ptr.pstring);
			break;
		case BYTES:
			fprintf(stream, "(binary)");
			break;
		default:
			fprintf(stream, "(unknown)");
			break;
	}
	printf("\n");
}

unsigned long db_fprint(FILE *stream, DB_RESULT result)
{
	unsigned long cnt;

	if (!db_success(result)) {
		fprintf(stream, "An error occured.\n");
		return 0;
	}

	switch (db_type(result)) {
		case DDL_STMT:
			return 0;
		case DML_MODI:
			cnt = R(result)->val.aftpcnt;
			fprintf(stream, "%lu tuple%s affected.\n", cnt,
					(cnt > 1) ? "s" : "");
			return cnt;
		case DML_SP:
			fprint_value(stream, &R(result)->val.spval);
			return 1;
		case DML_QUERY:
			cnt = xrel_fprint(stream, R(result)->val.rl);
			fprintf(stream, "%lu tuple%s in relation.\n", cnt,
					(cnt > 1) ? "s" : "");
			return cnt;
		default:
			fprintf(stream, "Kakadu: %d\n", db_type(result));
			return 0;
	}
}

unsigned long db_print(DB_RESULT result)
{
	return db_fprint(stdout, result);
}

unsigned long db_tpcount(DB_RESULT result)
{
	if (db_success(result) && db_is_modification(result))
		return R(result)->val.aftpcnt;
	else
		return 0;
}

struct db_val db_spvalue(DB_RESULT result)
{
	struct db_val val;

	val.relation = "<sp>";
	val.name = "<sp>";
	val.domain = -1;
	val.size = 0;
	val.val.pstring = NULL;

	if (db_success(result) && db_is_sp(result)) {
		struct value *value;

		value = &R(result)->val.spval;
		switch (value->domain) {
			case INT:
				val.domain = DB_INT;
				val.size = sizeof(db_int_t);
				val.val.vint = value->ptr.vint;
				return val;
			case UINT:
				val.domain = DB_UINT;
				val.size = sizeof(db_uint_t);
				val.val.vuint = value->ptr.vuint;
				return val;
			case LONG:
				val.domain = DB_LONG;
				val.size = sizeof(db_long_t);
				val.val.vlong = value->ptr.vlong;
				return val;
			case ULONG:
				val.domain = DB_ULONG;
				val.size = sizeof(db_ulong_t);
				val.val.vulong = value->ptr.vulong;
				return val;
			case FLOAT:
				val.domain = DB_FLOAT;
				val.size = sizeof(db_float_t);
				val.val.vfloat = value->ptr.vfloat;
				return val;
			case DOUBLE:
				val.domain = DB_DOUBLE;
				val.size = sizeof(db_double_t);
				val.val.vdouble = value->ptr.vdouble;
				return val;
			case STRING:
				val.domain = DB_STRING;
				val.size = strlen(value->ptr.pstring);
				val.val.pstring = value->ptr.pstring;
				return val;
			case BYTES:
				return val;
			default:
				return val;
		}
	} else
		return val;
}

int db_attrcount(DB_RESULT result)
{
	if (db_success(result) && db_is_query(result))
		return R(result)->val.rl->rl_atcnt;
	else
		return -1;
}

DB_ITERATOR db_iterator(DB_RESULT result)
{
	DB_ITERATOR iter;
	struct xrel *rl;

	if (!db_success(result) || !db_is_query(result)) {
		iter.result.ptr = NULL;
		iter.rl = NULL;
		iter.iter = NULL;
		iter.val_buf =  NULL;
		return iter;
	}

	iter.result = result;
	rl = R(result)->val.rl;
	iter.rl = rl;
	iter.iter = rl->rl_iterator(rl);
	iter.val_buf = xmalloc(sizeof(struct db_val) * rl->rl_atcnt);
	return iter;
}

void db_free_iterator(DB_ITERATOR iter)
{
	if (iter.val_buf != NULL) 
		free(iter.val_buf);
	xrel_iter_free(ITER(iter));
}

const char *db_next_buf(DB_ITERATOR iter)
{
	return ITER(iter)->it_next(ITER(iter));
}

struct db_val *db_next(DB_ITERATOR iter)
{
	struct xrel *rl;
	struct db_val *vals;
	struct xattr *attr;
	const char *tuple, *val;
	int i;

	if ((tuple = db_next_buf(iter)) == NULL)
		return NULL;

	rl = RL(iter);
	vals = iter.val_buf;

	for (i = 0; i < rl->rl_atcnt; i++) {
		attr = rl->rl_attrs[i];
		vals[i].relation = attr->at_srl->rl_header.hd_name;
		vals[i].name = attr->at_sattr->at_name;
		vals[i].size = attr->at_sattr->at_size;
		val = tuple + rl->rl_attrs[i]->at_offset;
		switch (rl->rl_attrs[i]->at_sattr->at_domain) {
			case STRING:
				vals[i].domain = DB_STRING;
				vals[i].val.pstring = val;
				break;
			case BYTES:
				vals[i].domain = DB_BYTES;
				vals[i].val.pbytes = val;
				break;
			case INT:
				vals[i].domain = DB_INT;
				vals[i].val.vint = (*(db_int_t *)val);
				break;
			case UINT:
				vals[i].domain = DB_UINT;
				vals[i].val.vuint = (*(db_uint_t *)val);
				break;
			case LONG:
				vals[i].domain = DB_LONG;
				vals[i].val.vlong = (*(db_long_t *)val);
				break;
			case ULONG:
				vals[i].domain = DB_ULONG;
				vals[i].val.vulong = (*(db_ulong_t *)val);
				break;
			case FLOAT:
				vals[i].domain = DB_FLOAT;
				vals[i].val.vfloat = (*(db_float_t *)val);
				break;
			case DOUBLE:
				vals[i].domain = DB_DOUBLE;
				vals[i].val.vdouble = (*(db_double_t *)val);
				break;
			default:
				assert(false);
				return NULL;
		}
	}
	return vals;
}
 

void db_header(DB_RESULT result, void *ctx,
		void (*func)(void *ctx, unsigned short cnt,
			const struct db_val *vals))
{
	if (db_success(result) && db_is_query(result)) {
		struct xrel *rl = R(result)->val.rl;
		struct db_val vals[rl->rl_atcnt];
		int i;

		for (i = 0; i < rl->rl_atcnt; i++) {
			struct xattr *attr;

			attr = rl->rl_attrs[i];
			vals[i].relation = attr->at_srl->rl_header.hd_name;
			vals[i].name = attr->at_sattr->at_name;
			vals[i].size = attr->at_sattr->at_size;
			switch (rl->rl_attrs[i]->at_sattr->at_domain) {
				case STRING:
					vals[i].domain = DB_STRING;
					break;
				case BYTES:
					vals[i].domain = DB_BYTES;
					break;
				case INT:
					vals[i].domain = DB_INT;
					break;
				case UINT:
					vals[i].domain = DB_UINT;
					break;
				case LONG:
					vals[i].domain = DB_LONG;
					break;
				case ULONG:
					vals[i].domain = DB_ULONG;
					break;
				case FLOAT:
					vals[i].domain = DB_FLOAT;
					break;
				case DOUBLE:
					vals[i].domain = DB_DOUBLE;
					break;
				default:
					assert(false);
					return;
			}
		}
		func(ctx, rl->rl_atcnt, vals);
	}
}

int db_iterate(DB_RESULT result, void *ctx,
		void (*func)(void *ctx, unsigned short cnt,
			const struct db_val *vals))
{
	if (db_success(result) && db_is_query(result)) {
		struct xrel *rl = R(result)->val.rl;
		struct xattr *attr;
		struct db_val vals[rl->rl_atcnt];
		DB_ITERATOR iter;
		const char *tuple, *val;
		int i, j;

		for (i = 0; i < rl->rl_atcnt; i++) {
			attr = rl->rl_attrs[i];
			vals[i].relation = attr->at_srl->rl_header.hd_name;
			vals[i].name = attr->at_sattr->at_name;
			vals[i].size = attr->at_sattr->at_size;
			switch (rl->rl_attrs[i]->at_sattr->at_domain) {
				case STRING:
					vals[i].domain = DB_STRING;
					break;
				case BYTES:
					vals[i].domain = DB_BYTES;
					break;
				case INT:
					vals[i].domain = DB_INT;
					break;
				case UINT:
					vals[i].domain = DB_UINT;
					break;
				case LONG:
					vals[i].domain = DB_LONG;
					break;
				case ULONG:
					vals[i].domain = DB_ULONG;
					break;
				case FLOAT:
					vals[i].domain = DB_FLOAT;
					break;
				case DOUBLE:
					vals[i].domain = DB_DOUBLE;
					break;
				default:
					assert(false);
					return -1;
			}
		}

		iter = db_iterator(result);
		for (i = 0; (tuple = db_next_buf(iter)) != NULL; i++) {
			for (j = 0; j < rl->rl_atcnt; j++) {
				val = tuple + rl->rl_attrs[j]->at_offset;
				switch (rl->rl_attrs[j]->at_sattr->at_domain) {
					case STRING:
						vals[j].val.pstring = val;
						break;
					case BYTES:
						vals[j].val.pbytes = val;
						break;
					case INT:
						vals[j].val.vint
							= *(db_int_t *)val;
						break;
					case UINT:
						vals[j].val.vuint
							= *(db_uint_t *)val;
						break;
					case LONG:
						vals[j].val.vlong
							= *(db_long_t *)val;
						break;
					case ULONG:
						vals[j].val.vulong
							= *(db_ulong_t *)val;
						break;
					case FLOAT:
						vals[j].val.vfloat
							= *(db_float_t *)val;
						break;
					case DOUBLE:
						vals[j].val.vdouble
							= *(db_double_t *)val;
						break;
					default:
						assert(false);
						db_free_iterator(iter);
						return -1;
				}
			}
			func(ctx, rl->rl_atcnt, vals);
		}
		db_free_iterator(iter);
		return i;
	} else
		return -1;
}

void db_cleanup(void)
{
	dql_cleanup();
	close_relations();
}

