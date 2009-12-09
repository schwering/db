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

#ifndef _MY_VERY_OWN_DB_HPP_
#define _MY_VERY_OWN_DB_HPP_

#include "Exception.hpp"
#include <db.h>
#include <iostream>
#include <string>

/**
 * C++ wrapper of the dingsbums C API, db.h.
 * There are four important initial functions which should be used for the
 * respective commands:
 *   - executeDefinition()			CREATE/DROP TABLE/INDEX/VIEW
 *   - executeModification()		INSERT/UPDATE/DELETE
 *   - executeProcedure()			stored procedures i.e. COUNT/AVG
 *   - executeQuery()				SELECT/PROJECT/UNION/JOIN/SORT
 *
 * They return a DefinitionResult, ModificationResult, ProcedureResult
 * or QueryResult object, if the statement is executed successfully.
 * Otherwise, a DatabaseException is thrown.
 * Each of the *Result objects provides methods that might be useful for the
 * respective kind of result.
 * The *Result objects wrap what you might know from the C API as DB_RESULT.
 *
 * The Iterator class is important in the context of executeQuery(). It wraps
 * the C API's DB_ITERATOR structure.
 *
 * The Value class wraps the db_val structure of the C API.
 *
 * The Domain enumeration simply defines the same values as the C API's
 * db_domain enumeration just without the leading ``DB_''.
 *
 * The destructores of the *Result classes and the Iterator class hide
 * the db_free_result() respectively db_free_iterator() calls from the 
 * programmer.
 * To close all open relations and free internally used memory, call the
 * cleanup() function that simply forwards to db_cleanup() as you know it 
 * from the C API.
 *
 * Additionally to the methods declared in this file, all the dingsbums
 * standard C functions and definitions from db.h are available in the
 * namesapce.
 *
 * Though the namespace's name is ``dingsbums'', I'd suggest setting an 
 * alias ``DB'' for dingsbums -- this is done at the bottom of this file :-).
 */

namespace dingsbums {
using std::pair;
using std::ostream;
using std::string;

typedef dingsbumsxml::CustomException<DB_RESULT> DatabaseException;
typedef dingsbumsxml::CustomException<DB_ITERATOR> IteratorException;

enum Domain { INT=DB_INT, UINT=DB_UINT, LONG=DB_LONG, ULONG=DB_ULONG,
	FLOAT=DB_FLOAT, DOUBLE=DB_DOUBLE, STRING=DB_STRING, BYTES=DB_BYTES };

class ResultContainer {
	public:
		ResultContainer(DB_RESULT r) : r(r) {
			refs = new unsigned int;
			*refs = 1;
		}

		ResultContainer(const ResultContainer &result) {
			r = result.r;
			refs = result.refs;
			(*refs)++;
		}

		ResultContainer &operator=(const ResultContainer &result) {
			if (this == &result)
				return *this;
			if (--(*refs) == 0)
				db_free_result(r);
			r = result.r;
			refs = result.refs;
			(*refs)++;
		}

		~ResultContainer() {
			if (--(*refs) == 0) {
				db_free_result(r);
				delete refs;
			}
		}

		DB_RESULT r;

	private:
		unsigned int *refs;
};

class Value {
	public:
		Value(const struct db_val &v) : val(v) {
			// We have a copy of val here, so we can modify it. And 
			// because it would be nice if a Value would live longer 
			// than the iterator to which it belongs, we copy the the
			// content to which val points.
			char *ptr;
			unsigned int size;

			size = strlen(val.relation) + 1;
			ptr = new char[size];
			memcpy(ptr, val.relation, size);
			val.relation = ptr;

			size = strlen(val.name) + 1;
			ptr = new char[size];
			memcpy(ptr, val.name, size);
			val.name = ptr;

			if (val.domain == DB_STRING) {
				ptr = new char[val.size];
				memcpy(ptr, val.val.pstring, val.size);
				val.val.pstring = ptr;
			} else if (val.domain == DB_BYTES) {
				ptr = new char[val.size];
				memcpy(ptr, val.val.pbytes, val.size);
				val.val.pbytes = ptr;
			}
		}

		~Value() {
			if (val.domain == DB_STRING)
				delete[] val.val.pstring;
			else if (val.domain == DB_BYTES)
				delete[] val.val.pbytes;
			delete[] val.name;
			delete[] val.relation;
		}

		friend ostream &operator<<(ostream &out, const Value &v);

		const char *getRelation() const { return val.relation; }
		const char *getAttribute() const { return val.name; }
		Domain getDomain() const { return static_cast<Domain>(val.domain); }
		unsigned int getSize() const {
			return static_cast<unsigned int>(val.size); }

		bool isString() const { return getDomain() == STRING; }
		const char *getString() const { return val.val.pstring; }

		bool isBytes() const { return getDomain() == BYTES; }
		const char *getBytes() const { return val.val.pstring; }

		bool isInt() const { return getDomain() == INT; }
		int getInt() const { return val.val.vint; }

		bool isUInt() const { return getDomain() == UINT; }
		unsigned int getUInt() const { return val.val.vuint; }

		bool isLong() const { return getDomain() == LONG; }
		long getLong() const { return val.val.vlong; }

		bool isULong() const { return getDomain() == ULONG; }
		unsigned long getULong() const { return val.val.vulong; }

		bool isFloat() const { return getDomain() == FLOAT; }
		float getFloat() const { return val.val.vfloat; }

		bool isDouble() const { return getDomain() == DOUBLE; }
		double getDouble() const { return val.val.vdouble; }

		string toString() const;
	private:
		struct db_val val;
};

class Iterator {
	public:
		Iterator(const ResultContainer &r, const DB_ITERATOR &i)
			: r(r), i(i), refs(new unsigned int) {
			*refs = 1;
		}

		Iterator(const Iterator &iter) 
			: r(iter.r), i(iter.i), refs(iter.refs) {
			(*refs)++;
		}

		Iterator &operator=(const Iterator &iter) {
			if (this == &iter)
				return *this;
			if (--(*refs) == 0)
				db_free_iterator(i);
			r = iter.r;
			i = iter.i;
			refs = iter.refs;
			(*refs)++;
		}

		~Iterator() {
			if (--(*refs) == 0) {
				db_free_iterator(i);
				delete refs;
			}
		}

		bool next() { return (tp = db_next(i)) != NULL; }
		unsigned int length() const { return db_attrcount(r.r); }

		Value operator[](unsigned int i) const {
			if (i >= length()) throw IteratorException(EXC_ARGS);
			return Value(tp[i]);
		}

		Value operator[](pair<string, string> p) const {
			const char *rl = p.first.c_str();
			const char *attr = p.second.c_str();
			for (unsigned int i = 0; i < length(); i++)
				if (!strcmp(rl,tp[i].relation) && !strcmp(attr,tp[i].name))
					return Value(tp[i]);
			throw IteratorException(EXC_ARGS);
		}

		Value operator[](pair<string, const char *> p) const {
			const char *rl = p.first.c_str();
			const char *attr = p.second;
			for (unsigned int i = 0; i < length(); i++)
				if (!strcmp(rl,tp[i].relation) && !strcmp(attr,tp[i].name))
					return Value(tp[i]);
			throw IteratorException(EXC_ARGS);
		}

		Value operator[](pair<const char *, const string> p) const {
			const char *rl = p.first;
			const char *attr = p.second.c_str();
			for (unsigned int i = 0; i < length(); i++)
				if (!strcmp(rl,tp[i].relation) && !strcmp(attr,tp[i].name))
					return Value(tp[i]);
			throw IteratorException(EXC_ARGS);
		}

		Value operator[](pair<const char *, const char *> p) const {
			const char *rl = p.first;
			const char *attr = p.second;
			for (unsigned int i = 0; i < length(); i++)
				if (!strcmp(rl,tp[i].relation) && !strcmp(attr,tp[i].name))
					return Value(tp[i]);
			throw IteratorException(EXC_ARGS);
		}

	private:
		ResultContainer r;
		DB_ITERATOR i;
		unsigned int *refs;
		struct db_val *tp;
};

class DefinitionResult {
	public:
		DefinitionResult(DB_RESULT &r) : r(ResultContainer(r)) { }
	private:
		ResultContainer r;
};

class ModificationResult {
	public:
		ModificationResult(DB_RESULT &r) : r(ResultContainer(r)) { }
		unsigned long getTupleCount() const { return db_tpcount(r.r); }
	private:
		ResultContainer r;
};

class ProcedureResult {
	public:
		ProcedureResult(DB_RESULT &r) : r(ResultContainer(r)) { }
		Value getValue() const { return Value(db_spvalue(r.r)); };
	private:
		ResultContainer r;
};

class QueryResult {
	public:
		QueryResult(DB_RESULT &r) : r(ResultContainer(r)) { }
		int getAttributeCount() const { return db_attrcount(r.r); }
		Iterator getIterator() const {
			return Iterator(r, db_iterator(r.r)); };
		void print() const { db_print(r.r); }
	private:
		ResultContainer r;
};

DefinitionResult executeDefinition(const string &stmt);
ModificationResult executeModification(const string &stmt);
ProcedureResult executeProcedure(const string &stmt);
QueryResult executeQuery(const string &stmt);
void cleanup(void);

// The two following constants are only used in the XML mapping; you don't
// have to care about them. You can simply delete the lines.
const Domain DEFAULT_DOMAIN = STRING;
const unsigned int DEFAULT_SIZE = 256;

}

namespace DB = dingsbums;

#endif

