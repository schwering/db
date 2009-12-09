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

#ifndef NAIVEMAPPING_HPP
#define NAIVEMAPPING_HPP

#include "DTDGraph.hpp"
#include "Mapping.hpp"
#include <vector>

namespace dingsbumsxml {

using std::vector;

class NaiveMapping : public Mapping {
	public:
		typedef unsigned int Id;

		static const DB::Domain ID_DOMAIN = DB::UINT;
		static const char *ID_NAME;
		static const char *DATA_NAME;
		static const char *PRESENT_SUFFIX;
		static const DB::Domain PRESENT_DOMAIN = DB::INT;

		NaiveMapping(const DTDGraph &graph);
		~NaiveMapping();

		void create();

		void insertElementBegin(const string &name,
				const map<string, string> &attrs);
		void insertPCData(const string &data);
		void insertElementEnd(const string &name);
		void insert();

		vector<XMLElement> search(const XPath &path);

	private:
		const DTDGraph &graph;
		unsigned int getQuantifier(const string &from, const string &to);
		vector<Relation> relations;
		int indent;
		class Insertion;
		class StackElem;
		vector<StackElem> stack; // stores element begins/end
		vector<Insertion *> insertions;
		void rollup(); // converts the last level in the stack into insertions
		void makeXPathExpr(string *query, const string &last,
				const XPath::Node &node);
		void makeNextLevelQuery(string *query, vector<string> *sortBy,
				const string &from, const string &to);
		string getXPathQuery(const XPath &path);
		XMLElement buildXMLElement(const string &elem, const DB::Iterator &it);
		vector<XMLElement> buildXMLFragment(const string &elem,
				const string &query);
};

}

#endif

