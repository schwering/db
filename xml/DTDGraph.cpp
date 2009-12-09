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

#include "DTDGraph.hpp"
#include "XMLStringUtils.hpp"
#include <iostream>
#include <stdio.h>
#include <string>
#include <vector>


namespace dingsbumsxml {

using std::vector;

const char *DTDGraph::ATTRIBUTE_PREFIX = "A_";
const char *DTDGraph::ELEMENT_PREFIX = "E_";
const char *DTDGraph::DB_TYPE_PREFIX = "DBT";

// Parser for element model {{{
// The parser also directly simplifies the model:
// Let a be an arbitrary element names, E, F any model expression and X, Y any
// quantifier. Valid real-world quantifiers are *, +, ? or none. We need an
// order: * > ? > none (+ doesn't matter because it's replaced by +). Let max
// be the maximum function.
// 	E+			=>	E*
// 	(E, F)X		=>	EX, FX
// 	(E | F)		=> 	E?, F?
//	EXY			=>	Emax(X,Y)
//	EX,...,EY	=>	E*,...

void applyQuantifier(vector<DTDGraph::Child> *vec, unsigned int q) {
	for (vector<DTDGraph::Child>::iterator it = vec->begin();
			it != vec->end(); it++)
		it->quantifier |= q;
}

void applyQuantifier(vector<DTDGraph::Child> *vec, unsigned int last,
		unsigned int q) {
	for (vector<DTDGraph::Child>::reverse_iterator it = vec->rbegin();
			it != vec->rend() && last-- > 0; it--)
		it->quantifier |= q;
}

void append(vector<DTDGraph::Child> *vec, const string &s, unsigned q) {
	for (vector<DTDGraph::Child>::iterator it = vec->begin();
			it != vec->end(); it++) {
		if (it->dest == s) {
			it->quantifier = DTDGraph::Q_MULT;
			return;
		}
	}
	vec->push_back(DTDGraph::Child(s, q));
}

void appendVector(vector<DTDGraph::Child> *v1,
		const vector<DTDGraph::Child> *v2) {
	for (vector<DTDGraph::Child>::const_iterator it = v2->begin();
			it != v2->end(); it++)
		append(v1, it->dest, it->quantifier);
}

static inline unsigned int charToQuantifier(char c) {
	switch (c) {
		case '?':
			return DTDGraph::Q_OPT;
		case '*':
		case '+':
			return DTDGraph::Q_MULT;
		default:
			return DTDGraph::Q_ONCE;
	}
}

static inline char quantifierToChar(unsigned int q) {
	switch (q) {
		case DTDGraph::Q_OPT:
			return '?';
		case DTDGraph::Q_MULT:
			return '*';
		case DTDGraph::Q_ONCE:
			return '-';
		default:
			return '%'; // Obviously, % represents an error
	}
}

static inline bool isIn(int c, const char *str) {
	for (; *str; str++)
		if (c == *str)
			return true;
	return false;
}

vector<DTDGraph::Child> *DTDGraph::parseModel(const string &s) {
	vector<Child> *vec = new vector<Child>();
	unsigned int last = 0;
	bool hadOr = false;

	for (string::const_iterator it = s.begin(); it != s.end(); it++) {
		if (*it == '(') {
			string::const_iterator jt;
			unsigned int skipClosingBrackets = 0;
			for (jt = it+1; skipClosingBrackets != 0 || *jt != ')'; jt++) {
				if (*jt == '(')
					skipClosingBrackets++;
				else if (*jt == ')')
					skipClosingBrackets--;
			}
			unsigned int start = it - s.begin() + 1;
			unsigned int len = jt - it - 1;
			vector<Child> *subvec = parseModel(s.substr(start, len));

			unsigned int quantifier = 0;
			for (jt++; isIn(*jt, "?+*"); jt++)
				quantifier |= charToQuantifier(*jt);
			if (hadOr) {
				quantifier |= DTDGraph::Q_OPT;
				hadOr = false;
			}
			applyQuantifier(subvec, quantifier);
			it = jt - 1;

			appendVector(vec, subvec);
			last = subvec->size();
			delete subvec;
		} else if (*it == '|') {
			applyQuantifier(vec, last, DTDGraph::Q_OPT);
			hadOr = true;
		} else if (*it == ',') {
			// Oops?
		} else {
			string::const_iterator jt;
			for (jt = it; jt != s.end() && !isIn(*jt, "()|,?+*"); jt++)
				;
			unsigned int start = it - s.begin();
			unsigned int len = jt - it;
			string name = s.substr(start, len);

			unsigned int quantifier = 0;
			for (; jt != s.end() && isIn(*jt, "?+*"); jt++)
				quantifier |= charToQuantifier(*jt);
			if (hadOr) {
				quantifier |= DTDGraph::Q_OPT;
				hadOr = false;
			}
			it = jt - 1;

			append(vec, name, quantifier);
			last = 1;
		}
	}

	return vec;
}

// }}}

void DTDGraph::addElementNode(const XMLCh *xmlName, const XMLCh *xmlModel) {
	string *name = newString(xmlName);
	name->insert(0, ELEMENT_PREFIX);
	string *model = newString(xmlModel);

	nodes.insert(*name);

	if (*model == "ANY") {

		unsigned int q = Q_ONCE;
		edges.insert(make_pair(*name, Child("ANY", q)));

	} else if (*model == "EMPTY") {

		// nothing

	} else {

		const vector<Child> *vec = parseModel(*model);
		for (vector<Child>::const_iterator it = vec->begin();
				it != vec->end(); it++) {
			string dest = it->dest;

			if (dest != "#PCDATA") {
				dest.insert(0, ELEMENT_PREFIX);
			} else {
				// XXX This is quite ugly, because an element might have a 
				// model that allows semantically different PCDATAs, i.e. one
				// might be INT and the other STRING.
				// The complete procedure in parsing and then here ignores this
				// fact.
				DataInfo info(DB::DEFAULT_DOMAIN, DB::DEFAULT_SIZE);
				elemInfoMap.insert(make_pair(*name, info));
			}

			unsigned int quantifier = it->quantifier;
			edges.insert(make_pair(*name, Child(dest, quantifier)));

		}
		delete vec;
		delete name;
		delete model;

	}
}

static pair<DB::Domain, unsigned int> stringToDomain(const string& str) {
	pair<DB::Domain, unsigned int> p(DB::STRING, 256);
	if (str == "INT")			p.first = DB::INT;
	else if (str == "UINT")		p.first = DB::UINT;
	else if (str == "LONG")		p.first = DB::LONG;
	else if (str == "ULONG")	p.first = DB::ULONG;
	else if (str == "FLOAT")	p.first = DB::FLOAT;
	else if (str == "DOUBLE")	p.first = DB::DOUBLE;
	else if (str.find("STRING") == 0) {
		p.first = DB::STRING;
		sscanf(str.c_str(), "STRING(%u)", &p.second);
	}
	return p;
}

void DTDGraph::addAttributeNode(const XMLCh *xmlElem, const XMLCh *xmlAttr,
		const XMLCh *xmlType, const XMLCh *xmlMode, const XMLCh *xmlValue) {
	string *elem = newString(xmlElem);
	string *attr = newString(xmlAttr);

#if TYPING_BY_ATTRIBUTE
	if (attr->find(DB_TYPE_PREFIX, 0) != 0) { // normal attribute
#endif
		elem->insert(0, ELEMENT_PREFIX);
		nodes.insert(*elem);
		attr->insert(0, ATTRIBUTE_PREFIX);

		// The parser will replace empty (left out) attributes with their
		// default values if it is specified in the DTD. We have no further
		// interest in default values, as the parser will hide these
		// definitions. We simply store the data.

		string *type = newString(xmlType);
		string *mode = (xmlMode) ? newString(xmlMode) : 0;

		bool optional = (*mode == "#IMPLIED");
		bool id = (*type == "ID");

		unsigned int q;
		if (optional)
			q = Q_OPT;
		else
			q = Q_ONCE;
		edges.insert(make_pair(*elem, Child(*attr, q)));

		pair<string, string> key(*elem, *attr);
		DataInfo info(DB::STRING, 255, id);
		attrInfoMap.insert(make_pair(key, info));

		delete type;
		if (mode)
			delete mode;
#if TYPING_BY_ATTRIBUTE
	} else { // type-definition attribute
		string *value = newString(xmlValue);
		pair<DB::Domain, unsigned> domainWithSize = stringToDomain(*value);
		delete value;
		DataInfo info(domainWithSize.first, domainWithSize.second);

		if (attr->length() == strlen(DB_TYPE_PREFIX)) {
			elem->insert(0, ELEMENT_PREFIX);

			elemInfoMap[*elem] = info;
		} else {
			elem->insert(0, ELEMENT_PREFIX);
			// we need to cut the DB_TYPE_PREFIX before
			// inserting ATTRIBUTE_PREFIX
			string attrname = attr->substr(strlen(DB_TYPE_PREFIX));
			attrname.insert(0, ATTRIBUTE_PREFIX);

			pair<string, string> key(*elem, attrname);
			attrInfoMap[key] = info;
		}
	}
#endif

	delete elem;
	delete attr;
}

vector<const DTDGraph::Child *> DTDGraph::getChildren(const string &elem)
	const {
	vector<const Child *> vec;
	for (multimap<string, Child>::const_iterator it = edges.find(elem);
			it != edges.end() && it->first == elem; it++) {
		vec.push_back(&it->second);
	}
	return vec;
}

unsigned int DTDGraph::getQuantifier(const string &elem,
		const string &child) const {
	vector<const Child *> vec;
	for (multimap<string, Child>::const_iterator it = edges.find(elem);
			it != edges.end() && it->first == elem; it++) {
		if (child == it->second.dest)
			return it->second.quantifier;
	}
	std::cerr << elem << " 2 " << child<< std::endl;
	throw NotFoundException(EXC_ARGS);
}

bool operator==(const DTDGraph::Child &c, const DTDGraph::Child &d) {
	return c.dest == d.dest && c.quantifier == d.quantifier;
}

bool operator==(const DTDGraph::DataInfo &i, const DTDGraph::DataInfo &j) {
	return i.domain == j.domain && i.size == j.size && i.id == j.id;
}

bool operator==(const DTDGraph &g, const DTDGraph &h) {
	return g.nodes == h.nodes
		&& g.edges == h.edges
		&& g.elemInfoMap == h.elemInfoMap
		&& g.attrInfoMap == h.attrInfoMap;
}

ostream &operator<<(ostream &out, const DTDGraph::Child &c) {
	out << c.dest << ' ' << c.quantifier;
	return out;
}

istream &operator>>(istream &in, DTDGraph::Child &c) {
	in >> c.dest >> c.quantifier;
	return in;
}

ostream &operator<<(ostream &out, const DTDGraph::DataInfo &info) {
	out << info.domain << ' ' << info.size << ' ' << info.id;
	return out;
}

istream &operator>>(istream &in, DTDGraph::DataInfo &info) {
	unsigned int domain;
	in >> domain >> info.size >> info.id;
	info.domain = static_cast<DB::Domain>(domain);
	return in;
}

ostream &operator<<(ostream &out, const DTDGraph &g) {
	out << g.nodes.size() << std::endl;
	for (set<string>::const_iterator it = g.nodes.begin();
			it != g.nodes.end(); it++)
		out << *it << std::endl;

	out << g.edges.size() << std::endl;
	for (multimap<string, DTDGraph::Child>::const_iterator it = g.edges.begin();
			it != g.edges.end(); it++)
		out << it->first << ' ' << it->second << std::endl;

	out << g.elemInfoMap.size() << std::endl;
	for (map<string, DTDGraph::DataInfo>::const_iterator it
			= g.elemInfoMap.begin(); it != g.elemInfoMap.end(); it++)
		out << it->first << ' ' << it->second << std::endl;

	out << g.attrInfoMap.size() << std::endl;
	for (map<pair<string, string>, DTDGraph::DataInfo>::const_iterator it
			= g.attrInfoMap.begin(); it != g.attrInfoMap.end(); it++)
		out << it->first.first << ' ' << it->first.second << ' '
			<< it->second << std::endl;
	return out;
}

istream &operator>>(istream &in, DTDGraph &g) {
	unsigned int size;

	in >> size;
	for (unsigned int i = 0; i < size; i++) {
		string s;
		in >> s;
		g.nodes.insert(s);
	}

	in >> size;
	for (unsigned int i = 0; i < size; i++) {
		string s;
		DTDGraph::Child c;
		in >> s >> c;
		g.edges.insert(make_pair(s, c));
	}

	in >> size;
	for (unsigned int i = 0; i < size; i++) {
		string s;
		DTDGraph::DataInfo info;
		in >> s >> info;
		g.elemInfoMap.insert(make_pair(s, info));
	}

	in >> size;
	for (unsigned int i = 0; i < size; i++) {
		string s, t;
		DTDGraph::DataInfo info;
		in >> s >> t >> info;
		g.attrInfoMap.insert(make_pair(make_pair(s, t), info));
	}
	return in;
}

#ifndef NDEBUG
void DTDGraph::printNodes() const {
	std::cout << "Printing nodes:" << std::endl;
	for (const_iterator it = begin(); it != end(); it++)
		std::cout << "\tNode: " << *it << std::endl;
}

void DTDGraph::printEdges() const {
	std::cout << "Printing edges:" << std::endl;
	for (multimap<string, Child>::const_iterator it = edges.begin();
			it != edges.end(); it++)  {
		std::cout << "\tEdge: " << it->first
			<< " --" << quantifierToChar(it->second.quantifier) << "--> "
			<< it->second.dest << std::endl;
	}
}

void DTDGraph::draw(ostream& out) const {
	out << "digraph DTDGraph {" << std::endl;
	for (multimap<string, Child>::const_iterator it = edges.begin();
			it != edges.end(); it++)  {
		out << "\"" << it->first << "\" -> \"" << it->second.dest << "\""
			<< " [label=\"" << quantifierToChar(it->second.quantifier) << "\"]"
			<< std::endl;
	}
	out << "}" << std::endl;
}
#endif

}

