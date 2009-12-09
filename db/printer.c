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

#include "printer.h"
#include "block.h"
#include "err.h"
#include "parser.h"
#include "rlalg.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define BUFSIZE		4096

#define LEFT		-1
#define	CENTER		0
#define RIGHT		1

#define STRING_LEN	25
#define INT_LEN		12
#define LONG_LEN	17
#define FLOAT_LEN	12
#define DOUBLE_LEN	17

static int field_size(struct xattr *attr)
{
	switch (attr->at_sattr->at_domain) {
		case STRING:	return STRING_LEN;
		case INT:	return INT_LEN;
		case UINT:	return INT_LEN;
		case LONG:	return LONG_LEN;
		case ULONG:	return LONG_LEN;
		case FLOAT:	return FLOAT_LEN;
		case DOUBLE:	return DOUBLE_LEN;
		default:	return -1;
	}
}

static void print_line(FILE *out, struct xrel *rl)
{
	int i;

	for (i = 0; i < rl->rl_atcnt; i++) {
		struct xattr *attr;
		int j, size;
		
		fprintf(out, "+");
		attr = rl->rl_attrs[i];
		size = field_size(attr);
		for (j = 0; j < size+2; j++)
			fprintf(out, "-");
	}
	fprintf(out, "+");
	fprintf(out, "\n");
}

static void print_field(FILE *out, char *s, int size, int align)
{
	int len;

	fprintf(out, " ");
	if ((len = strlen(s)) <= size) {
		int i;
		len = size - len;

		if (align == RIGHT)
			for (i = 0; i < len; i++)
				fprintf(out, " ");
		if (align == CENTER)
			for (i = 0; i < len/2; i++)
				fprintf(out, " ");

		fprintf(out, "%s", s);

		if (align == CENTER)
			for (i = len/2; i < len; i++)
				fprintf(out, " ");
		if (align == LEFT)
			for (i = 0; i < len; i++)
				fprintf(out, " ");

	} else {
		char buf[size+1];
		int first, second;

		first = (size % 2 == 0) ? size / 2 - 1 : size / 2;
		second = (size % 2 == 0) ? size / 2 - 1 : size / 2 - 1;

		strncpy(buf, s, first);
		strcpy(buf + first, "..");
		strcpy(buf + first+2, s + len - second);
		fprintf(out, "%s", buf);
	}
	fprintf(out, " ");
}

static void print_attrname(FILE *out, struct xattr *attr)
{
	struct srel *srl;
	struct sattr *sattr;
	int offset;
	static char buf[RL_NAME_MAX + 1 + AT_NAME_MAX + 1];

	srl = attr->at_srl;
	sattr = attr->at_sattr;
	offset = strlen(srl->rl_header.hd_name);

	assert(strlen(srl->rl_header.hd_name) <= RL_NAME_MAX);
	assert(strlen(sattr->at_name) <= AT_NAME_MAX);

	strcpy(buf, srl->rl_header.hd_name);
	strcpy(buf + offset, ".");
	strcpy(buf + offset + 1, sattr->at_name);
	print_field(out, buf, field_size(attr), CENTER);
}

static void print_attrdesc(FILE *out, struct xattr *attr)
{
	char buf[1024];

	switch (attr->at_sattr->at_domain) {
		case STRING:
			sprintf(buf, "(string (%d))",
					(int)attr->at_sattr->at_size);
			break;
		case BYTES:
			sprintf(buf, "(binary (%d))",
					(int)attr->at_sattr->at_size);
			break;
		case INT:
			sprintf(buf, "(int)");
			break;
		case UINT:
			sprintf(buf, "(uint)");
			break;
		case LONG:
			sprintf(buf, "(long)");
			break;
		case ULONG:
			sprintf(buf, "(ulong)");
			break;
		case FLOAT:
			sprintf(buf, "(float)");
			break;
		case DOUBLE:
			sprintf(buf, "(double)");
			break;
	}
	print_field(out, buf, field_size(attr), CENTER);
}

static void print_attrval(FILE *out, struct xattr *attr, const char *tuple)
{
	const char *val;
	char buf[1024];
	int size;

	size = field_size(attr);
	val = tuple + attr->at_offset;

	switch (attr->at_sattr->at_domain) {
		case STRING:
			strncpy(buf, val, attr->at_sattr->at_size);
			print_field(out, buf, size, LEFT);
			break;
		case INT:
			sprintf(buf, DB_INT_FMT_NICE, *(db_int_t *)val);
			print_field(out, buf, size, RIGHT);
			break;
		case UINT:
			sprintf(buf, DB_UINT_FMT_NICE, *(db_uint_t *)val);
			print_field(out, buf, size, RIGHT);
			break;
		case LONG:
			sprintf(buf, DB_LONG_FMT_NICE, *(db_long_t *)val);
			print_field(out, buf, size, RIGHT);
			break;
		case ULONG:
			sprintf(buf, DB_ULONG_FMT_NICE, *(db_ulong_t *)val);
			print_field(out, buf, size, RIGHT);
			break;
		case FLOAT:
			sprintf(buf, DB_FLOAT_FMT_NICE, *(db_float_t *)val);
			print_field(out, buf, size, RIGHT);
			break;
		case DOUBLE:
			sprintf(buf, DB_DOUBLE_FMT_NICE, *(db_double_t *)val);
			print_field(out, buf, size, RIGHT);
			break;
		case BYTES:
			sprintf(buf, "(binary)");
			print_field(out, buf, size, LEFT);
			break;
		default:
			sprintf(buf, "(unknown)");
			print_field(out, buf, size, LEFT);
	}
}

tpcnt_t xrel_fprint(FILE *out, struct xrel *rl)
{
	struct xrel_iter *iter;
	const char *tuple;
	tpcnt_t i;
	int j;

	assert(rl != NULL);

	iter = rl->rl_iterator(rl);
	assert(iter != NULL);

	print_line(out, rl);

	fprintf(out, "|");
	for (j = 0; j < rl->rl_atcnt; j++) {
		print_attrname(out, rl->rl_attrs[j]);
		fprintf(out, "|");
	}
	fprintf(out, "\n");

	fprintf(out, "|");
	for (j = 0; j < rl->rl_atcnt; j++) {
		print_attrdesc(out, rl->rl_attrs[j]);
		fprintf(out, "|");
	}
	fprintf(out, "\n");

	print_line(out, rl);

	for (i = 0; (tuple = iter->it_next(iter)) != NULL; i++) {
		fprintf(out, "|");
		for (j = 0; j < rl->rl_atcnt; j++) {
			print_attrval(out, rl->rl_attrs[j], tuple);
			fprintf(out, "|");
		}
		fprintf(out, "\n");
	}
	assert(iter->it_next(iter) == NULL);
	xrel_iter_free(iter);
	print_line(out, rl);
	return i;
}

tpcnt_t xrel_print(struct xrel *rl)
{
	return xrel_fprint(stdout, rl);
}

