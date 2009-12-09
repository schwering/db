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
 * A short example for how to use the dingsbums interface db.h.
 *
 * The example only shows how to handle queries that create relations (in
 * contrast to insertion/deletion/update, aggregate functions or data
 * definition statements).
 * See the db.h header for the function definitions and their documentation.
 *
 * The main() calls test1() and test2(). Both do the same thing:
 * Iterate over a relation and print each tuple to stdout (in an ugly but
 * simple format).
 * test1() also shows how to use parsef() to build a query.
 */

#include "db.h"
#include "btree.h"
#include "err.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static void print(void *ctx, unsigned short cnt, const struct db_val *vals)
{
	const struct db_val *val;
	int i;

	printf("found record:\n");
	for (i = 0; i < cnt; i++) {
		val = &vals[i];
		printf("\t%s.%s ", val->relation, val->name);
		switch(val->domain) {
			case DB_BYTES:
				printf("(BYTES(%u)):\t%s", 
						(unsigned int)val->size,
						val->val.pstring);
				break;
			case DB_STRING:
				printf("(STRING(%u)):\t%s", 
						(unsigned int)val->size,
						val->val.pstring);
				break;
			case DB_INT:
				printf("(INT):\t%d", val->val.vint);
				break;
			case DB_FLOAT:
				printf("(FLOAT):\t%f", val->val.vfloat);
				break;
			default:
				printf("<unknown>");
		}
		printf("\n");
	}
}

static void test1(void)
{
	DB_RESULT result;
	const char *tbl_name = "people";
	const char *attr_name = "name";
	const char *name = "Carsten 'Rudi' Wiesbaum";

	/* build and execute query (the %S directive escapes all single quotes 
	 * in the string and then encloses it in two single quotes to mark it 
	 * as STRING value */
	result = db_parsef("SELECT FROM %s WHERE %s.%s = %S",
			tbl_name, tbl_name, attr_name, name);

	if (!db_success(result)) {
		printf("Unsuccessful!\n");
		return;
	}

	/* iterate over the relation and call print() for each tuple */
	db_iterate(result, NULL, print);

	/* free the result's resources */
	db_free_result(result);
}

static void test2(void)
{
	DB_RESULT result;
	DB_ITERATOR iter;
	struct db_val *vals;

	/* execute query */
	result = db_parsef("SELECT FROM salaries WHERE salaries.%s >= %d",
			"age", 25);

	if (!db_success(result)) {
		printf("Unsuccessful!\n");
		return;
	}

	/* request iterator */
	iter = db_iterator(result);

	/* iterate over relation, call print() each time and free() the tuple */
	while ((vals = db_next(iter)) != NULL)
		print(NULL, db_attrcount(result), vals);

	/* free the iterator's resources */
	db_free_iterator(iter);

	/* free the result's resources */
	db_free_result(result);
}

/* Until main() it follows some index testing code you don't need to 
 * understand. */

#define def_primary_cmpf(type)		\
	static int cmpf_primar_##type(const char *a, const char *b, size_t s)\
	{\
		assert(s == sizeof(type));\
		if (*(type *)a < *(type *)b)		return -1;\
		else if (*(type *)a == *(type *)b)	return 0;\
		else					return 1;\
	}
def_primary_cmpf(db_int_t)
#define primary_cmpf(type)		cmpf_primar_##type

static void testix(void)
{
	struct index *ix;
	struct ix_iter *iter;
	blkaddr_t addr;
	unsigned int count;
	const char *n = "data/j.i.ix";

	PRINT_STR(n);
	ix = ix_open(n, primary_cmpf(db_int_t));
	if (!ix) {
		printf("Mist\n");
		return;
	}

	iter = ix_min(ix);
	count = 0;
	while ((addr = ix_rnext(iter)) != INVALID_ADDR) {
		PRINT_INT(addr);
		if (ix_rval(iter))
			PRINT_INT(*(int *)ix_rval(iter));
		else
			PRINT_INT(0);
		count++;
	}
	ix_close(ix);
	PRINT_INT(count);
}

int main(int argc, char *argv[])
{
	printf("Calling test1():\n");
	test1();
	printf("\n\n");
	printf("Calling test2():\n");
	test2();
	printf("Calling testix():\n");
	testix();

	/* Frees all  internal memory. */
	db_cleanup();
	return 0;
}

