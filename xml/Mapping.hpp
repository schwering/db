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

#ifndef MAPPING_HPP
#define MAPPING_HPP

#include "db.hpp"
#include "DTDGraph.hpp"
#include "XMLElement.hpp"
#include "XPath.hpp"
#include <map>
#include <string>

namespace dingsbumsxml {

using std::map;
using std::string;

class Mapping {
	public:
		virtual ~Mapping() { }

		typedef CustomException<Mapping> MappingException;

		struct Attribute {
			string name;
			DB::Domain domain;
			unsigned int size;
			bool foreign;
			bool id;
			bool unique;
			Attribute(const string &n, DB::Domain d)
				: name(n), domain(d), size(0), foreign(false), id(false),
				unique(false) {};
			Attribute(const string &n, DB::Domain d, unsigned int s)
				: name(n), domain(d), size(s), foreign(false), id(false),
				unique(false) {};
			Attribute(const string &n, const DTDGraph::DataInfo &i)
				: name(n), domain(i.domain), size(i.size), foreign(false),
				id(i.id), unique(false) {};
		};

		struct Relation {
			string name;
			vector<Attribute> attrs;
			Relation(const string &n) : name(n), attrs(vector<Attribute>()) {};
		};

		virtual void create() = 0;

		virtual void insertElementBegin(const string &name,
				const map<string, string> &attrs) = 0;
		virtual void insertPCData(const string &data) = 0;
		virtual void insertElementEnd(const string &name) = 0;
		virtual void insert() = 0;

		virtual vector<XMLElement> search(const XPath &path) = 0;
};

}

#endif

