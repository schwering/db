#ifndef PHP_DINGSBUMS_H
#define PHP_DINGSBUMS_H

#include <db.h>

#ifdef ZTS
#include "TSRM.h"
#endif

ZEND_BEGIN_MODULE_GLOBALS(dingsbums)
	long counter;
	zend_bool direction;
ZEND_END_MODULE_GLOBALS(dingsbums)

#ifdef ZTS
#define DINGSBUMS_G(v) TSRMG(dingsbums_globals_id, zend_dingsbums_globals *, v)
#else
#define DINGSBUMS_G(v) (dingsbums_globals.v)
#endif

#define PHP_DINGSBUMS_WORLD_VERSION	"1.0"
#define PHP_DINGSBUMS_WORLD_EXTNAME	"dingsbums"

typedef struct _php_dingsbums_result {
	DB_RESULT result;
} php_dingsbums_result;

#define PHP_DINGSBUMS_RESULT_RES_NAME	"dingsbums result"

typedef struct _php_dingsbums_iterator {
	DB_ITERATOR iter;
} php_dingsbums_iterator;

#define PHP_DINGSBUMS_ITERATOR_RES_NAME	"dingsbums iterator"

PHP_MINIT_FUNCTION(dingsbums);
PHP_MSHUTDOWN_FUNCTION(dingsbums);

PHP_FUNCTION(db_parse);
PHP_FUNCTION(db_free_result);
PHP_FUNCTION(db_success);
PHP_FUNCTION(db_is_definition);
PHP_FUNCTION(db_is_modification);
PHP_FUNCTION(db_is_sp);
PHP_FUNCTION(db_is_query);
PHP_FUNCTION(db_tpcount);
PHP_FUNCTION(db_spvalue);
PHP_FUNCTION(db_attrcount);
PHP_FUNCTION(db_iterator);
PHP_FUNCTION(db_free_iterator);
PHP_FUNCTION(db_next);
PHP_FUNCTION(db_cleanup);
PHP_FUNCTION(db_info);

extern zend_module_entry	dingsbums_module_entry;
#define phpext_dingsbums_ptr	&dingsbums_module_entry

#endif

