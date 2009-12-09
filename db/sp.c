/* vim:foldmethod=marker:
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
 * The goto- and action-table is calculated in nfa/nfa.c. The generated C code
 * is pasted into this file.
 * To avoid conflicts with the scanner, be careful that the grammar does not 
 * contain any terminal symbols not in a-z (e.g. '.' or '=').
 * The compiler is pretty ugly when it comes to memory management.
 */

#include "sp.h"
#include "constants.h"
#include "db.h"
#include "err.h"
#include "linkedlist.h"
#include "str.h"
#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>

#ifdef MALLOC_TRACE
#include <mcheck.h>
#endif

#define MAXPRGLEN	512
#define MAXNAME		32
#define MAXVARS		32
#define MAXARGS		8
#define MAXLINES	128
#define INVALID_CELL	((celladdr_t)-1)
#define INVALID_VALUE	(struct sp_value){ -1, { 0 } }
#define ERROR_CONTAINER	(container_t){ SP_PARSE_ERROR, { 0 } }
#define EMPTY_CONTAINER	(container_t){ SP_NOTHING, { 0 } }

#define CHECK(e)	{ if (!(e)) return ERR(E_SP_ERROR), ERROR_CONTAINER; }

#define DEBUG

typedef int ssize_t;
typedef int celladdr_t;

enum datatype { T_ERROR=-1, T_INT, T_FLOAT, T_STRING, T_TUPLE, T_AUTO };

typedef struct {
	unsigned short len;
	char *str;
} string_t;

typedef struct {
	int cnt;
	struct db_val *vals;
} tuple_t;

union value_u {
	db_int_t vint;
	db_float_t vfloat;
	string_t vstring;
	tuple_t vtuple;
};

struct sp_value {
	enum datatype type;
	union value_u val;
};

struct cell {
	enum { O_ASSIGN, O_MULT, O_DIV, O_ADD, O_SUB, O_MINUS, O_LT, O_LEQ,
		O_EQ, O_NEQ, O_GEQ, O_GT, O_AND, O_OR, O_IF, O_WHILE,
		O_FOREACH, O_RETURN, F_FUNCCALL, V_DECL, V_CONST, V_VAR,
		B_LIST, P_HEADER } type;
	union {
		struct header {
			int argc;
			celladdr_t argdecls[MAXARGS];
			celladdr_t body_addr;
			long addr_map[MAXPRGLEN];
		} header;
		struct {
			celladdr_t lines[MAXLINES];
			int cnt;
		} list;
		struct {
			celladdr_t oprnd[3];
		} op;
		struct {
			enum datatype type;
			int var_id;
		} decl;
		int var_id;
		struct sp_value con;
		struct {
			int func_id;
			celladdr_t argv[MAXARGS];
		} funccall;
	} action;
};

typedef struct {
	char name[MAXNAME+1];
	struct cell stack[MAXPRGLEN];
	int size;
	struct {
		enum datatype type;
		char symbol[MAXNAME+1];
	} vars[MAXVARS];
	int varcnt;
	mid_t id;
} context_t;

typedef struct {
	FILE *fp;
	long addr_map[MAXPRGLEN];
	struct sp_value vars[MAXVARS];
	bool is_auto[MAXVARS];
	bool is_initialized[MAXVARS];
	struct sp_value *retval;
	bool terminate;
	mid_t id;
} execstate_t;

/* The predefined functions. {{{ */

static struct sp_value f_query(execstate_t *state, const struct sp_value argv[])
{
	if (argv[0].type == T_STRING) {
		struct sp_value v;
		DB_RESULT result;

		v.type = T_INT;
		result = db_parse(argv[0].val.vstring.str);
		v.val.vint = db_success(result);
		db_free_result(result);
		return v;
	} else
		return INVALID_VALUE;
}

static struct sp_value f_db_attrcount(execstate_t *state,
		const struct sp_value argv[])
{
	if (argv[0].type == T_TUPLE) {
		struct sp_value v;

		v.type = T_INT;
		v.val.vint = argv[0].val.vtuple.cnt;
		return v;
	} else
		return INVALID_VALUE;
}

static struct sp_value f_db_attrname(execstate_t *state,
		const struct sp_value argv[])
{
	if (argv[0].type == T_TUPLE && argv[1].type == T_INT) {
		struct db_val *vals;
		int i;

		vals = argv[0].val.vtuple.vals;
		i = argv[1].val.vint;
		if (i < argv[0].val.vtuple.cnt) {
			struct sp_value v;

			v.type = T_STRING;
			v.val.vstring.str = cat_gc(state->id, 1, vals[i].name);
			v.val.vstring.len = strlen(vals[i].name);
			return v;
		} else
			return INVALID_VALUE;
	} else
		return INVALID_VALUE;
}

static struct sp_value f_db_attrval(execstate_t *state,
		const struct sp_value argv[])
{
	if (argv[0].type == T_TUPLE && (argv[1].type == T_STRING
				|| argv[1].type == T_INT)) {
		struct db_val *vals;
		struct sp_value v;
		int i;

		vals = argv[0].val.vtuple.vals;

		if (argv[1].type == T_STRING) {
			int cnt;
			const char *name;

			cnt = argv[0].val.vtuple.cnt;
			name = argv[1].val.vstring.str;
			for (i = 0; i < cnt; i++)
				if (!strcmp(vals[i].name, name))
					break;
		} else
			i = argv[1].val.vint;

		if (i >= argv[0].val.vtuple.cnt)
			return INVALID_VALUE;

		switch (vals[i].domain) {
			case DB_INT:
				v.type = T_INT;
				v.val.vint = vals[i].val.vint;
				return v;
			case DB_FLOAT:
				v.type = T_FLOAT;
				v.val.vfloat = vals[i].val.vfloat;
				return v;
			case DB_STRING:
				v.type = T_STRING;
				v.val.vstring.str = cat_gc(state->id,
						1, vals[i].val.pstring);
				v.val.vstring.len
					= strlen(v.val.vstring.str);
				return v;
			case DB_BYTES:
				v.type = T_STRING;
				v.val.vstring.str = cat_gc(state->id,
						1, vals[i].val.pstring);
				v.val.vstring.len = vals[i].size;
				return v;
			default:
				return INVALID_VALUE;
		}
	} else
		return INVALID_VALUE;
}

static struct sp_value f_strlen(execstate_t *state,
		const struct sp_value argv[])
{
	if (argv[0].type == T_STRING) {
		struct sp_value v;

		v.type = T_INT;
		v.val.vint = argv[0].val.vstring.len;
		return v;
	} else
		return INVALID_VALUE;
}

static struct sp_value f_substr(execstate_t *state,
		const struct sp_value argv[])
{
	if (argv[0].type == T_STRING
			&& argv[1].type == T_INT
			&& argv[2].type == T_INT) {
		struct sp_value v;
		const char *str;
		char *sub;
		int len, i, j, k;

		str = argv[0].val.vstring.str;
		len = argv[0].val.vstring.len;
		i = argv[1].val.vint;
		j = argv[2].val.vint;
		if (j < 0) {
			j *= -1;
			i -= j;
		}
		if (i < 0 || i+j > len)
			return INVALID_VALUE;
		sub = gmalloc(j + 1, state->id);
		for (k = 0; k < j; k++)
			sub[k] = str[i+k];
		sub[k] = '\0';

		v.type = T_STRING;
		v.val.vstring.str = sub;
		v.val.vstring.len = j;
		return v;
	} else
		return INVALID_VALUE;
}

static struct sp_value f_strindex(execstate_t *state,
		const struct sp_value argv[])
{
	if (argv[0].type == T_STRING
			&& argv[1].type == T_STRING) {
		struct sp_value v;
		const char *haystack, *needle, *sub;

		haystack = argv[0].val.vstring.str;
		needle = argv[1].val.vstring.str;
		sub = strstr(haystack, needle);

		v.type = T_INT;
		v.val.vint = (sub != NULL) ? sub - haystack : -1;
		return v;
	} else
		return INVALID_VALUE;
}

static struct sp_value f_newline(execstate_t *state,
		const struct sp_value argv[])
{
	printf("\n");
	return INVALID_VALUE;
}

static struct sp_value f_echo(execstate_t *state, const struct sp_value argv[])
{
	if (argv[0].type == T_INT)
		printf("%d\n", argv[0].val.vint);
	else if (argv[0].type == T_FLOAT)
		printf("%f\n", argv[0].val.vfloat);
	else if (argv[0].type == T_STRING)
		printf("%s\n", argv[0].val.vstring.str);
	else if (argv[0].type == T_TUPLE) {
		struct db_val *vals;
		int i, cnt;

		vals = argv[0].val.vtuple.vals;
		cnt = argv[0].val.vtuple.cnt;
		printf("(");
		for (i = 0; i < cnt; i++) {
			switch (vals[i].domain) {
				case DB_INT:
					printf("%d", vals[i].val.vint);
					break;
				case DB_UINT:
					printf("%u", vals[i].val.vuint);
					break;
				case DB_LONG:
					printf("%ld", vals[i].val.vlong);
					break;
				case DB_ULONG:
					printf("%lu", vals[i].val.vulong);
					break;
				case DB_FLOAT:
					printf("%f", vals[i].val.vfloat);
					break;
				case DB_DOUBLE:
					printf("%f", vals[i].val.vdouble);
					break;
				case DB_STRING:
					printf("'%s'", vals[i].val.pstring);
					break;
				case DB_BYTES:
					printf("<byte sequence>");
					break;
			}
			if (i+1 < cnt)
				printf(",");
		}
		printf(")\n");
	}
	return INVALID_VALUE;
}

static struct sp_value f_is_int(execstate_t *state,
		const struct sp_value argv[])
{
	struct sp_value v;

	v.type = T_INT;
	v.val.vint = (argv[0].type == T_INT);
	return v;
}

static struct sp_value f_is_float(execstate_t *state,
		const struct sp_value argv[])
{
	struct sp_value v;

	v.type = T_INT;
	v.val.vint = (argv[0].type == T_FLOAT);
	return v;
}

static struct sp_value f_is_string(execstate_t *state,
		const struct sp_value argv[])
{
	struct sp_value v;

	v.type = T_INT;
	v.val.vint = (argv[0].type == T_STRING);
	return v;
}

static struct sp_value f_to_int(execstate_t *state,
		const struct sp_value argv[])
{
	if (argv[0].type == T_INT)
		return argv[0];
	else {
		struct sp_value v;

		v.type = T_INT;
		v.val.vint = 0;
		if (argv[0].type == T_FLOAT)
			v.val.vint = (int)argv[0].val.vfloat;
		else if (argv[0].type == T_STRING)
			sscanf(argv[0].val.vstring.str, "%d", &v.val.vint);
		return v;
	}
}

static struct sp_value f_to_float(execstate_t *state,
		const struct sp_value argv[])
{
	if (argv[0].type == T_FLOAT)
		return argv[0];
	else {
		struct sp_value v;

		v.type = T_FLOAT;
		v.val.vfloat = 0.0;
		if (argv[0].type == T_INT)
			v.val.vfloat = (float)argv[0].val.vint;
		else if (argv[0].type == T_STRING)
			sscanf(argv[0].val.vstring.str, "%f", &v.val.vfloat);
		return v;
	}
}

static struct sp_value f_to_string(execstate_t *state,
		const struct sp_value argv[])
{
	if (argv[0].type == T_STRING)
		return argv[0];
	else {
		struct sp_value v;
		char buf[512];

		v.type = T_STRING;
		v.val.vfloat = 0.0;
		if (argv[0].type == T_INT)
			sprintf(buf, "%d", argv[0].val.vint);
		else if (argv[0].type == T_FLOAT)
			sprintf(buf, "%f", argv[0].val.vfloat);
		v.val.vstring.str = cat_gc(state->id, 1, buf);
		v.val.vstring.len = strlen(buf);
		return v;
	}
}

static struct function {
	char *symbol;
	int argc;
	struct sp_value (*func)(execstate_t *state,
			const struct sp_value argv[]);
} functions[] = {
	{ "exec", 1, f_query },
	{ "attrcount", 1, f_db_attrcount },
	{ "attrname", 2, f_db_attrname },
	{ "attrval", 2, f_db_attrval },
	{ "echo", 1, f_echo },
	{ "newline", 0, f_newline },
	{ "strlen", 1, f_strlen },
	{ "substr", 3, f_substr },
	{ "strindex", 2, f_strindex },
	{ "is_int", 1, f_is_int },
	{ "is_float", 1, f_is_float },
	{ "is_string", 1, f_is_string },
	{ "to_int", 1, f_to_int },
	{ "to_float", 1, f_to_float },
	{ "to_string", 1, f_to_string },
};

#define FUNCTIONS_CNT	((ssize_t)(sizeof(functions) / sizeof(functions[0])))

/* }}} */

/* Printing and debugging functions. {{{ */

#ifdef DEBUG
static void print_program(context_t *ctx)
{
	int i, j;

	printf("VARIABLES: ");
	for (i = 0; i < ctx->varcnt; i++)
		printf("%s(%d), ", ctx->vars[i].symbol, ctx->vars[i].type);
	printf("\n");
	printf("STATEMENTS:\n");
	for (i = 0; i < ctx->size; i++) {
		struct cell *c;

		c = &ctx->stack[i];
		printf("%2d.)  ", i);
		switch (c->type) {
		case O_ASSIGN:
			printf("O_ASSIGN\n");
			goto op;
			break;
		case O_MULT:
			printf("O_MULT\n");
			goto op;
			break;
		case O_DIV:
			printf("O_DIV\n");
			goto op;
			break;
		case O_ADD:
			printf("O_ADD\n");
			goto op;
			break;
		case O_SUB:
			printf("O_SUB\n");
			goto op;
			break;
		case O_MINUS:
			printf("O_MINUS\n");
			goto op;
			break;
		case O_LT:
			printf("O_LT\n");
			goto op;
			break;
		case O_GT:
			printf("O_GT\n");
			goto op;
			break;
		case O_LEQ:
			printf("O_LEQ\n");
			goto op;
			break;
		case O_GEQ:
			printf("O_GEQ\n");
			goto op;
			break;
		case O_NEQ:
			printf("O_NEQ\n");
			goto op;
			break;
		case O_EQ:
			printf("O_EQ\n");
			goto op;
			break;
		case O_AND:
			printf("O_AND\n");
			goto op;
			break;
		case O_OR:
			printf("O_OR\n");
			goto op;
			break;
		case O_IF:
			printf("O_IF\n");
			goto op;
			break;
		case O_WHILE:
			printf("O_WHILE\n");
			goto op;
			break;
		case O_FOREACH:
			printf("O_FOREACH\n");
			goto op;
			break;
		case O_RETURN:
			printf("O_RETURN\n");
op:
			printf("\t%d | %d | %d\n",
					c->action.op.oprnd[0],
					c->action.op.oprnd[1],
					c->action.op.oprnd[2]);
			break;
		case V_DECL:
			printf("V_DECL\n");
			printf("\t");
			PRINT_INT(c->action.decl.type);
			printf("\t");
			PRINT_INT(c->action.decl.var_id);
			break;
		case F_FUNCCALL:
			printf("F_FUNCCALL\n");
			printf("\t");
			PRINT_INT(c->action.funccall.func_id);
			break;
		case V_CONST:
			printf("V_CONST\n");
			printf("\t");
			switch (c->action.con.type) {
			case T_INT:
				PRINT_INT(c->action.con.val.vint);
				break;
			case T_FLOAT:
				PRINT_FLOAT(c->action.con.val.vfloat);
				break;
			case T_STRING:
				PRINT_STR(c->action.con.val.vstring.str);
				break;
			case T_TUPLE:
				assert(false);
			case T_AUTO:
				assert(false);
			case T_ERROR:
				assert(false);
			}
			break;
		case V_VAR:
			printf("V_VAR\n");
			printf("\t");
			PRINT_INT(c->action.var_id);
			break;
		case B_LIST:
			printf("B_LIST\n");
			printf("\t");
			PRINT_INT(c->action.list.cnt);
			for (j = 0; j < c->action.list.cnt; j++) {
				printf("\t");
				PRINT_INT(c->action.list.lines[j]);
			}
			break;
		case P_HEADER:
			printf("P_HEADER\n");
			printf("\t");
			PRINT_INT(c->action.header.argc);
			for (j = 0; j < c->action.header.argc; j++) {
				printf("\t");
				PRINT_INT(c->action.header.argdecls[j]);
			}
			printf("\t");
			PRINT_INT(c->action.header.body_addr);
			break;
		}
	}
}
#endif /* }}} */

/* Bytecode IO functions. {{{ */

#define WRITE(fp, var)		((bool)fwrite(&(var), sizeof (var), 1, fp) == 1)
#define TWRITE(fp, var)		((WRITE(fp, var)) ? true :\
					(ERR(E_IO_ERROR), false))
#define WRITEA(fp, var, len)	((bool)fwrite(var, sizeof (*var), len, fp) == 1)
#define TWRITEA(fp, var, len)	((WRITEA(fp, var, len)) ? true\
					: (ERR(E_IO_ERROR), false))

static long write_cell(const struct cell *cell, FILE *fp)
{
	long pos;

	pos = ftell(fp);
	if (!TWRITE(fp, cell->type))
		return -1;
	switch (cell->type) {
		case O_ASSIGN:
		case O_MULT:
		case O_DIV:
		case O_ADD:
		case O_SUB:
		case O_MINUS:
		case O_LT:
		case O_LEQ:
		case O_EQ:
		case O_NEQ:
		case O_AND:
		case O_OR:
		case O_GT:
		case O_GEQ:
		case O_IF:
		case O_WHILE:
		case O_FOREACH:
		case O_RETURN:
			if (!TWRITE(fp, cell->action.op))
				return -1;
			break;
		case F_FUNCCALL:
			if (!TWRITE(fp, cell->action.funccall))
				return -1;
			break;
		case V_DECL:
			if (!TWRITE(fp, cell->action.decl))
				return -1;
			break;
		case V_CONST:
			if (!TWRITE(fp, cell->action.con.type))
				return -1;
			if (cell->action.con.type == T_STRING) {
				int len = cell->action.con.val.vstring.len;
				char *s = cell->action.con.val.vstring.str;
				if (!TWRITE(fp, len))
					return -1;
				if (len > 0)
					if (!TWRITEA(fp, s, len))
						return -1;
			} else {
				if (!TWRITE(fp, cell->action.con.val))
					return -1;
			}
			break;
		case V_VAR:
			if (!TWRITE(fp, cell->action.var_id))
				return -1;
			break;
		case B_LIST:
			if (!TWRITE(fp, cell->action.list))
				return -1;
			break;
		case P_HEADER:
			if (!TWRITE(fp, cell->action.header))
				return -1;
			break;
	}
	return pos;
}

static bool generate_byte_code(context_t *ctx)
{
	char *fn;
	FILE *fp;
	long pos;
	struct header *hdr;
	int i;

	assert(ctx->stack[ctx->size - 1].type == P_HEADER);
	assert(ctx->size <= MAXPRGLEN);

	fn = cat(3, SP_BASEDIR, ctx->name, SP_SUFFIX);
	fp = fopen(fn, "wb");
	free(fn);
	if (!fp) {
		ERR(E_OPEN_FAILED);
		return false;
	}

	fseek(fp, sizeof(long), SEEK_SET); /* offset */
	hdr = &ctx->stack[ctx->size - 1].action.header;

	for (i = 0; i < ctx->size; i++) {
		pos = write_cell(&ctx->stack[i], fp);
		if (pos == -1) {
			ERR(E_SP_WRITE_CELL_FAILED);
			fclose(fp);
			return false;
		}
		hdr->addr_map[i] = pos;
	}

	fseek(fp, 0L, SEEK_SET);
	if (!TWRITE(fp, pos)) {
		ERR(E_SP_WRITE_START_FAILED);
		fclose(fp);
		return false;
	}

	fclose(fp);
	return true;
}

#define READ(fp, var)		((bool)fread(&(var), sizeof (var), 1, fp) == 1)
#define TREAD(fp, var)		((READ(fp, var)) ? true :\
					(ERR(E_IO_ERROR), false))
#define READA(fp, var, len)	((bool)fread(var, sizeof (*var), len, fp) == 1)
#define TREADA(fp, var, len)	((READA(fp, var, len)) ? true\
					: (ERR(E_IO_ERROR), false))

static bool read_cell(struct cell *cell, execstate_t *state, long pos)
{
	if (fseek(state->fp, pos, SEEK_SET) == -1)
		return false;

	if (!TREAD(state->fp, cell->type))
		return false;
	switch (cell->type) {
		case O_ASSIGN:
		case O_MULT:
		case O_DIV:
		case O_ADD:
		case O_SUB:
		case O_MINUS:
		case O_LT:
		case O_LEQ:
		case O_EQ:
		case O_NEQ:
		case O_AND:
		case O_OR:
		case O_GT:
		case O_GEQ:
		case O_IF:
		case O_WHILE:
		case O_FOREACH:
		case O_RETURN:
			if (!TREAD(state->fp, cell->action.op))
				return false;
			break;
		case F_FUNCCALL:
			if (!TREAD(state->fp, cell->action.funccall))
				return false;
			break;
		case V_DECL:
			if (!TREAD(state->fp, cell->action.decl))
				return false;
			break;
		case V_CONST:
			if (!TREAD(state->fp, cell->action.con.type))
				return false;
			if (cell->action.con.type == T_STRING) {
				int len;
				char *s;

				if (!TREAD(state->fp, len))
					return false;
				s = gmalloc(len + 1, state->id);
				if (len > 0)
					if (!TREADA(state->fp, s, len))
						return false;
				s[len] = '\0';
				cell->action.con.val.vstring.len = len;
				cell->action.con.val.vstring.str = s;
			} else {
				if (!TREAD(state->fp, cell->action.con.val))
					return false;
			}
			break;
		case V_VAR:
			if (!TREAD(state->fp, cell->action.var_id))
				return false;
			break;
		case B_LIST:
			if (!TREAD(state->fp, cell->action.list))
				return false;
			break;
		case P_HEADER:
			if (!TREAD(state->fp, cell->action.header))
				return false;
			break;
	}
	return true;
}

static bool exec_load_decl(execstate_t *state, celladdr_t addr)
{
	struct cell cell;

	if (!read_cell(&cell, state, state->addr_map[addr])) {
		ERR(E_SP_READ_CELL_FAILED);
		return false;
	}
	if (cell.type != V_DECL) {
		ERR(E_SP_UNEXPECTED_CELL);
		return false;
	}
	state->vars[cell.action.decl.var_id].type = cell.action.decl.type;
	state->is_auto[cell.action.decl.var_id]
		= cell.action.decl.type == T_AUTO;
	state->is_initialized[cell.action.decl.var_id] = false;
	return true;
}

static struct sp_value exec_load_expr(execstate_t *state, celladdr_t addr);

static struct sp_value exec_funccall(execstate_t *state, int func_id,
		celladdr_t argv[])
{
	struct sp_value argvvals[functions[func_id].argc];
	int i;

	for (i = 0; i < functions[func_id].argc; i++)
		argvvals[i] = exec_load_expr(state, argv[i]);
	return functions[func_id].func(state, argvvals);
}

#define EXEC_OP(t)	exec_op_##t

#define EXEC_OP_DEF(t,o) \
	static struct sp_value exec_op_##t(execstate_t *state,\
			struct sp_value v1, struct sp_value v2)\
	{\
		struct sp_value v;\
		\
		if (v1.type == T_INT && v2.type == T_INT) {\
			v.type = T_INT;\
			v.val.vint = v1.val.vint o v2.val.vint;\
		} else if (v1.type == T_FLOAT && v2.type == T_FLOAT) {\
			v.type = T_FLOAT;\
			v.val.vfloat = v1.val.vfloat o v2.val.vfloat;\
		} else if (v1.type == T_INT && v2.type == T_FLOAT) {\
			v.type = T_FLOAT;\
			v.val.vfloat = (float)v1.val.vint o v2.val.vfloat;\
		} else if (v1.type == T_FLOAT && v2.type == T_INT) {\
			v.type = T_FLOAT;\
			v.val.vfloat = v1.val.vfloat o (float)v2.val.vint;\
		} else\
			v.type = -1;\
		return v;\
	}

#define EXEC_OP_DEF_STR(t,o) \
	static struct sp_value exec_op_##t(execstate_t *state,\
			struct sp_value v1, struct sp_value v2)\
	{\
		struct sp_value v;\
		\
		if (v1.type == T_INT && v2.type == T_INT) {\
			v.type = T_INT;\
			v.val.vint = v1.val.vint o v2.val.vint;\
		} else if (v1.type == T_FLOAT && v2.type == T_FLOAT) {\
			v.type = T_FLOAT;\
			v.val.vfloat = v1.val.vfloat o v2.val.vfloat;\
		} else if (v1.type == T_INT && v2.type == T_FLOAT) {\
			v.type = T_FLOAT;\
			v.val.vfloat = (float)v1.val.vint o v2.val.vfloat;\
		} else if (v1.type == T_FLOAT && v2.type == T_INT) {\
			v.type = T_FLOAT;\
			v.val.vfloat = v1.val.vfloat o (float)v2.val.vint;\
		} else if (v1.type == T_STRING && v2.type == T_STRING) {\
			v.type = T_INT;\
			v.val.vint = strcmp(v1.val.vstring.str,\
					v2.val.vstring.str) o 0;\
		} else\
			v.type = -1;\
		return v;\
	}

EXEC_OP_DEF(add, +)
EXEC_OP_DEF(sub, -)
EXEC_OP_DEF(mult, *)
EXEC_OP_DEF(div, /)
EXEC_OP_DEF(and, &&)
EXEC_OP_DEF(or, ||)
EXEC_OP_DEF_STR(eq, ==)
EXEC_OP_DEF_STR(neq, !=)
EXEC_OP_DEF_STR(leq, <=)
EXEC_OP_DEF_STR(lt, <)
EXEC_OP_DEF_STR(geq, >=)
EXEC_OP_DEF_STR(gt, >)

static struct sp_value exec_add(execstate_t *state, struct sp_value v1,
		struct sp_value v2)
{
	if (v1.type != T_STRING && v2.type != T_STRING)
		return EXEC_OP(add)(state, v1, v2);
	else {
		struct sp_value v;
		char buf[512], *s1, *s2;

		if (v1.type == T_STRING)
			s1 = v1.val.vstring.str;
		else if (v1.type == T_INT) {
			sprintf(buf, "%d", v1.val.vint);
			s1 = buf;
		} else if (v1.type == T_FLOAT) {
			sprintf(buf, "%f", v1.val.vfloat);
			s1 = buf;
		} else
			s1 = NULL;

		if (v2.type == T_STRING)
			s2 = v2.val.vstring.str;
		else if (v2.type == T_INT) {
			sprintf(buf, "%d", v2.val.vint);
			s2 = buf;
		} else if (v1.type == T_FLOAT) {
			sprintf(buf, "%f", v2.val.vfloat);
			s2 = buf;
		} else
			s2 = NULL;

		v.type = T_STRING;
		v.val.vstring.str = cat_gc(state->id, 2, s1, s2);
		v.val.vstring.len = strlen(v.val.vstring.str);
		return v;
	}
}

static struct sp_value exec_load_expr(execstate_t *state, celladdr_t addr)
{
	struct cell cell;
	struct sp_value (*func)(execstate_t *, struct sp_value, struct sp_value);
	struct sp_value v1, v2;

	if (!read_cell(&cell, state, state->addr_map[addr])) {
		ERR(E_SP_READ_CELL_FAILED);
		return INVALID_VALUE;
	}
	switch (cell.type) {
		case V_VAR:
			if (!state->is_initialized[cell.action.var_id]) {
				ERR(E_SP_VAR_NOT_INITIALIZED);
				return INVALID_VALUE;
			}
			return state->vars[cell.action.var_id];
		case V_CONST:
			return cell.action.con;
		case F_FUNCCALL:
			return exec_funccall(state,
					cell.action.funccall.func_id,
					cell.action.funccall.argv);
		case O_ADD:
			func = exec_add;
			break;
		case O_SUB:
			func = EXEC_OP(sub);
			break;
		case O_MULT:
			func = EXEC_OP(mult);
			break;
		case O_DIV:
			func = EXEC_OP(div);
			break;
		case O_MINUS:
			v1 = exec_load_expr(state, cell.action.op.oprnd[0]);
			if (v1.type == T_INT)
				v1.val.vint *= -1;
			else if (v1.type == T_FLOAT)
				v1.val.vfloat *= -1;
			return v1;
		case O_EQ:
			func = EXEC_OP(eq);
			break;
		case O_NEQ:
			func = EXEC_OP(neq);
			break;
		case O_AND:
			func = EXEC_OP(and);
			break;
		case O_OR:
			func = EXEC_OP(or);
			break;
		case O_LEQ:
			func = EXEC_OP(leq);
			break;
		case O_LT:
			func = EXEC_OP(lt);
			break;
		case O_GEQ:
			func = EXEC_OP(geq);
			break;
		case O_GT:
			func = EXEC_OP(gt);
			break;
		default:
			ERR(E_SP_UNEXPECTED_CELL);
			return INVALID_VALUE;
	}
	v1 = exec_load_expr(state, cell.action.op.oprnd[0]);
	v2 = exec_load_expr(state, cell.action.op.oprnd[1]);
	return func(state, v1, v2);
}

static bool exec_expr_is_true(execstate_t *state, celladdr_t expr_addr)
{
	struct sp_value v;

	v = exec_load_expr(state, expr_addr);
	switch (v.type) {
		case T_INT:
			return v.val.vint != 0;
		case T_FLOAT:
			return v.val.vfloat != 0.0;
		case T_STRING:
			return v.val.vstring.len > 0;
		case T_TUPLE:
			return false;
		case T_AUTO:
			return false;
		case -1:
			return false;
		default:
			assert(false);
			return false;
	}
}

static bool exec_assign(execstate_t *state, int var_id,
		celladdr_t expr_addr)
{
	struct sp_value expr_val;

	expr_val = exec_load_expr(state, expr_addr);
	if (expr_val.type == -1) {
		ERR(E_SP_INVALID_EXPR);
		return false;
	}

	if (!state->is_auto[var_id]
			&& expr_val.type != state->vars[var_id].type) {
		OUT();
		PRINT_INT(state->is_auto[var_id]);
		PRINT_INT(state->vars[var_id].type);
		ERR(E_SP_INVALID_EXPR_TYPE);
		return false;
	}

	state->vars[var_id].type = expr_val.type;
	state->vars[var_id].val = expr_val.val;
	state->is_initialized[var_id] = true;
	return true;
}

static bool exec_list(execstate_t *state, celladdr_t lines[], int cnt);
static bool exec_load_list(execstate_t *state, celladdr_t addr);

static bool exec_load_line(execstate_t *state, celladdr_t addr)
{
	struct cell cell;

	if (!read_cell(&cell, state, state->addr_map[addr])) {
		ERR(E_SP_READ_CELL_FAILED);
		return false;
	}

	if (cell.type == V_DECL) {
		state->vars[cell.action.decl.var_id].type
			= cell.action.decl.type;
		state->is_auto[cell.action.decl.var_id]
			= cell.action.decl.type == T_AUTO;
		state->is_initialized[cell.action.decl.var_id] = false;
		return true;
	} else if (cell.type == F_FUNCCALL) {
		exec_funccall(state, cell.action.funccall.func_id,
				cell.action.funccall.argv);
		return true;
	} else if (cell.type == O_ASSIGN) {
		exec_assign(state, cell.action.op.oprnd[0],
				cell.action.op.oprnd[1]);
		return true;
	} else if (cell.type == O_IF) {
		if (exec_expr_is_true(state, cell.action.op.oprnd[0])) {
			if (!exec_load_list(state, cell.action.op.oprnd[1])) {
				ERR(E_SP_LIST_ERROR);
				return false;
			}
			if (state->terminate)
				return true;
		}
		return true;
	} else if (cell.type == O_WHILE) {
		while (exec_expr_is_true(state, cell.action.op.oprnd[0])) {
			if (!exec_load_list(state, cell.action.op.oprnd[1])) {
				ERR(E_SP_LIST_ERROR);
				return false;
			}
			if (state->terminate)
				return true;
		}
		return true;
	} else if (cell.type == O_FOREACH) {
		int var_id, cnt;
		celladdr_t expr_addr, block_addr;
		struct sp_value expr_val;
		DB_RESULT result;
		DB_ITERATOR iter;
		struct db_val *vals;

		var_id = cell.action.op.oprnd[0];
		expr_addr = cell.action.op.oprnd[1];
		block_addr = cell.action.op.oprnd[2];

		if (state->vars[var_id].type != T_TUPLE) {
			ERR(E_SP_INVALID_VAR_TYPE);
			return false;
		}

		expr_val = exec_load_expr(state, expr_addr);
		if (expr_val.type != T_STRING) {
			ERR(E_SP_INVALID_EXPR_TYPE);
			return false;
		}

		result = db_parse(expr_val.val.vstring.str);
		if (!db_success(result)) {
			ERR(E_SP_QUERY_FAILED);
			return false;
		}

		cnt = db_attrcount(result);
		iter = db_iterator(result);
		while ((vals = db_next(iter)) != NULL) {
			state->vars[var_id].type = T_TUPLE;
			state->vars[var_id].val.vtuple.cnt = cnt;
			state->vars[var_id].val.vtuple.vals = vals;
			state->is_initialized[var_id] = true;
			if (!exec_load_list(state, cell.action.op.oprnd[2])) {
				ERR(E_SP_LIST_ERROR);
				db_free_iterator(iter);
				db_free_result(result);
				return false;
			}
			if (state->terminate) {
				db_free_iterator(iter);
				db_free_result(result);
				return true;
			}
		}
		db_free_iterator(iter);
		db_free_result(result);
		return true;
	} else if (cell.type == B_LIST) {
		return exec_list(state, cell.action.list.lines,
				cell.action.list.cnt);
	} else if (cell.type == O_RETURN) {
		struct sp_value v;

		v = exec_load_expr(state, cell.action.op.oprnd[0]);
		*state->retval = v;
		state->terminate = true;
		return true;
	} else {
		ERR(E_SP_UNEXPECTED_CELL);
		return false;
	}
}

static bool exec_list(execstate_t *state, celladdr_t lines[], int cnt)
{
	int i;

	for (i = 0; i < cnt; i++) {
		if (!exec_load_line(state, lines[i])) {
			ERR(E_SP_LIST_ERROR);
			return false;
		}
		if (state->terminate)
			return true;
	}
	return true;
}

static bool exec_load_list(execstate_t *state, celladdr_t addr)
{
	struct cell cell;

	if (!read_cell(&cell, state, state->addr_map[addr])) {
		ERR(E_SP_READ_CELL_FAILED);
		return false;
	}

	if (cell.type != B_LIST) {
		ERR(E_SP_UNEXPECTED_CELL);
		return false;
	}

	return exec_list(state, cell.action.list.lines,
			cell.action.list.cnt);
}

static bool interpret_byte_code(const char *name, int argc,
		const struct sp_value argv[], struct sp_value *retval)
{
	char *fn;
	FILE *fp;
	long pos;
	struct cell cell;
	execstate_t state;
	celladdr_t body_addr;
	int i;

	fn = cat(3, SP_BASEDIR, name, SP_SUFFIX);
	fp = fopen(fn, "rb");
	free(fn);
	if (!fp) {
		ERR(E_OPEN_FAILED);
		return false;
	}

	if (!TREAD(fp, pos)) {
		ERR(E_SP_READ_START_FAILED);
		fclose(fp);
		return false;
	}
	state.fp = fp;
	state.retval = retval;
	state.terminate = false;
	state.id = gnew();
	if (!read_cell(&cell, &state, pos)) {
		ERR(E_SP_READ_CELL_FAILED);
		fclose(fp);
		gc(state.id);
		return false;
	}
	if (cell.type != P_HEADER) {
		ERR(E_SP_INVALID_HEADER);
		fclose(fp);
		gc(state.id);
		return false;
	}
	if (cell.action.header.argc != argc) {
		ERR(E_SP_INVALID_ARGC);
		fclose(fp);
		gc(state.id);
		return false;
	}

	body_addr = cell.action.header.body_addr;
	memcpy(state.addr_map, cell.action.header.addr_map,
			sizeof(cell.action.header.addr_map));

	for (i = 0; i < argc; i++) {
		if (!exec_load_decl(&state, cell.action.header.argdecls[i])) {
			ERR(E_SP_DECL_FAILED);
			fclose(fp);
			gc(state.id);
			return false;
		}
		state.vars[i].type = argv[i].type;
		state.vars[i].val = argv[i].val;
		state.is_initialized[i] = true;
	}

	if (!exec_load_list(&state, body_addr)) {
		ERR(E_SP_LIST_ERROR);
		fclose(fp);
		gc(state.id);
		return false;
	}
	if (!state.terminate) {
		ERR(E_SP_RETURN_ERROR);
		fclose(fp);
		gc(state.id);
		return false;
	}

	if (retval->type == T_STRING) /* create a new copy of return val */
		retval->val.vstring.str = cat(1, retval->val.vstring.str);
	fclose(fp);
	gc(state.id);
	return true;
}

/* }}} */

/* Reduction functions (called while parsing). {{{ */

typedef struct {
	enum { SP_PARSE_ERROR, SP_NOTHING, SP_ADDR, SP_LIST, SP_SYMBOL,
	       SP_C_INT, SP_C_FLOAT, SP_C_STRING } type;
	union {
		celladdr_t addr;
		struct llist *list;
		char *symbol;
		int vint;
		float vfloat;
		char *vstring;
	} val;
} container_t;

static celladdr_t store_cell(context_t *ctx, struct cell *cell)
{
	ctx->stack[ctx->size] = *cell;
	return ctx->size++;
}

static int get_func_id(context_t *ctx, const char *symbol)
{
	int i;

	for (i = 0; i < FUNCTIONS_CNT; i++)
		if (!strncmp(symbol, functions[i].symbol, MAXNAME))
			break;
	if (i == FUNCTIONS_CNT) {
		ERR(E_SP_FUNC_NOT_FOUND);
		return -1;
	} else
		return i;
}

static int get_var_id(context_t *ctx, const char *symbol)
{
	int i;

	for (i = 0; i < ctx->varcnt; i++)
		if (!strncmp(symbol, ctx->vars[i].symbol, MAXNAME))
			break;
	if (i == ctx->varcnt) {
		ERR(E_SP_VAR_NOT_FOUND);
		return -1;
	} else
		return i;
}

#define RDC_FWD(i,j)	rdc_fwd_##i_##j
#define RDC_FWD_DEF(i,j) static container_t rdc_fwd_##i_##j(context_t *ctx,\
					int argc, container_t argv[])\
			{\
				assert(argc == j);\
				\
				return argv[i-1];\
			}

RDC_FWD_DEF(2, 3)

static container_t rdc_procedure_args(context_t *ctx, int argc,
		container_t argv[])
{
	celladdr_t addr;
	container_t c;
	struct cell cell;
	struct llist *list;
	struct llentry *e;
	int i;

	CHECK(argc == 6);
	CHECK(argv[1].type == SP_SYMBOL);
	CHECK(argv[3].type == SP_LIST);
	CHECK(argv[5].type == SP_ADDR);

	strntermcpy(ctx->name, argv[1].val.symbol, MAXNAME+1);

	cell.type = P_HEADER;
	list = argv[3].val.list;
	for (i = list->cnt-1, e = list->first; e != NULL; i--, e = e->next) {
		CHECK(i < MAXARGS);
		cell.action.header.argdecls[i] = *(celladdr_t *)e->val;
	}
	cell.action.header.argc = list->cnt;
	ll_free(list);
	cell.action.header.body_addr = argv[5].val.addr;
	addr = store_cell(ctx, &cell);

	c.type = SP_ADDR;
	c.val.addr = addr;
	return c;
}

static container_t rdc_procedure_void(context_t *ctx, int argc,
		container_t argv[])
{
	celladdr_t addr;
	container_t c;
	struct cell cell;

	CHECK(argc == 5);
	CHECK(argv[1].type == SP_SYMBOL);
	CHECK(argv[4].type == SP_ADDR);

	strntermcpy(ctx->name, argv[1].val.symbol, MAXNAME+1);

	cell.type = P_HEADER;
	cell.action.header.argc = 0;
	cell.action.header.body_addr = argv[4].val.addr;
	addr = store_cell(ctx, &cell);

	c.type = SP_ADDR;
	c.val.addr = addr;
	return c;
}

static container_t rdc_argdecls(context_t *ctx, int argc, container_t argv[])
{
	container_t c;
	struct llist *list;

	CHECK(argc == 3);
	CHECK(argv[0].type == SP_LIST);
	CHECK(argv[2].type == SP_ADDR);

	list = argv[0].val.list;
	ll_add(list, &argv[2].val.addr);

	c.type = SP_LIST;
	c.val.list = list;
	return c;
}

static container_t rdc_argdecl(context_t *ctx, int argc, container_t argv[])
{
	container_t c;
	struct llist *list;

	CHECK(argc == 1);
	CHECK(argv[0].type == SP_ADDR);

	list = ll_init_gc(sizeof(celladdr_t), ctx->id);
	ll_add(list, &argv[0].val.addr);

	c.type = SP_LIST;
	c.val.list = list;
	return c;
}

static container_t rdc_body(context_t *ctx, int argc,
		container_t argv[])
{
	celladdr_t addr;
	container_t c;
	struct cell cell;
	struct llist *list;
	struct llentry *e;
	int i;

	CHECK(argc == 4);
	CHECK(argv[1].type == SP_LIST);
	CHECK(argv[2].type == SP_LIST);

	cell.type = B_LIST;
	/* firstly the declarations */
	list = argv[1].val.list;
	for (i = list->cnt-1, e = list->first; e != NULL; i--, e = e->next) {
		CHECK(i < MAXLINES);
		cell.action.list.lines[i] = *(celladdr_t *)e->val;
	}
	/* then the lines */
	list = argv[2].val.list;
	for (i = argv[1].val.list->cnt + list->cnt-1, e = list->first;
			e != NULL; i--, e = e->next) {
		CHECK(i < MAXLINES);
		cell.action.list.lines[i] = *(celladdr_t *)e->val;
	}
	cell.action.list.cnt = argv[1].val.list->cnt + argv[2].val.list->cnt;
	ll_free(argv[1].val.list);
	ll_free(argv[2].val.list);
	addr = store_cell(ctx, &cell);

	c.type = SP_ADDR;
	c.val.addr = addr;
	return c;
}

static container_t rdc_mult_line_block(context_t *ctx, int argc,
		container_t argv[])
{
	celladdr_t addr;
	container_t c;
	struct cell cell;
	struct llist *list;
	struct llentry *e;
	int i;

	CHECK(argc == 3);
	CHECK(argv[1].type == SP_LIST);

	cell.type = B_LIST;
	list = argv[1].val.list;
	for (i = list->cnt-1, e = list->first; e != NULL; i--, e = e->next) {
		CHECK(i < MAXLINES);
		cell.action.list.lines[i] = *(celladdr_t *)e->val;
	}
	cell.action.list.cnt = list->cnt;
	ll_free(list);
	addr = store_cell(ctx, &cell);

	c.type = SP_ADDR;
	c.val.addr = addr;
	return c;
}

static container_t rdc_single_line_block(context_t *ctx, int argc,
		container_t argv[])
{
	celladdr_t addr, line_addr;
	container_t c;
	struct cell cell;

	CHECK(argc == 1);
	CHECK(argv[0].type == SP_ADDR);

	line_addr = argv[0].val.addr;

	cell.type = B_LIST;
	cell.action.list.lines[0] = line_addr;
	cell.action.list.cnt = 1;
	addr = store_cell(ctx, &cell);

	c.type = SP_ADDR;
	c.val.addr = addr;
	return c;
}

static container_t rdc_decls(context_t *ctx, int argc, container_t argv[])
{
	container_t c;
	struct llist *list;

	CHECK(argc == 3);
	CHECK(argv[0].type == SP_LIST);
	CHECK(argv[1].type == SP_ADDR);

	list = argv[0].val.list;
	ll_add(list, &argv[1].val.addr);

	c.type = SP_LIST;
	c.val.list = list;
	return c;
}

static container_t rdc_decl(context_t *ctx, int argc, container_t argv[])
{
	container_t c;
	struct llist *list;

	CHECK(argc == 2);
	CHECK(argv[0].type == SP_ADDR);

	list = ll_init_gc(sizeof(celladdr_t), ctx->id);
	ll_add(list, &argv[0].val.addr);

	c.type = SP_LIST;
	c.val.list = list;
	return c;
}

static container_t rdc_lines(context_t *ctx, int argc, container_t argv[])
{
	container_t c;
	struct llist *list;

	CHECK(argc == 2);
	CHECK(argv[0].type == SP_LIST);
	CHECK(argv[1].type == SP_ADDR);

	list = argv[0].val.list;
	ll_add(list, &argv[1].val.addr);

	c.type = SP_LIST;
	c.val.list = list;
	return c;
}

static container_t rdc_line(context_t *ctx, int argc, container_t argv[])
{
	container_t c;
	struct llist *list;

	CHECK(argc == 1);
	CHECK(argv[0].type == SP_ADDR);

	list = ll_init_gc(sizeof(celladdr_t), ctx->id);
	ll_add(list, &argv[0].val.addr);

	c.type = SP_LIST;
	c.val.list = list;
	return c;
}

static container_t rdc_assign(context_t *ctx, int argc, container_t argv[])
{
	celladdr_t addr;
	container_t c;
	struct cell cell;
	int id;

	CHECK(argc == 5);
	CHECK(argv[0].type == SP_SYMBOL);
	CHECK(argv[3].type == SP_ADDR);

	id = get_var_id(ctx, argv[0].val.symbol);
	CHECK(id != -1);

	cell.type = O_ASSIGN;
	cell.action.op.oprnd[0] = id;
	cell.action.op.oprnd[1] = argv[3].val.addr;
	addr = store_cell(ctx, &cell);

	c.type = SP_ADDR;
	c.val.addr = addr;
	return c;
}

static container_t rdc_return(context_t *ctx, int argc, container_t argv[])
{
	celladdr_t addr;
	container_t c;
	struct cell cell;

	CHECK(argc == 3);
	CHECK(argv[1].type == SP_ADDR);

	cell.type = O_RETURN;
	cell.action.op.oprnd[0] = argv[1].val.addr;
	addr = store_cell(ctx, &cell);

	c.type = SP_ADDR;
	c.val.addr = addr;
	return c;
}

static container_t rdc_if(context_t *ctx, int argc, container_t argv[])
{
	celladdr_t addr;
	container_t c;
	struct cell cell;

	CHECK(argc == 5);
	CHECK(argv[2].type == SP_ADDR);
	CHECK(argv[4].type == SP_ADDR);

	cell.type = O_IF;
	cell.action.op.oprnd[0] = argv[2].val.addr;
	cell.action.op.oprnd[1] = argv[4].val.addr;
	addr = store_cell(ctx, &cell);

	c.type = SP_ADDR;
	c.val.addr = addr;
	return c;
}

static container_t rdc_while(context_t *ctx, int argc, container_t argv[])
{
	celladdr_t addr;
	container_t c;
	struct cell cell;

	CHECK(argc == 5);
	CHECK(argv[2].type == SP_ADDR);
	CHECK(argv[4].type == SP_ADDR);

	cell.type = O_WHILE;
	cell.action.op.oprnd[0] = argv[2].val.addr;
	cell.action.op.oprnd[1] = argv[4].val.addr;
	addr = store_cell(ctx, &cell);

	c.type = SP_ADDR;
	c.val.addr = addr;
	return c;
}

static container_t rdc_foreach(context_t *ctx, int argc, container_t argv[])
{
	celladdr_t addr;
	container_t c;
	struct cell cell;
	int id;

	CHECK(argc == 7);
	CHECK(argv[2].type == SP_SYMBOL);
	CHECK(argv[4].type == SP_ADDR);
	CHECK(argv[6].type == SP_ADDR);

	id = get_var_id(ctx, argv[2].val.symbol);
	CHECK(id != -1);

	cell.type = O_FOREACH;
	cell.action.op.oprnd[0] = id;
	cell.action.op.oprnd[1] = argv[4].val.addr;
	cell.action.op.oprnd[2] = argv[6].val.addr;
	addr = store_cell(ctx, &cell);

	c.type = SP_ADDR;
	c.val.addr = addr;
	return c;
}

#define RDC_DECL(t)	rdc_decl_##t
#define RDC_DECL_DEF(t)	static container_t rdc_decl_##t(context_t *ctx,\
					int argc, container_t argv[])\
			{\
				celladdr_t addr;\
				container_t c;\
				struct cell cell;\
				char *s;\
				\
				CHECK(argc == 2);\
				\
				cell.type = V_DECL;\
				cell.action.decl.type = t;\
				cell.action.decl.var_id = ctx->varcnt;\
				addr = store_cell(ctx, &cell);\
				\
				ctx->vars[ctx->varcnt].type = t;\
				s = ctx->vars[ctx->varcnt].symbol;\
				strntermcpy(s, argv[1].val.symbol, MAXNAME+1);\
				ctx->varcnt++;\
				\
				c.type = SP_ADDR;\
				c.val.addr = addr;\
				return c;\
			}

RDC_DECL_DEF(T_INT)
RDC_DECL_DEF(T_FLOAT)
RDC_DECL_DEF(T_STRING)
RDC_DECL_DEF(T_TUPLE)
RDC_DECL_DEF(T_AUTO)

#define RDC_EXPR(t)	rdc_expr_##t
#define RDC_EXPR_DEF(t,i,j,k)	static container_t rdc_expr_##t(context_t *ctx,\
					int argc, container_t argv[])\
			{\
				celladdr_t addr;\
				container_t c;\
				struct cell cell;\
				\
				CHECK(argc == k);\
				CHECK(argv[i].type == SP_ADDR);\
				CHECK(argv[j].type == SP_ADDR);\
				\
				cell.type = t;\
				cell.action.op.oprnd[0] = argv[i].val.addr;\
				cell.action.op.oprnd[1] = argv[j].val.addr;\
				addr = store_cell(ctx, &cell);\
				\
				c.type = SP_ADDR;\
				c.val.addr = addr;\
				return c;\
			}

RDC_EXPR_DEF(O_ADD, 1, 3, 5)
RDC_EXPR_DEF(O_SUB, 1, 3, 5)
RDC_EXPR_DEF(O_MULT, 1, 3, 5)
RDC_EXPR_DEF(O_DIV, 1, 3, 5)
RDC_EXPR_DEF(O_AND, 1, 3, 5)
RDC_EXPR_DEF(O_OR, 1, 3, 5)
RDC_EXPR_DEF(O_EQ, 1, 3, 5)
RDC_EXPR_DEF(O_NEQ, 1, 4, 6)
RDC_EXPR_DEF(O_LEQ, 1, 4, 6)
RDC_EXPR_DEF(O_LT, 1, 3, 5)
RDC_EXPR_DEF(O_GEQ, 1, 4, 6)
RDC_EXPR_DEF(O_GT, 1, 3, 5)

static container_t rdc_expr_minus(context_t *ctx, int argc, container_t argv[])
{
	celladdr_t addr;
	container_t c;
	struct cell cell;

	CHECK(argc == 4);
	CHECK(argv[2].type == SP_ADDR);

	cell.type = O_MINUS;
	cell.action.op.oprnd[0] = argv[2].val.addr;
	addr = store_cell(ctx, &cell);

	c.type = SP_ADDR;
	c.val.addr = addr;
	return c;
}

static container_t rdc_funccall_void(context_t *ctx, int argc,
		container_t argv[])
{
	celladdr_t addr;
	container_t c;
	struct cell cell;
	int id;

	CHECK(argc == 5);
	CHECK(argv[1].type == SP_SYMBOL);

	id = get_func_id(ctx, argv[1].val.symbol);
	CHECK(id != -1);

	cell.type = F_FUNCCALL;
	cell.action.funccall.func_id = id;
	addr = store_cell(ctx, &cell);

	c.type = SP_ADDR;
	c.val.addr = addr;
	return c;
}

static container_t rdc_funccall_args(context_t *ctx, int argc,
		container_t argv[])
{
	celladdr_t addr;
	container_t c;
	struct cell cell;
	struct llist *list;
	struct llentry *e;
	int id, i;

	CHECK(argc == 5 || argc == 6); /* in case of Line, there is a trailing
					* semicolon; in Expr it is not */
	CHECK(argv[1].type == SP_SYMBOL);
	CHECK(argv[3].type == SP_LIST);

	id = get_func_id(ctx, argv[1].val.symbol);
	CHECK(id != -1);

	cell.type = F_FUNCCALL;
	cell.action.funccall.func_id = id;
	list = argv[3].val.list;
	for (i = list->cnt-1, e = list->first; e != NULL; i--, e = e->next) {
		CHECK(i < MAXARGS);
		cell.action.funccall.argv[i] = *(celladdr_t *)e->val;
	}
	ll_free(list);
	addr = store_cell(ctx, &cell);

	c.type = SP_ADDR;
	c.val.addr = addr;
	return c;
}

static container_t rdc_args(context_t *ctx, int argc, container_t argv[])
{
	container_t c;
	struct llist *list;

	CHECK(argc == 3);
	CHECK(argv[0].type == SP_LIST);
	CHECK(argv[2].type == SP_ADDR);

	list = argv[0].val.list;
	ll_add(list, &argv[2].val.addr);

	c.type = SP_LIST;
	c.val.list = list;
	return c;
}

static container_t rdc_arg(context_t *ctx, int argc, container_t argv[])
{
	container_t c;
	struct llist *list;

	CHECK(argc == 1);
	CHECK(argv[0].type == SP_ADDR);

	list = ll_init_gc(sizeof(celladdr_t), ctx->id);
	ll_add(list, &argv[0].val.addr);

	c.type = SP_LIST;
	c.val.list = list;
	return c;
}

static container_t rdc_symbol(context_t *ctx, int argc, container_t argv[])
{
	celladdr_t addr;
	container_t c;
	struct cell cell;

	CHECK(argc == 1);
	CHECK(argv[0].type == SP_SYMBOL);

	cell.type = V_VAR;
	cell.action.var_id = get_var_id(ctx, argv[0].val.symbol);
	addr = store_cell(ctx, &cell);

	c.type = SP_ADDR;
	c.val.addr = addr;
	return c;
}

static container_t rdc_string(context_t *ctx, int argc, container_t argv[])
{
	celladdr_t addr;
	container_t c;
	struct cell cell;
	int len;
	char *str;

	CHECK(argc == 1);
	CHECK(argv[0].type == SP_C_STRING);

	len = strlen(argv[0].val.vstring);
	str = gmalloc(len + 1, ctx->id);
	strcpy(str, argv[0].val.vstring);

	cell.type = V_CONST;
	cell.action.con.type = T_STRING;
	cell.action.con.val.vstring.len = len;
	cell.action.con.val.vstring.str = str;
	addr = store_cell(ctx, &cell);

	c.type = SP_ADDR;
	c.val.addr = addr;
	return c;
}

static container_t rdc_float(context_t *ctx, int argc, container_t argv[])
{
	celladdr_t addr;
	container_t c;
	struct cell cell;

	CHECK(argc == 1);
	CHECK(argv[0].type == SP_C_FLOAT);

	cell.type = V_CONST;
	cell.action.con.type = T_FLOAT;
	cell.action.con.val.vfloat = argv[0].val.vfloat;
	addr = store_cell(ctx, &cell);

	c.type = SP_ADDR;
	c.val.addr = addr;
	return c;
}

static container_t rdc_int(context_t *ctx, int argc, container_t argv[])
{
	celladdr_t addr;
	container_t c;
	struct cell cell;

	CHECK(argc == 1);
	CHECK(argv[0].type == SP_C_INT);

	cell.type = V_CONST;
	cell.action.con.type = T_INT;
	cell.action.con.val.vint = argv[0].val.vint;
	addr = store_cell(ctx, &cell);

	c.type = SP_ADDR;
	c.val.addr = addr;
	return c;
}

/* }}} */

/* Scanner. {{{ */

#define IS_BLANK(c)	((c) == ' ' || (c) == '\t'\
			|| (c) == '\r' || (c) == '\n')
#define IS_LOWER(c)	((c) >= 'a' && (c) <= 'z')
#define IS_UPPER(c)	((c) >= 'A' && (c) <= 'Z')
#define IS_SYMBOL(c)	((c) == '_')
#define IS_ALPHA(c)	(IS_LOWER(c) || IS_UPPER(c) || IS_SYMBOL(c))
#define IS_DIGIT(c)	((c) >= '0' && (c) <= '9')
#define KEYWORDS_SIZE	((ssize_t)((sizeof keywords) / (sizeof keywords[0])))

const char *keywords[] = {
	"int", "float", "string", "tuple", "auto", "procedure", "begin", "do",
	"end", "returns", "return", "if", "while", "foreach", "in", "and", "or"
};

struct token {
	int alph_ix;
	container_t val;
};

static float power(float base, int exp)
{
	float r;

	if (exp < 0) {
		exp *= -1;
		base = 1 / base;
	}
	r = 1.0;
	for (; exp > 0; exp--)
		r *= base;
	return r;
}

/* Goto- and action table (generated by nfa.c). {{{ */
/* BEGIN OF GENERATED CODE -- DO NOT EDIT */

static const struct rule {
	const char * const v;
	const char * const x;
	container_t (*func)(context_t *ctx, int argc, container_t argv[]);
} rules[46] = {
	{ "Start", "procedure symbol ( Argdecllist ) Body", rdc_procedure_args },
	{ "Start", "procedure symbol ( ) Body", rdc_procedure_void },
	{ "Argdecllist", "Argdecllist , Decl", rdc_argdecls },
	{ "Argdecllist", "Decl", rdc_argdecl },
	{ "Body", "begin Decllist Linelist end", rdc_body },
	{ "Decllist", "Decllist Decl ;", rdc_decls },
	{ "Decllist", "Decl ;", rdc_decl },
	{ "Decl", "int symbol", RDC_DECL(T_INT) },
	{ "Decl", "float symbol", RDC_DECL(T_FLOAT) },
	{ "Decl", "string symbol", RDC_DECL(T_STRING) },
	{ "Decl", "tuple symbol", RDC_DECL(T_TUPLE) },
	{ "Decl", "auto symbol", RDC_DECL(T_AUTO) },
	{ "Block", "Line", rdc_single_line_block },
	{ "Block", "do Linelist end", rdc_mult_line_block },
	{ "Linelist", "Linelist Line", rdc_lines },
	{ "Linelist", "Line", rdc_line },
	{ "Line", "! symbol ( Arglist ) ;", rdc_funccall_args },
	{ "Line", "! symbol ( ) ;", rdc_funccall_void },
	{ "Line", "symbol : = Expr ;", rdc_assign },
	{ "Line", "return Expr ;", rdc_return },
	{ "Line", "if ( Expr ) Block", rdc_if },
	{ "Line", "while ( Expr ) Block", rdc_while },
	{ "Line", "foreach ( symbol in Expr ) Block", rdc_foreach },
	{ "Expr", "( Expr )", RDC_FWD(2, 3) },
	{ "Expr", "( Expr + Expr )", RDC_EXPR(O_ADD) },
	{ "Expr", "( Expr - Expr )", RDC_EXPR(O_SUB) },
	{ "Expr", "( - Expr )", rdc_expr_minus },
	{ "Expr", "( Expr * Expr )", RDC_EXPR(O_MULT) },
	{ "Expr", "( Expr / Expr )", RDC_EXPR(O_DIV) },
	{ "Expr", "( Expr or Expr )", RDC_EXPR(O_OR) },
	{ "Expr", "( Expr and Expr )", RDC_EXPR(O_AND) },
	{ "Expr", "( Expr = Expr )", RDC_EXPR(O_EQ) },
	{ "Expr", "( Expr ! = Expr )", RDC_EXPR(O_NEQ) },
	{ "Expr", "( Expr < = Expr )", RDC_EXPR(O_LEQ) },
	{ "Expr", "( Expr < Expr )", RDC_EXPR(O_LT) },
	{ "Expr", "( Expr > Expr )", RDC_EXPR(O_GT) },
	{ "Expr", "( Expr > = Expr )", RDC_EXPR(O_GEQ) },
	{ "Expr", "! symbol ( Arglist )", rdc_funccall_args },
	{ "Expr", "! symbol ( )", rdc_funccall_void },
	{ "Expr", "symbol", rdc_symbol },
	{ "Expr", "intval", rdc_int },
	{ "Expr", "floatval", rdc_float },
	{ "Expr", "stringval", rdc_string },
	{ "Expr", "tupleval", NULL },
	{ "Arglist", "Arglist , Expr", rdc_args },
	{ "Arglist", "Expr", rdc_arg }
};

static int rulelen(const struct rule *rl)
{
	const char *s;
	int i;

	i = 0;
	for (s = rl->x; *s; s++)
		if (*s==' '||*s=='\t'||*s=='\r'||*s=='\n')
			i++;
	return i+1;
}

static const struct action {
	enum { ERROR = -1, SHIFT, REDUCE, ACCEPT } action;
	int ruleix;
} action_table[126] = {
	{ SHIFT, -1 },
	{ SHIFT, -1 },
	{ SHIFT, -1 },
	{ SHIFT, -1 },
	{ SHIFT, -1 },
	{ SHIFT, -1 },
	{ ACCEPT, 0 },
	{ SHIFT, -1 },
	{ SHIFT, -1 },
	{ REDUCE, 6 },
	{ SHIFT, -1 },
	{ SHIFT, -1 },
	{ SHIFT, -1 },
	{ SHIFT, -1 },
	{ REDUCE, 39 },
	{ SHIFT, -1 },
	{ SHIFT, -1 },
	{ SHIFT, -1 },
	{ SHIFT, -1 },
	{ REDUCE, 38 },
	{ SHIFT, -1 },
	{ REDUCE, 37 },
	{ SHIFT, -1 },
	{ REDUCE, 44 },
	{ REDUCE, 40 },
	{ REDUCE, 41 },
	{ REDUCE, 42 },
	{ REDUCE, 43 },
	{ REDUCE, 45 },
	{ SHIFT, -1 },
	{ REDUCE, 23 },
	{ SHIFT, -1 },
	{ SHIFT, -1 },
	{ SHIFT, -1 },
	{ REDUCE, 32 },
	{ SHIFT, -1 },
	{ SHIFT, -1 },
	{ REDUCE, 31 },
	{ SHIFT, -1 },
	{ SHIFT, -1 },
	{ REDUCE, 24 },
	{ SHIFT, -1 },
	{ SHIFT, -1 },
	{ REDUCE, 25 },
	{ SHIFT, -1 },
	{ SHIFT, -1 },
	{ REDUCE, 27 },
	{ SHIFT, -1 },
	{ SHIFT, -1 },
	{ REDUCE, 28 },
	{ SHIFT, -1 },
	{ SHIFT, -1 },
	{ REDUCE, 29 },
	{ SHIFT, -1 },
	{ SHIFT, -1 },
	{ REDUCE, 30 },
	{ SHIFT, -1 },
	{ SHIFT, -1 },
	{ SHIFT, -1 },
	{ REDUCE, 33 },
	{ SHIFT, -1 },
	{ REDUCE, 34 },
	{ SHIFT, -1 },
	{ SHIFT, -1 },
	{ SHIFT, -1 },
	{ REDUCE, 36 },
	{ SHIFT, -1 },
	{ REDUCE, 35 },
	{ SHIFT, -1 },
	{ SHIFT, -1 },
	{ REDUCE, 26 },
	{ SHIFT, -1 },
	{ REDUCE, 18 },
	{ SHIFT, -1 },
	{ REDUCE, 5 },
	{ SHIFT, -1 },
	{ REDUCE, 4 },
	{ REDUCE, 14 },
	{ SHIFT, -1 },
	{ SHIFT, -1 },
	{ SHIFT, -1 },
	{ SHIFT, -1 },
	{ REDUCE, 17 },
	{ SHIFT, -1 },
	{ SHIFT, -1 },
	{ REDUCE, 16 },
	{ SHIFT, -1 },
	{ SHIFT, -1 },
	{ REDUCE, 19 },
	{ SHIFT, -1 },
	{ SHIFT, -1 },
	{ SHIFT, -1 },
	{ SHIFT, -1 },
	{ REDUCE, 20 },
	{ REDUCE, 12 },
	{ SHIFT, -1 },
	{ SHIFT, -1 },
	{ REDUCE, 13 },
	{ SHIFT, -1 },
	{ SHIFT, -1 },
	{ SHIFT, -1 },
	{ SHIFT, -1 },
	{ REDUCE, 21 },
	{ SHIFT, -1 },
	{ SHIFT, -1 },
	{ SHIFT, -1 },
	{ SHIFT, -1 },
	{ SHIFT, -1 },
	{ SHIFT, -1 },
	{ REDUCE, 22 },
	{ REDUCE, 15 },
	{ SHIFT, -1 },
	{ REDUCE, 7 },
	{ SHIFT, -1 },
	{ REDUCE, 8 },
	{ SHIFT, -1 },
	{ REDUCE, 9 },
	{ SHIFT, -1 },
	{ REDUCE, 10 },
	{ SHIFT, -1 },
	{ REDUCE, 11 },
	{ SHIFT, -1 },
	{ REDUCE, 2 },
	{ SHIFT, -1 },
	{ ACCEPT, 1 },
	{ REDUCE, 3 }
};

#define ALPHABET_SIZE	((ssize_t)((sizeof alphabet) / sizeof(alphabet[0])))

const char *alphabet[44] = {
	"Start",
	"procedure",
	"symbol",
	"(",
	"Argdecllist",
	")",
	"Body",
	",",
	"Decl",
	"begin",
	"Decllist",
	"Linelist",
	"end",
	";",
	"int",
	"float",
	"string",
	"tuple",
	"auto",
	"Block",
	"Line",
	"do",
	"!",
	"Arglist",
	":",
	"=",
	"Expr",
	"return",
	"if",
	"while",
	"foreach",
	"in",
	"+",
	"-",
	"*",
	"/",
	"or",
	"and",
	"<",
	">",
	"intval",
	"floatval",
	"stringval",
	"tupleval"
};

const short goto_table[126][43] = {
	{ -1, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, 4, 123, -1, -1, 125, -1, -1, -1, -1, -1, 111, 113, 115, 117, 119, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, 5, -1, 121, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, 6, -1, -1, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, 8, -1, 10, -1, -1, -1, 111, 113, 115, 117, 119, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, 11, -1, -1, -1, -1, -1, 73, -1, -1, 75, -1, -1, 111, 113, 115, 117, 119, -1, 110, -1, 78, -1, -1, -1, -1, 86, 89, 98, 103, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 12, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 13, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 16, -1, -1, -1, 71, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 24, 25, 26 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 16, -1, -1, -1, 29, -1, -1, -1, -1, -1, -1, 68, -1, -1, -1, -1, -1, -1, 24, 25, 26 },
	{ -1, -1, 17, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, 18, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, 14, 15, -1, 19, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 16, 20, -1, -1, 28, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 24, 25, 26 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, 21, -1, 22, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 16, -1, -1, -1, 23, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 24, 25, 26 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, 30, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 31, -1, -1, 35, -1, -1, -1, -1, -1, -1, 38, 41, 44, 47, 50, 53, 56, 62, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 32, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 16, -1, -1, -1, 33, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 24, 25, 26 },
	{ -1, -1, -1, -1, -1, 34, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 16, -1, -1, -1, 36, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 24, 25, 26 },
	{ -1, -1, -1, -1, -1, 37, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 16, -1, -1, -1, 39, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 24, 25, 26 },
	{ -1, -1, -1, -1, -1, 40, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 16, -1, -1, -1, 42, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 24, 25, 26 },
	{ -1, -1, -1, -1, -1, 43, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 16, -1, -1, -1, 45, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 24, 25, 26 },
	{ -1, -1, -1, -1, -1, 46, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 16, -1, -1, -1, 48, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 24, 25, 26 },
	{ -1, -1, -1, -1, -1, 49, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 16, -1, -1, -1, 51, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 24, 25, 26 },
	{ -1, -1, -1, -1, -1, 52, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 16, -1, -1, -1, 54, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 24, 25, 26 },
	{ -1, -1, -1, -1, -1, 55, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 16, -1, -1, 57, 60, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 24, 25, 26 },
	{ -1, -1, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 16, -1, -1, -1, 58, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 24, 25, 26 },
	{ -1, -1, -1, -1, -1, 59, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, 61, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 16, -1, -1, 63, 66, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 24, 25, 26 },
	{ -1, -1, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 16, -1, -1, -1, 64, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 24, 25, 26 },
	{ -1, -1, -1, -1, -1, 65, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, 67, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 16, -1, -1, -1, 69, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 24, 25, 26 },
	{ -1, -1, -1, -1, -1, 70, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 72, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 74, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, 76, -1, -1, -1, -1, -1, -1, -1, 77, -1, 78, -1, -1, -1, -1, 86, 89, 98, 103, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, 79, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, 80, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, 14, 15, -1, 81, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 16, 83, -1, -1, 28, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 24, 25, 26 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 82, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, 84, -1, 22, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 85, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 16, -1, -1, -1, 87, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 24, 25, 26 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 88, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, 90, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 16, -1, -1, -1, 91, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 24, 25, 26 },
	{ -1, -1, -1, -1, -1, 92, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 93, 94, 95, 78, -1, -1, -1, -1, 86, 89, 98, 103, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, 11, -1, -1, -1, -1, -1, -1, -1, -1, 96, -1, -1, -1, -1, -1, -1, -1, -1, 110, -1, 78, -1, -1, -1, -1, 86, 89, 98, 103, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, 97, -1, -1, -1, -1, -1, -1, -1, 77, -1, 78, -1, -1, -1, -1, 86, 89, 98, 103, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, 99, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 16, -1, -1, -1, 100, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 24, 25, 26 },
	{ -1, -1, -1, -1, -1, 101, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 102, 94, 95, 78, -1, -1, -1, -1, 86, 89, 98, 103, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, 104, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, 105, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 106, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 16, -1, -1, -1, 107, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 24, 25, 26 },
	{ -1, -1, -1, -1, -1, 108, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 109, 94, 95, 78, -1, -1, -1, -1, 86, 89, 98, 103, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, 112, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, 114, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, 116, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, 118, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, 120, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, 122, -1, -1, -1, -1, -1, 111, 113, 115, 117, 119, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, 124, -1, -1, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
};

/* END OF GENERATED CODE -- DO NOT EDIT */

/* }}} */

static int alph_index(const char *str)
{
	int i;

	for (i = 0; i < ALPHABET_SIZE; i++)
		if (!strcmp(str, alphabet[i]))
			return i;
	return -1;
}

static struct token next_token(const char **str, mid_t id)
{
	struct token token;
	const char *s;

	s = *str;

scan:
	while (IS_BLANK(*s))
		s++;

	if (*s == '#') {
		while (*s && *s != '\n')
			s++;
		goto scan;
	}

	if (!*s)
		goto error;

	if (IS_ALPHA(*s)) { /* symbol or a keyword */
		const char *t;
		int i;

		for (t = s+1; IS_ALPHA(*t) || IS_DIGIT(*t); t++)
			;

		for (i = 0; i < KEYWORDS_SIZE; i++)
			if (!strncasecmp(s, keywords[i], t-s)
					&& (int)strlen(keywords[i]) == t-s)
				break;
		if (i < KEYWORDS_SIZE) { /* keyword */
			token.alph_ix = alph_index(keywords[i]);
			token.val = EMPTY_CONTAINER;
		} else { /* symbol */
			token.val.val.symbol = gmalloc((t-s + 1) * sizeof(char),
					id);
			strncpy(token.val.val.symbol, s, t - s);
			token.val.val.symbol[t-s] = '\0';
			token.val.type = SP_SYMBOL;
			token.alph_ix = alph_index("symbol");
		}
		s = t;
	} else if (*s == '\"') { /* string constant */
		const char *t;

		for (t = s+1; *t && *t != '\"'; t++)
			;
		s++;
		if (*t != '\"')
			goto error;
		token.val.val.vstring = gmalloc(t - s + 1, id);
		strncpy(token.val.val.vstring, s, t - s);
		token.val.val.vstring[t-s] = '\0';
		token.val.type = SP_C_STRING;
		token.alph_ix = alph_index("stringval");
		s = t+1;
	} else if (IS_DIGIT(*s)) {
		const char *t, *dot;

		t = s+1;
		dot = NULL;
		for (t = s+1; IS_DIGIT(*t) || *t == '.'; t++) {
			if (*t == '.') {
				if (dot != NULL)
					goto error;
				dot = t;
			}
		}
		if (!dot) { /* integer */
			int i;

			i = 0;
			for (; s < t; s++)
				i += power(10.0f, t - s - 1) * ((*s) - '0');
			token.val.val.vint = i;
			token.val.type = SP_C_INT;
			token.alph_ix = alph_index("intval");
		} else { /* float */
			float f;

			f = 0.0;
			for (; s < dot; s++)
				f += power(10.0f, dot - s - 1) * ((*s) - '0');
			for (s = dot+1; s < t; s++)
				f += power(10.0f, dot - s) * ((*s) - '0');
			token.val.val.vfloat = f;
			token.val.type = SP_C_FLOAT;
			token.alph_ix = alph_index("floatval");
		}
		s = t;
	} else { /* rest: +, (, ), - etc. */
		char buf[2];

		buf[0] = *s;
		buf[1] = '\0';
		token.alph_ix = alph_index(buf);
		token.val.type = SP_PARSE_ERROR;
		s++;
	}
	*str = s;
	return token;

error:
	token.alph_ix = ERROR;
	token.val = ERROR_CONTAINER;
	return token;
}

/* }}} */

/* Parser. {{{ */

#define STACKSIZE	512

struct repl {
	const char *sym;
	container_t val;
};

struct stack {
	union {
		short itemset;
		struct repl repl;
	} elems[STACKSIZE];
	int size;
};

static void push_itemset(struct stack *stack, short i)
{
	assert(stack->size < STACKSIZE);
	stack->elems[stack->size++].itemset = i;
}

static void push_repl(struct stack *stack, const char *s, container_t val)
{
	assert(stack->size < STACKSIZE);
	stack->elems[stack->size].repl.sym = s;
	stack->elems[stack->size].repl.val = val;
	stack->size++;
}

static short top_itemset(struct stack *stack)
{
	assert(stack->size > 0);
	return stack->elems[stack->size-1].itemset;
}

static struct repl top_repl(struct stack *stack)
{
	assert(stack->size > 0);
	return stack->elems[stack->size-1].repl;
}

static void pop(struct stack *stack)
{
	assert(stack->size > 0);
	stack->size--;
}

#ifdef DEBUG
static void print_stack(struct stack *stack)
{
	int i;

	printf("Stack: ");
	for (i = 1; i < stack->size; i+=2)
		printf("%s ", stack->elems[i].repl.sym);
	printf("\n");
}
#endif

static bool shift(struct stack *stack, context_t *ctx, const char **str_ptr)
{
	struct token tok;
	short i, j;

	i = top_itemset(stack);
	tok = next_token(str_ptr, ctx->id);
	if (tok.alph_ix == ERROR)
		return false;

	if ((j = goto_table[i][tok.alph_ix]) == ERROR)
		return false;
	push_repl(stack, alphabet[tok.alph_ix], tok.val);
	push_itemset(stack, j);
	return true;
}

static bool reduce(struct stack *stack, context_t *ctx, const struct rule *rl)
{
	int k, argc;
	short i, j;
	container_t val, *argv;

	argc = rulelen(rl);
	argv = xmalloc(argc * sizeof(container_t));
	for (k = argc; --k >= 0; ) {
		pop(stack);
		argv[k] = top_repl(stack).val;
		pop(stack);
	}

	if (rl->func != NULL)
		val = rl->func(ctx, argc, argv);
	else
		val = EMPTY_CONTAINER;
	free(argv);

	if (val.type == SP_PARSE_ERROR)
		return false;

	i = top_itemset(stack);
	if ((j = goto_table[i][alph_index(rl->v)]) == ERROR)
		return false;
	push_repl(stack, rl->v, val);
	push_itemset(stack, j);
	return true;
}

static bool accept(struct stack *stack, context_t *ctx, const struct rule *rl)
{
	int k, argc;
	container_t val, *argv;

	argc = rulelen(rl);
	argv = xmalloc(argc * sizeof(container_t));
	for (k = argc; --k >= 0; ) {
		pop(stack);
		argv[k] = top_repl(stack).val;
		pop(stack);
	}

	if (rl->func != NULL)
		val = rl->func(ctx, argc, argv);
	else
		val = EMPTY_CONTAINER;
	free(argv);

	if (val.type == SP_PARSE_ERROR)
		return false;
	return true;
}

static bool parse(context_t *ctx, const char *str)
{
	struct stack stack;

	stack.size = 0;
	ctx->size = 0;
	ctx->varcnt = 0;
	ctx->id = gnew();

	push_itemset(&stack, 0);
	for (;;) {
		short i;
		const struct action *a;

#ifdef DEBUG
		print_stack(&stack);
#endif
		i = top_itemset(&stack);
		a = &action_table[i];
		if (a->action == ACCEPT) {
#ifdef DEBUG
			printf("ACCEPT %s -> %s\n", rules[a->ruleix].v,
					rules[a->ruleix].x);
#endif
			return accept(&stack, ctx, &rules[a->ruleix]);
		} else if (a->action == ERROR) {
#ifdef DEBUG
			printf("ERROR\n");
#endif
			return false;
		} else if (a->action == SHIFT) {
#ifdef DEBUG
			printf("SHIFT\n");
#endif
			if (!shift(&stack, ctx, &str))
				return false;
		} else if (a->action == REDUCE) {
#ifdef DEBUG
			printf("REDUCE %s -> %s\n", rules[a->ruleix].v,
					rules[a->ruleix].x);
#endif
			if (!reduce(&stack, ctx, &rules[a->ruleix]))
				return false;
		}
	}
}

/* }}} */

/* Controlling functions. {{{ */

bool sp_compile(const char *prog)
{
	context_t *ctx;

	ctx = xmalloc(sizeof(context_t));
	if (!parse(ctx, prog)) {
		gc(ctx->id);
		free(ctx);
		ERR(E_SP_PARSING_FAILED);
		return false;
	}
#ifdef DEBUG
	print_program(ctx);
#endif
	if (!generate_byte_code(ctx)) {
		gc(ctx->id);
		free(ctx);
		ERR(E_SP_GENERATING_FAILED);
		return false;
	}
	gc(ctx->id);
	free(ctx);
	return true;
}

bool sp_vrun(const char *name, int argc, struct value *argv[],
		struct value *retval)
{
	struct sp_value argv2[argc], retval2;
	int i;

	for (i = 0; i < argc; i++) {
		switch (argv[i]->domain) {
			case INT:
				argv2[i].type = T_INT;
				argv2[i].val.vint = argv[i]->ptr.vint;
				break;
			case FLOAT:
				argv2[i].type = T_FLOAT;
				argv2[i].val.vfloat = argv[i]->ptr.vfloat;
				break;
			case STRING:
				argv2[i].type = T_STRING;
				argv2[i].val.vstring.str = argv[i]->ptr.pstring;
				argv2[i].val.vstring.len
					= strlen(argv[i]->ptr.pstring);
				break;
			case BYTES:
				assert(false);
				return false;
			default:
				assert(false);
				break;
		}
	}
	if (!interpret_byte_code(name, i, argv2, &retval2))
		return false;
	switch (retval2.type) {
		case T_INT:
			retval->domain = INT;
			retval->ptr.vint = retval2.val.vint;
			return true;
		case T_FLOAT:
			retval->domain = FLOAT;
			retval->ptr.vfloat = retval2.val.vfloat;
			return true;
		case T_STRING:
			retval->domain = STRING;
			retval->ptr.pstring = retval2.val.vstring.str;
			return true;
		case T_TUPLE:
			return false;
		default:
			return false;
	}
}

static bool copy_retval(const char *fmt, int i, va_list ap,
		struct sp_value *retval)
{
	struct db_val *vals;
	int cnt, j;

	if (!*fmt) {
		ERR(E_SP_TOO_MANY_RETURN_POINTERS);
		return false;
	}

	switch (retval->type) {
		case T_INT:
			if (fmt[i] == 'd') {
				int *retptr;

				retptr = va_arg(ap, int *);
				*retptr = retval->val.vint;
				return true;
			} else if (fmt[i] == 'a') {
				struct db_val *retptr;

				retptr = va_arg(ap, struct db_val *);
				retptr->domain = DB_INT;
				retptr->val.vint = retval->val.vint;
				return true;
			} else {
				ERR(E_SP_INVALID_RETURN_POINTER);
				return false;
			}
		case T_FLOAT:
			if (fmt[i] == 'f') {
				float *retptr;

				retptr = va_arg(ap, float *);
				*retptr = retval->val.vfloat;
				return true;
			} else if (fmt[i] == 'a') {
				struct db_val *retptr;

				retptr = va_arg(ap, struct db_val *);
				retptr->domain = DB_FLOAT;
				retptr->val.vfloat = retval->val.vfloat;
				return true;
			} else {
				ERR(E_SP_INVALID_RETURN_POINTER);
				return false;
			}
		case T_STRING:
			if (fmt[i] == 's') {
				char **retptr;

				retptr = va_arg(ap, char **);
				*retptr = cat(1, retval->val.vstring.str);
				return true;
			} else if (fmt[i] == 'a') {
				struct db_val *retptr;
				char *str;

				retptr = va_arg(ap, struct db_val *);
				retptr->domain = DB_STRING;
				retptr->size = retval->val.vstring.len;
				str = cat(1, retval->val.vstring.str);
				retptr->val.pstring = str;
				return true;
			} else {
				ERR(E_SP_INVALID_RETURN_POINTER);
				return false;
			}
		case T_TUPLE:
			cnt = retval->val.vtuple.cnt;
			vals = retval->val.vtuple.vals;
			for (j = 0; j < cnt; j++) {
				if (!copy_retval(fmt, i+j, ap, retval)) {
					ERR(E_SP_INVALID_RETURN_POINTER);
					return false;
				}
			}
			return true;
		default:
			return false;
	}
}

bool sp_run(const char *name, const char *fmt, ...)
{
	struct sp_value argv[MAXARGS], retval;
	va_list ap;
	int i;

	va_start(ap, fmt);
	for (i = 0; fmt[i] && fmt[i] != '='; i++) {
		switch (fmt[i]) {
			case 'd':
				argv[i].type = T_INT;
				argv[i].val.vint = va_arg(ap, int);
				break;
			case 'f':
				argv[i].type = T_FLOAT;
				argv[i].val.vfloat = (float)va_arg(ap, double);
				break;
			case 's':
				argv[i].type = T_STRING;
				argv[i].val.vstring.str = va_arg(ap, char *);
				argv[i].val.vstring.len
					= strlen(argv[i].val.vstring.str);
				break;
			default:
				assert(false);
				break;
		}
	}
	if (!interpret_byte_code(name, i, argv, &retval)) {
		va_end(ap);
		return false;
	}
	if (fmt[i] == '=' && !copy_retval(fmt, i+1, ap, &retval)) {
		va_end(ap);
		return false;
	}
	va_end(ap);
	return true;
}

/* }}} */

