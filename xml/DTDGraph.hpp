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

#ifndef DTDGRAPH_HPP
#define DTDGRAPH_HPP

#include "db.hpp"
#include "Exception.hpp"
#include <map>
#include <set>
#include <string>
#include <iostream>
#include <vector>
#include <xercesc/util/XMLString.hpp>

namespace dingsbumsxml {

using std::istream;
using std::map;
using std::multimap;
using std::pair;
using std::ostream;
using std::set;
using std::string;
using std::vector;
using xercesc::XMLString;

#define TYPING_BY_ATTRIBUTE	1

class DTDGraph {
	public:
		typedef set<string>::const_iterator const_iterator;

		struct Child {
			string dest;
			unsigned int quantifier;
			Child() : dest(""), quantifier(0) {};
			Child(const string &d, unsigned int q) : dest(d), quantifier(q) {};
			friend bool operator==(const Child &g, const Child &h);
			friend ostream &operator<<(ostream &out, const Child &c);
			friend istream &operator>>(istream &in, DTDGraph::Child &c);
		};

		struct DataInfo {
			DB::Domain domain;
			unsigned int size;
			bool id; // attr only
			DataInfo() : domain(DB::STRING), size(256), id(false) {};
			DataInfo(DB::Domain d, unsigned int s, bool id = false)
				: domain(d), size(s), id(id) {};
			friend bool operator==(const DataInfo &g, const DataInfo &h);
			friend ostream &operator<<(ostream &out, const DataInfo &info);
			friend istream &operator>>(istream &in, DataInfo &info);
		};

		typedef CustomException<DTDGraph> NotFoundException;

		static const char *ELEMENT_PREFIX;
		static const char *ATTRIBUTE_PREFIX;
		static const char *DB_TYPE_PREFIX;

		static const unsigned int Q_ONCE = 0x00;
		static const unsigned int Q_OPT = 0x01;
		static const unsigned int Q_MULT = 0x11;

		DTDGraph() {
		}

		~DTDGraph() {
		}

		void addElementNode(const XMLCh *name, const XMLCh *model);

		void addAttributeNode(const XMLCh *elem, const XMLCh *attr,
				const XMLCh *type, const XMLCh *mode, const XMLCh *value);

		const_iterator begin() const {
			return nodes.begin();
		}

		const_iterator end() const {
			return nodes.end();
		}

		/* Returns the children of a specified element. The children can be
		 * either elements, which can have further children, or attributes,
		 * which cannot have children, or it can be "#PCDATA".
		 * Note that while content-containing elements have a "#PCDATA" child,
		 * attributes do never have any child.
		 * Elements can be recognized by the leading ELEMENT_PREFIX, attributes
		 * by the leading ATTRIBUTE_PREFIX. */
		vector<const Child *> getChildren(const string &elem) const;

		unsigned int getQuantifier(const string &elem,
				const string &child) const;

		/* Returns a DataInfo structure for elements that contain #PCDATA
		 * (and only for * them). */
		const DataInfo *getDataInfo(const string &elem) const {
			map<string, DataInfo>::const_iterator it
				= elemInfoMap.lower_bound(elem);
			return it != elemInfoMap.end() ? &it->second : 0;
		}

		/* Returns a DataInfo structure for any attribute of any element. */
		const DataInfo *getDataInfo(const string &elem,
				const string &attr) const {
			map<pair<string, string>, DataInfo>::const_iterator it
				= attrInfoMap.lower_bound(make_pair(elem, attr));
			return it != attrInfoMap.end() ? &it->second : 0;
		}

		static bool isElement(const string &name) {
			return name.find(ELEMENT_PREFIX, 0) == 0;
		}

		static bool isAttribute(const string &name) {
			return name.find(ATTRIBUTE_PREFIX, 0) == 0;
		}

		static bool isPCData(const string &name) {
			return name == "#PCDATA";
		}

		static bool isAny(const string &name) {
			return name == "ANY";
		}

		friend bool operator==(const DTDGraph &g, const DTDGraph &h);
		friend ostream &operator<<(ostream &out, const DTDGraph &g);
		friend istream &operator>>(istream &in, DTDGraph &g);

#ifndef NDEBUG
		void printNodes() const;
		void printEdges() const;
		void draw(ostream &out) const;
#endif

	private:
		set<string> nodes;
		multimap<string, Child> edges;
		map<string, DataInfo> elemInfoMap;
		map<pair<string, string>, DataInfo> attrInfoMap;

		vector<Child> *parseModel(const string &s);
};

}

#endif

