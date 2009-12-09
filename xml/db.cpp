/* vim:foldmethod=marker:tabstop=4:shiftwidth=4
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

#include "db.hpp"
#include <sstream>

namespace dingsbums {
ostream &operator<<(ostream &out, const Value &v) {
	out << v.toString();
	return out;
}

string Value::toString() const {
	std::stringstream s;
	if (isString())			s << getString();
	else if (isBytes())		s << getBytes();
	else if (isInt())		s << getInt();
	else if (isUInt())		s << getUInt() << 'U';
	else if (isLong())		s << getLong() << 'L';
	else if (isULong())		s << getULong() << "UL";
	else if (isFloat())		s << getFloat() << 'F';
	else if (isDouble())	s << getDouble();
	return s.str();
}

DefinitionResult executeDefinition(const string &stmt) {
#ifndef NDEBUG
	std::cout << "executeDefinition(" << stmt << ");" << std::endl;
#endif
	DB_RESULT r = db_parse(stmt.c_str());
	if (!db_success(r)) {
		db_free_result(r);
		throw DatabaseException("Unsuccessful", EXC_ARGS);
	} else if (!db_is_definition(r)) {
		db_free_result(r);
		throw DatabaseException("Not a definition", EXC_ARGS);
	} else
		return DefinitionResult(r);
};

ModificationResult executeModification(const string &stmt) {
#ifndef NDEBUG
	std::cout << "executeModification(" << stmt << ");" << std::endl;
#endif
	DB_RESULT r = db_parse(stmt.c_str());
	if (!db_success(r)) {
		db_free_result(r);
		throw DatabaseException("Unsuccessful", EXC_ARGS);
	} else if (!db_is_modification(r)) {
		db_free_result(r);
		throw DatabaseException("Not a modification", EXC_ARGS);
	} else
		return ModificationResult(r);
};

ProcedureResult executeProcedure(const string &stmt) {
#ifndef NDEBUG
	std::cout << "executeProcedure(" << stmt << ");" << std::endl;
#endif
	DB_RESULT r = db_parse(stmt.c_str());
	if (!db_success(r)) {
		db_free_result(r);
		throw DatabaseException("Unsuccessful", EXC_ARGS);
	} else if (!db_is_sp(r)) {
		db_free_result(r);
		throw DatabaseException("Not a procedure", EXC_ARGS);
	} else
		return ProcedureResult(r);
};

QueryResult executeQuery(const string &stmt) {
#ifndef NDEBUG
	std::cout << "executeQuery(" << stmt << ");" << std::endl;
#endif
	DB_RESULT r = db_parse(stmt.c_str());
	if (!db_success(r)) {
		db_free_result(r);
		throw DatabaseException("Unsuccessful", EXC_ARGS);
	} else if (!db_is_query(r)) {
		db_free_result(r);
		throw DatabaseException("Not a Query", EXC_ARGS);
	} else
		return QueryResult(r);
};

void cleanup(void) {
	db_cleanup();
}

}

