#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "php_dingsbums.h"

int le_dingsbums_result;
int le_dingsbums_result_persist;
int le_dingsbums_iterator;
int le_dingsbums_iterator_persist;

ZEND_DECLARE_MODULE_GLOBALS(dingsbums)

static function_entry dingsbums_functions[] = {
	PHP_FE(db_parse, NULL)
	PHP_FE(db_free_result, NULL)
	PHP_FE(db_success, NULL)
	PHP_FE(db_is_definition, NULL)
	PHP_FE(db_is_modification, NULL)
	PHP_FE(db_is_sp, NULL)
	PHP_FE(db_is_query, NULL)
	PHP_FE(db_tpcount, NULL)
	PHP_FE(db_spvalue, NULL)
	PHP_FE(db_attrcount, NULL)
	PHP_FE(db_iterator, NULL)
	PHP_FE(db_free_iterator, NULL)
	PHP_FE(db_next, NULL)
	PHP_FE(db_cleanup, NULL)
	PHP_FE(db_info, NULL)
	{NULL, NULL, NULL}
};

zend_module_entry dingsbums_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	PHP_DINGSBUMS_WORLD_EXTNAME,
	dingsbums_functions,
	PHP_MINIT(dingsbums),
	PHP_MSHUTDOWN(dingsbums),
	NULL,
	NULL,
	NULL,
#if ZEND_MODULE_API_NO >= 20010901
	PHP_DINGSBUMS_WORLD_VERSION,
#endif
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_DINGSBUMS
ZEND_GET_MODULE(dingsbums)
#endif

PHP_INI_BEGIN()
PHP_INI_END()

PHP_MINIT_FUNCTION(dingsbums)
{
	le_dingsbums_result = zend_register_list_destructors_ex(NULL, NULL,
			PHP_DINGSBUMS_RESULT_RES_NAME, module_number);
	le_dingsbums_iterator = zend_register_list_destructors_ex(NULL, NULL,
			PHP_DINGSBUMS_ITERATOR_RES_NAME, module_number);
	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(dingsbums)
{
	return SUCCESS;
}

PHP_FUNCTION(db_parse)
{
	php_dingsbums_result *result;
	char *stmt;
	int stmt_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", 
				&stmt, &stmt_len) == FAILURE) {
		RETURN_FALSE;
	}

	result = emalloc(sizeof(php_dingsbums_result));
	result->result = db_parse(stmt);
	ZEND_REGISTER_RESOURCE(return_value, result, le_dingsbums_result);
}

PHP_FUNCTION(db_free_result)
{
	php_dingsbums_result *result;
	zval *zresult;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &zresult)
			== FAILURE) {
		RETURN_FALSE;
	}

	ZEND_FETCH_RESOURCE(result, php_dingsbums_result *, &zresult, -1, 
			PHP_DINGSBUMS_RESULT_RES_NAME, le_dingsbums_result);

	db_free_result(result->result);
	RETURN_TRUE;
}

PHP_FUNCTION(db_success)
{
	php_dingsbums_result *result;
	zval *zresult;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &zresult)
			== FAILURE) {
		RETURN_FALSE;
	}

	ZEND_FETCH_RESOURCE(result, php_dingsbums_result *, &zresult, -1, 
			PHP_DINGSBUMS_RESULT_RES_NAME, le_dingsbums_result);

	RETURN_BOOL(db_success(result->result));
}

PHP_FUNCTION(db_is_definition)
{
	php_dingsbums_result *result;
	zval *zresult;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &zresult)
			== FAILURE) {
		RETURN_FALSE;
	}

	ZEND_FETCH_RESOURCE(result, php_dingsbums_result *, &zresult, -1, 
			PHP_DINGSBUMS_RESULT_RES_NAME, le_dingsbums_result);

	RETURN_BOOL(db_is_definition(result->result));
}

PHP_FUNCTION(db_is_modification)
{
	php_dingsbums_result *result;
	zval *zresult;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &zresult)
			== FAILURE) {
		RETURN_FALSE;
	}

	ZEND_FETCH_RESOURCE(result, php_dingsbums_result *, &zresult, -1, 
			PHP_DINGSBUMS_RESULT_RES_NAME, le_dingsbums_result);

	RETURN_BOOL(db_is_modification(result->result));
}

PHP_FUNCTION(db_is_sp)
{
	php_dingsbums_result *result;
	zval *zresult;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &zresult)
			== FAILURE) {
		RETURN_FALSE;
	}

	ZEND_FETCH_RESOURCE(result, php_dingsbums_result *, &zresult, -1, 
			PHP_DINGSBUMS_RESULT_RES_NAME, le_dingsbums_result);

	RETURN_BOOL(db_is_sp(result->result));
}

PHP_FUNCTION(db_is_query)
{
	php_dingsbums_result *result;
	zval *zresult;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &zresult)
			== FAILURE) {
		RETURN_FALSE;
	}

	ZEND_FETCH_RESOURCE(result, php_dingsbums_result *, &zresult, -1, 
			PHP_DINGSBUMS_RESULT_RES_NAME, le_dingsbums_result);

	RETURN_BOOL(db_is_query(result->result));
}

PHP_FUNCTION(db_tpcount)
{
	php_dingsbums_result *result;
	zval *zresult;
	unsigned long cnt;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &zresult)
			== FAILURE) {
		RETURN_FALSE;
	}

	ZEND_FETCH_RESOURCE(result, php_dingsbums_result *, &zresult, -1, 
			PHP_DINGSBUMS_RESULT_RES_NAME, le_dingsbums_result);

	cnt = db_tpcount(result->result);
	RETURN_LONG(cnt);
}

PHP_FUNCTION(db_spvalue)
{
	php_dingsbums_result *result;
	zval *zresult;
	struct db_val retval;
	char *str;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &zresult)
			== FAILURE) {
		RETURN_FALSE;
	}

	ZEND_FETCH_RESOURCE(result, php_dingsbums_result *, &zresult, -1, 
			PHP_DINGSBUMS_RESULT_RES_NAME, le_dingsbums_result);

	retval = db_spvalue(result->result);
	switch (retval.domain) {
		case DB_INT:
			RETURN_LONG(retval.val.vint);
		case DB_FLOAT:
			RETURN_DOUBLE((double)retval.val.vfloat);
		case DB_STRING:
			str = malloc(strlen(retval.val.pstring)+1);
			strcpy(str, retval.val.pstring);
			RETURN_STRING(str, 0);
		case DB_BYTES:
			str = malloc(retval.size+1);
			memcpy(str, retval.val.pbytes, retval.size);
			RETURN_STRING(str, 0);
		default:
			RETURN_NULL();
	}
}

PHP_FUNCTION(db_attrcount)
{
	php_dingsbums_result *result;
	zval *zresult;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &zresult)
			== FAILURE) {
		RETURN_FALSE;
	}

	ZEND_FETCH_RESOURCE(result, php_dingsbums_result *, &zresult, -1, 
			PHP_DINGSBUMS_RESULT_RES_NAME, le_dingsbums_result);

	RETURN_LONG((long)db_attrcount(result->result));
}

PHP_FUNCTION(db_iterator)
{
	php_dingsbums_result *result;
	zval *zresult;
	php_dingsbums_iterator *iterator;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &zresult)
			== FAILURE) {
		RETURN_FALSE;
	}

	ZEND_FETCH_RESOURCE(result, php_dingsbums_result *, &zresult, -1, 
			PHP_DINGSBUMS_RESULT_RES_NAME, le_dingsbums_result);

	iterator = emalloc(sizeof(php_dingsbums_iterator));
	iterator->iter = db_iterator(result->result);
	ZEND_REGISTER_RESOURCE(return_value, iterator, le_dingsbums_iterator);
}

PHP_FUNCTION(db_free_iterator)
{
	php_dingsbums_iterator *iterator;
	zval *ziterator;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &ziterator)
			== FAILURE) {
		RETURN_FALSE;
	}

	ZEND_FETCH_RESOURCE(iterator, php_dingsbums_iterator *, &ziterator, -1, 
			PHP_DINGSBUMS_ITERATOR_RES_NAME, le_dingsbums_iterator);

	db_free_iterator(iterator->iter);
	RETURN_TRUE;
}

PHP_FUNCTION(db_next)
{
	php_dingsbums_iterator *iterator;
	zval *ziterator;
	struct db_val *vals;
	int i;
	zend_bool assoc_flag = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,
				"r|b", &ziterator, &assoc_flag) == FAILURE) {
		RETURN_FALSE;
	}

	ZEND_FETCH_RESOURCE(iterator, php_dingsbums_iterator *, &ziterator, -1, 
			PHP_DINGSBUMS_ITERATOR_RES_NAME, le_dingsbums_iterator);

	vals = db_next(iterator->iter);
	if (vals == NULL) {
		RETURN_FALSE;
	}

	array_init(return_value);
	for (i = 0; i < db_attrcount(iterator->iter.result); i++) {
		int rel_len = strlen(vals[i].relation);
		int name_len = strlen(vals[i].name);
		char key[rel_len + 1 + name_len + 1];
		float val_f;
		int val_i;
		char *val_s;

		strcpy(key, vals[i].relation);
		strcpy(key+rel_len, ".");
		strcpy(key+rel_len+1, vals[i].name);

		switch (vals[i].domain) {
			case DB_STRING:
				val_s = malloc(strlen(vals[i].val.pstring)+1);
				strcpy(val_s, vals[i].val.pstring);
				if (assoc_flag)
					add_assoc_string(return_value, key,
							val_s, 0);
				else
					add_index_string(return_value, i,
							val_s, 0);
				break;
			case DB_BYTES:
				val_s = malloc(vals[i].size);
				memcpy(val_s, vals[i].val.pbytes, vals[i].size);
				if (assoc_flag)
					add_assoc_stringl(return_value,
							key, val_s,
							vals[i].size, 0);
				else
					add_index_stringl(return_value,
							i, val_s,
							vals[i].size, 0);
				break;
			case DB_INT:
				val_i = vals[i].val.vint;
				if (assoc_flag)
					add_assoc_long(return_value,
							key, val_i);
				else
					add_index_long(return_value,
							i, val_i);
				break;
			case DB_FLOAT:
				val_f = vals[i].val.vfloat;
				if (assoc_flag)
					add_assoc_double(return_value,
							key, val_f);
				else
					add_index_double(return_value,
							i, val_f);
				break;
			default:
				if (assoc_flag)
					add_assoc_null(return_value, key);
				else
					add_index_null(return_value, i);
		}
	}
}

PHP_FUNCTION(db_cleanup)
{
	db_cleanup();
	RETURN_TRUE;
}

PHP_FUNCTION(db_info)
{
	char buf[512];
	int offset;

	sprintf(buf, "%s %s (build %s)\n", DB_NAME, DB_VERSION,
			__DATE__ " " __TIME__);
	RETURN_STRING(buf, 1);
}

