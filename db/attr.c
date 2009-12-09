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

#include "attr.h"
#include "constants.h"
#include "dml.h"
#include "block.h"
#include "io.h"
#include "rlmngt.h"
#include "str.h"
#include <assert.h>
#include <string.h>

/* primary indexes comparison functions behave same like tuple comparison
 * functions; both simply compare the values */
#define cmpf(type)			cmpf_primar_##type
#define primary_cmpf(type)		cmpf_primar_##type
#define def_primary_cmpf(type)		\
	static int cmpf_primar_##type(const char *a, const char *b, size_t s)\
	{\
		assert(s == sizeof(type));\
		if (*(type *)a < *(type *)b)		return -1;\
		else if (*(type *)a == *(type *)b)	return 0;\
		else					return 1;\
	}

/* secondary index comparison functions compare the value itself, and 
 * if this is equal, they also compare the tuple address which is stored
 * behind the value in secondary indexes */
#define secondary_cmpf(type)		cmpf_secon_##type
#define def_secondary_cmpf(type)	\
	static int cmpf_secon_##type(const char *a, const char *b, size_t s)\
	{\
		if (*(type *)a < *(type *)b)\
			return -1;\
		else if (*(type *)a == *(type *)b)\
			return addrcmp(*(blkaddr_t *)(a + sizeof(type)),\
					*(blkaddr_t *)(b + sizeof(type)));\
		else return 1;\
	}

static int addrcmp(blkaddr_t a1, blkaddr_t a2)
{
	if (a1 == INVALID_ADDR || a2 == INVALID_ADDR)
		return 0;
	else if (a1 < a2)
		return -1;
	else if (a1 == a2)
		return 0;
	else
		return 1;
}

def_primary_cmpf(db_int_t)
def_secondary_cmpf(db_int_t)
def_primary_cmpf(db_uint_t)
def_secondary_cmpf(db_uint_t)
def_primary_cmpf(db_long_t)
def_secondary_cmpf(db_long_t)
def_primary_cmpf(db_ulong_t)
def_secondary_cmpf(db_ulong_t)
def_primary_cmpf(db_float_t)
def_secondary_cmpf(db_float_t)
def_primary_cmpf(db_double_t)
def_secondary_cmpf(db_double_t)

static int primary_cmpf(string)(const char *s1, const char *s2, size_t s)
{
	return strncmp(s1, s2, s);
}

static int primary_cmpf(bytes)(const char *b1, const char *b2, size_t s)
{
	return memcmp((void *)b1, (void *)b2, s);
}

static int secondary_cmpf(string)(const char *s1, const char *s2, size_t s)
{
	int val;
       
	if ((val = strncmp((void *)s1, (void *)s2, s - sizeof(blkaddr_t))) != 0)
		return val;
	else
		return addrcmp(*(blkaddr_t *)(s1 + s - sizeof(blkaddr_t)),
				*(blkaddr_t *)(s2 + s - sizeof(blkaddr_t)));
}

static int secondary_cmpf(bytes)(const char *b1, const char *b2, size_t s)
{
	int val;
       
	if ((val = memcmp((void *)b1, (void *)b2, s - sizeof(blkaddr_t))) != 0)
		return val;
	else
		return addrcmp(*(blkaddr_t *)(b1 + s - sizeof(blkaddr_t)),
				*(blkaddr_t *)(b2 + s - sizeof(blkaddr_t)));
}

cmpf_t cmpf_by_sattr(struct sattr *attr)
{
	assert(attr != NULL);
	assert(attr->at_domain == INT
			|| attr->at_domain == UINT
			|| attr->at_domain == LONG
			|| attr->at_domain == ULONG
			|| attr->at_domain == FLOAT
			|| attr->at_domain == DOUBLE
			|| attr->at_domain == STRING
			|| attr->at_domain == BYTES);

	switch (attr->at_domain) {
		case INT:	return cmpf(db_int_t);
		case UINT:	return cmpf(db_uint_t);
		case LONG:	return cmpf(db_long_t);
		case ULONG:	return cmpf(db_ulong_t);
		case FLOAT:	return cmpf(db_float_t);
		case DOUBLE:	return cmpf(db_double_t);
		case STRING:	return cmpf(string);
		case BYTES:	return cmpf(bytes);
		default:	return NULL;
	}
}

cmpf_t ixcmpf_by_sattr(struct sattr *attr)
{
	assert(attr != NULL);
	assert(attr->at_domain == INT
			|| attr->at_domain == UINT
			|| attr->at_domain == LONG
			|| attr->at_domain == ULONG
			|| attr->at_domain == FLOAT
			|| attr->at_domain == DOUBLE
			|| attr->at_domain == STRING
			|| attr->at_domain == BYTES);
	assert(attr->at_indexed == PRIMARY || attr->at_indexed == SECONDARY);

	if (attr->at_indexed == PRIMARY) {
		switch (attr->at_domain) {
			case INT:	return primary_cmpf(db_int_t);
			case UINT:	return primary_cmpf(db_uint_t);
			case LONG:	return primary_cmpf(db_long_t);
			case ULONG:	return primary_cmpf(db_ulong_t);
			case FLOAT:	return primary_cmpf(db_float_t);
			case DOUBLE:	return primary_cmpf(db_double_t);
			case STRING:	return primary_cmpf(string);
			case BYTES:	return primary_cmpf(bytes);
			default:	return NULL;
		}
	} else if (attr->at_indexed == SECONDARY) {
		switch (attr->at_domain) {
			case INT:	return secondary_cmpf(db_int_t);
			case UINT:	return secondary_cmpf(db_uint_t);
			case LONG:	return secondary_cmpf(db_long_t);
			case ULONG:	return secondary_cmpf(db_ulong_t);
			case FLOAT:	return secondary_cmpf(db_float_t);
			case DOUBLE:	return secondary_cmpf(db_double_t);
			case STRING:	return secondary_cmpf(string);
			case BYTES:	return secondary_cmpf(bytes);
			default:	return NULL;
		}
	} else {
		assert(false);
		return NULL;
	}
}

void set_sattr_val(char *tuple, struct sattr *sattr, struct value *value)
{
	char *dest;

	assert(tuple != NULL);
	assert(sattr != NULL);
	assert(value != NULL);
	assert(sattr->at_domain == value->domain);

	dest = tuple + sattr->at_offset;
	switch (sattr->at_domain) {
		case INT:
			memcpy(dest, &value->ptr.vint, sizeof(db_int_t));
			break;
		case UINT:
			memcpy(dest, &value->ptr.vuint, sizeof(db_uint_t));
			break;
		case LONG:
			memcpy(dest, &value->ptr.vlong, sizeof(db_long_t));
			break;
		case ULONG:
			memcpy(dest, &value->ptr.vulong, sizeof(db_ulong_t));
			break;
		case FLOAT:
			memcpy(dest, &value->ptr.vfloat, sizeof(db_float_t));
			break;
		case DOUBLE:
			memcpy(dest, &value->ptr.vdouble, sizeof(db_double_t));
			break;
		case STRING:
			strntermcpy(dest, value->ptr.pstring, sattr->at_size);
			break;
		case BYTES:
			memcpy(dest, value->ptr.pbytes, sattr->at_size);
			break;
		default:
			assert(false);
	}
}

struct sattr *sattr_by_srl_and_attr_name(struct srel *srl, char *attr_name)
{
	int i;

	assert(srl != NULL);
	assert(attr_name != NULL);

	for (i = 0; i < srl->rl_header.hd_atcnt; i++) {
		struct sattr *sattr = &srl->rl_header.hd_attrs[i];
		char *at_name = sattr->at_name;
		if (!strncmp(at_name, attr_name, AT_NAME_MAX))
			return sattr;
	}
	return NULL;
}

struct sattr *sattr_by_attr(struct attr *attr)
{
	struct srel *srl;

	assert(attr != NULL);
	assert(attr->tbl_name != NULL);
	assert(attr->attr_name != NULL);

	srl = open_relation(attr->tbl_name);
	return sattr_by_srl_and_attr_name(srl, attr->attr_name);
}

