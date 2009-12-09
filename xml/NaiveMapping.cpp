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

#include "NaiveMapping.hpp"
#include <sstream>

namespace dingsbumsxml {

using std::stringstream;

const char *NaiveMapping::ID_NAME = "id";
const char *NaiveMapping::DATA_NAME = "data";
const char *NaiveMapping::PRESENT_SUFFIX = "_P";

// Utility functions {{{ 

static string makeMiddleName(const string &parentRelationName, 
		const string &childRelationName) {
	return parentRelationName + "__to__" + childRelationName;
}

static string domainToDeclaration(DB::Domain domain, unsigned int size = 0) {
	switch (domain) {
		case DB::INT:	return "INT";
		case DB::UINT:	return "UINT";
		case DB::LONG:	return "LONG";
		case DB::ULONG:	return "ULONG";
		case DB::FLOAT:	return "FLOAT";
		case DB::DOUBLE:	return "DOUBLE";
		case DB::STRING: {
						stringstream s;
						s << "STRING(" << size << ")";
						return s.str();
						}
		case DB::BYTES: {
						stringstream s;
						s << "STRING(" << size << ")";
						return s.str();
						}
	}
	return "INVALID DOMAIN";
}

static string formatId(NaiveMapping::Id id) {
	stringstream stream;
	stream << id << "U";
	return stream.str();
}

static string formatPresent(bool present) {
	stringstream stream;
	stream << present;
	return stream.str();
}

static string formatData(const string &data,
		const DTDGraph::DataInfo &info) {
	stringstream stream;
	switch (info.domain) {
		case DB::INT:
		case DB::DOUBLE:
			stream << data;
			break;
		case DB::STRING:
		case DB::BYTES:
			// what about string escaping?
			stream << "'" << data << "'";
			break;
		case DB::UINT:
			stream << data << "U";
			break;
		case DB::LONG:
			stream << data << "L";
			break;
		case DB::ULONG:
			stream << data << "UL";
			break;
		case DB::FLOAT:
			stream << data << "F";
			break;
		default:
			throw Mapping::MappingException(EXC_ARGS);
	}
	return stream.str();
}

static string stripPrefix(const string &name) {
	if (name.find(DTDGraph::ELEMENT_PREFIX))
		return name.substr(strlen(DTDGraph::ELEMENT_PREFIX));
	else if (name.find(DTDGraph::ATTRIBUTE_PREFIX))
		return name.substr(strlen(DTDGraph::ATTRIBUTE_PREFIX));
	else
		return name;
}

template <class T>
static string toString(const T val) {
	stringstream s;
	s << val;
	return s.str();
}

// }}}

NaiveMapping::NaiveMapping(const DTDGraph &graph) // {{{
	: Mapping(), graph(graph), indent(0) {
	for (DTDGraph::const_iterator it = graph.begin();
			it != graph.end(); it++) {
		vector<const DTDGraph::Child *> vec = graph.getChildren(*it);
		Relation rl(*it);
		vector<Relation> middleRelations; // collects middle-relations of rl

		// this element's ID
		Attribute id(ID_NAME, ID_DOMAIN);
		id.unique = true;
		rl.attrs.push_back(id);

		for (vector<const DTDGraph::Child *>::const_iterator jt
				= vec.begin(); jt != vec.end(); jt++) {
			const string &n = (*jt)->dest;
			unsigned int q = (*jt)->quantifier;

			if (DTDGraph::isElement(n)) {

				if (q == DTDGraph::Q_ONCE) {
					// attribute for ID of tuple in the sub-element-relation
					Attribute subid(n, ID_DOMAIN);
					rl.attrs.push_back(subid);
				} else if (q == DTDGraph::Q_OPT) {
					// attribute for ID of tuple in the sub-element-relation
					// (value 0 indicates that the child is not present)
					Attribute subid(n, ID_DOMAIN);
					rl.attrs.push_back(subid);
				} else if (q == DTDGraph::Q_MULT) {
					// table-in-the-middle relation that links this relation's 
					// tuple's ID to all IDs of tuples in the sub-element-
					// relation
					string middleName = makeMiddleName(*it, n);
					Relation middle(middleName);
					Attribute middleId(ID_NAME, ID_DOMAIN); // used for sorting
					Attribute parent(*it, ID_DOMAIN);
					parent.foreign = true;
					Attribute child(n, ID_DOMAIN);
					middle.attrs.push_back(middleId);
					middle.attrs.push_back(parent);
					middle.attrs.push_back(child);
					middleRelations.push_back(middle);
				}

			} else if (DTDGraph::isAttribute(n)) {

				// attribute for content of the XML attribute
				const DTDGraph::DataInfo *info = graph.getDataInfo(*it, n);
				assert(info != 0);
				Attribute content(n, *info);
				rl.attrs.push_back(content);
				if (q == DTDGraph::Q_OPT) {
					string presentName = n + PRESENT_SUFFIX;
					Attribute present(presentName, PRESENT_DOMAIN);
					rl.attrs.push_back(present);
				}

			} else if (DTDGraph::isPCData(n)) {

				if (q == DTDGraph::Q_ONCE) {
					// attribute for content (#PCDATA)
					const DTDGraph::DataInfo *info = graph.getDataInfo(*it);
					assert(info != 0);
					Attribute content(DATA_NAME, *info);
					rl.attrs.push_back(content);
				} else if (q == DTDGraph::Q_MULT) {
					// table-in-the-middle relation that links this relation's 
					// tuple's ID to all IDs of tuples in the data-relation
					string middleName = makeMiddleName(*it, DATA_NAME);
					Relation middle(middleName);
					Attribute middleId(ID_NAME, ID_DOMAIN); // used for sorting
					Attribute parent(*it, ID_DOMAIN);
					parent.foreign = true;
					Attribute child(DATA_NAME, ID_DOMAIN);
					middle.attrs.push_back(middleId);
					middle.attrs.push_back(parent);
					middle.attrs.push_back(child);
					middleRelations.push_back(middle);
				} else {
					throw MappingException(EXC_ARGS);
				}

			} else if (DTDGraph::isAny(n)) {

				// attribute for content (#PCDATA)
				const DTDGraph::DataInfo *info = graph.getDataInfo(*it);
				assert(info != 0);
				Attribute content(DATA_NAME, *info);
				rl.attrs.push_back(content);

			} else {
				throw MappingException(EXC_ARGS);
			}
		}
		relations.push_back(rl);
		// now insert middle-relations of rl (do this now and not earlier
		// because they contain FOREIGN KEY references to rl)
		relations.insert(relations.end(), middleRelations.begin(),
				middleRelations.end());
	}
} // }}}

void NaiveMapping::create() { // {{{
	vector<string> statements;
	for (vector<Relation>::const_iterator it = relations.begin();
			it != relations.end(); it++) {
		const Relation &rl = *it;
		vector<int> indexAttrs;
		stringstream stmt;
		stmt << "CREATE TABLE " << rl.name << " (";
		for (vector<Attribute>::const_iterator jt = rl.attrs.begin();
				jt != rl.attrs.end(); jt++) {
			const Attribute &attr = *jt;
			stmt << attr.name << " "
				<< domainToDeclaration(attr.domain, attr.size);
			if (attr.name == ID_NAME)
				stmt << " PRIMARY KEY";
			if (attr.foreign)
				stmt << " FOREIGN KEY(" << attr.name << "," << ID_NAME << ")";
			if (jt+1 != rl.attrs.end())
				stmt << ", ";
			if (attr.id)
				indexAttrs.push_back(jt - rl.attrs.begin());
		}
		stmt << ");";
		statements.push_back(stmt.str());
		for (vector<int>::const_iterator jt = indexAttrs.begin();
				jt != indexAttrs.end(); jt++) {
			stringstream stmt;
			stmt << "CREATE INDEX ON " << rl.name
				<< " (" << rl.attrs[*jt].name << ");";
			statements.push_back(stmt.str());
		}
	}
	for_each(statements.begin(), statements.end(), DB::executeDefinition);
} // }}}

static unsigned int counter = 0;
class NaiveMapping::Insertion { // {{{
	public:
		Insertion(const NaiveMapping *owner) : owner(owner) {
		}

		~Insertion() {
		}

		void setName(const string &name) {
			this->name = name;
			this->id = counter++;
		}

		const string &getName() const {
			return name;
		}

		const Id *getId() const {
			return &id;
		}

		void setValue(const string &attr, const string &str) {
			dataMap[attr] = str;
		}

		void setValue(const string &attr, const Id *id) {
			idMap.insert(make_pair(attr, id));
		}

		void setValue(const string &attr, bool isPresent) {
			presentMap[attr] = isPresent;
		}

		map<string, string> *getDataMap() {
			return &dataMap;
		}

		multimap<string, const Id *> *getIdMap() {
			return &idMap;
		}

		map<string, bool> *getPresentMap() {
			return &presentMap;
		}

		bool operator==(const Insertion &i2) const {
			const Insertion &i1 = *this;
			return i1.name == i2.name && i1.dataMap == i2.dataMap
				&& i1.idMap == i2.idMap;
		}

		void setAttributeAbsences() {
			// Sorry for that code, I'm tired:
			vector<const DTDGraph::Child *> children
				= owner->graph.getChildren(name);
			for (vector<const DTDGraph::Child *>::const_iterator it 
					= children.begin(); it != children.end(); it++) {
				const DTDGraph::Child &child = **it;
				if (child.quantifier == DTDGraph::Q_OPT
						&& presentMap.find(child.dest) == presentMap.end()) {
					DB::Domain domain
						= owner->graph.getDataInfo(name, child.dest)->domain;
					if (DTDGraph::isElement(child.dest)) {
						setValue(child.dest, static_cast<Id *>(0));
					} else if (domain == DB::STRING || domain == DB::BYTES) {
						setValue(child.dest + PRESENT_SUFFIX, false);
						setValue(child.dest, string("(empty)"));
					} else if (domain == DB::INT || domain == DB::UINT
							|| domain == DB::LONG || domain == DB::ULONG) {
						setValue(child.dest + PRESENT_SUFFIX, false);
						setValue(child.dest, string("0"));
					} else if (domain == DB::FLOAT || domain == DB::DOUBLE) {
						setValue(child.dest + PRESENT_SUFFIX, false);
						setValue(child.dest, string("0.0"));
					} else {
						throw MappingException(EXC_ARGS);
					}
				}
			}
		}

		string toStatement() const {
			stringstream os;
			os << "INSERT INTO " << name << " (";

			// this row's ID
			os << name << "." << ID_NAME;
			// the element's DATA and all attributes' CDATA
			for (map<string, string>::const_iterator it = dataMap.begin();
					it != dataMap.end(); it++) {
				os << ", " << name << "." << it->first;
			}
			// all referenced IDs
			for (map<string, const Id *>::const_iterator it
					= idMap.begin(); it != idMap.end(); it++) {
				os << ", " << name << "." << it->first;
			}
			// all present attributes
			for (map<string, bool>::const_iterator it
					= presentMap.begin(); it != presentMap.end(); it++) {
				os << ", " << name << "." << it->first;
			}

			os << ") VALUES (";

			// this row's ID
			os << formatId(*getId());
			// the element's DATA and all attributes' CDATA
			for (map<string, string>::const_iterator it = dataMap.begin();
					it != dataMap.end(); it++) {
				const DTDGraph::DataInfo *info;
				if (it->first == DATA_NAME)
					info = owner->graph.getDataInfo(name);
				else
					info = owner->graph.getDataInfo(name, it->first);
				std::cout << "data " << it->first << " = " << it->second << std::endl;
				os << ", " << formatData(it->second, *info);
			}
			// all referenced IDs
			for (map<string, const Id *>::const_iterator it
					= idMap.begin(); it != idMap.end(); it++) {
				Id id = (it->second != 0) ? *it->second : 0;
				os << ", " << formatId(id);
			}
			// all present attributes
			for (map<string, bool>::const_iterator it
					= presentMap.begin(); it != presentMap.end(); it++) {
				os << ", " << formatPresent(it->second);
			}
			os << ");";
			return os.str();
		}

	private:
		const NaiveMapping *owner;
		string name;
		Id id;
		map<string, string> dataMap;
		multimap<string, const Id *> idMap;
		map<string, bool> presentMap;
}; // }}}

class NaiveMapping::StackElem { // {{{
	public:
		enum Type { ELEM_BEGIN, ELEM_END, PCDATA, INSERTION } type;
		int level;
		string str; // name-of-element or PCDATA
		map<string, string> attrs;
		Insertion *ins;

		static StackElem createBeginElement(const string &name,
				const map<string, string> &attrs, int *indent)  {
			StackElem e;
			e.type = ELEM_BEGIN;
			e.str = name;
			e.attrs = attrs;
			e.level = (*indent)++;
			return e;
		}

		static StackElem createPCData(const string &data) {
			StackElem e;
			e.type = PCDATA;
			e.str = data;
			e.level = -1;
			return e;
		}

		static StackElem createEndElement(const string &name, int *indent) {
			StackElem e;
			e.type = ELEM_END;
			e.str = name;
			e.level = --(*indent);
			return e;
		}

		static StackElem createInsertion(Insertion *ins) {
			StackElem e;
			e.type = INSERTION;
			e.ins = ins;
			e.level = -1;
			return e;
		}

		bool operator==(const StackElem &e2) const {
			const StackElem &e1 = *this;
			if (e1.type != e2.type)
				return false;
			switch (e1.type) {
				case ELEM_BEGIN:
					return e1.str == e2.str && e1.attrs == e2.attrs;
					break;
				case ELEM_END:
					return e1.str == e2.str;
					break;
				case PCDATA:
					return e1.str == e2.str;
					break;
				case INSERTION:
					return *e1.ins == *e2.ins;
					break;
				default:
					return false;
			}
		}

		~StackElem() {
		}

	private:
		StackElem() {}
}; // }}}

NaiveMapping::~NaiveMapping() {
	for (vector<Insertion *>::iterator it = insertions.begin();
			it != insertions.end(); it++)
		delete *it;
}

void NaiveMapping::insertElementBegin(const string &name,
		const map<string, string> &attrs) {
	string nname = DTDGraph::ELEMENT_PREFIX + name;
	map<string, string> nattrs;
	for (map<string, string>::const_iterator it = attrs.begin();
			it != attrs.end(); it++)
		nattrs[DTDGraph::ATTRIBUTE_PREFIX + it->first] = it->second;
	StackElem e = StackElem::createBeginElement(nname, nattrs, &indent);
	stack.push_back(e);
}

void NaiveMapping::insertPCData(const string &data) {
	StackElem e = StackElem::createPCData(data);
	stack.push_back(e);
}

void NaiveMapping::insertElementEnd(const string &name) {
	StackElem e = StackElem::createEndElement(name, &indent);
	stack.push_back(e);
	rollup();
}

unsigned int NaiveMapping::getQuantifier(const string &from, const string &to) {
	return graph.getQuantifier((from != "data") ? from : "#PCDATA",
			(to != "data") ? to : "#PCDATA");
}

void NaiveMapping::rollup() { // {{{
	vector<StackElem> vec;
	vector<Insertion *> insVec;

	// Collect the items from the stack that belong to the lastly 
	// completed element.
	bool breakCond = false;
	for (unsigned int pos = stack.size()-1;
			pos >= 0 && stack.size() > 0 && !breakCond; pos--) {
		StackElem &e = stack[pos];
		vec.push_back(e);

		breakCond = e.level == vec.front().level
			&& e.type == StackElem::ELEM_BEGIN;

		if (e.type == StackElem::INSERTION)
			insVec.push_back(e.ins);
		stack.erase(stack.begin() + pos);
	}

	// Merge the temporary insVec vector with the global insertions vector.
	// The insVec vector contains old insertions whose IDs are referenced by
	// the insertion that will be created in this method.
	// Also create additional attributes (specified / not specified) if the
	// attribute is optional or create an additional insertion if the element
	// might occur multiple times.
	for (vector<Insertion *>::reverse_iterator it = insVec.rbegin();
			it != insVec.rend(); it++) {
		Insertion *i = *it;
		insertions.push_back(i);
		map<string, string> *dataMap = i->getDataMap();
		// present attributes for optional attributes
		for (map<string, string>::iterator jt = dataMap->begin();
				jt != dataMap->end(); jt++) {
			unsigned int q = getQuantifier(i->getName(), jt->first);
			if (DTDGraph::isAttribute(jt->first) && q == DTDGraph::Q_OPT) {
				string presentName = jt->first + PRESENT_SUFFIX;
				i->setValue(presentName, true);
			}
		}
		// table-in-the-middle insertions for multiple elements
		multimap<string, const Id *> *idMap = i->getIdMap();
		for (multimap<string, const Id *>::iterator jt = idMap->begin();
				jt != idMap->end(); jt++) {
			unsigned int q = getQuantifier(i->getName(), jt->first);
			if (q == DTDGraph::Q_MULT) {
				Insertion *middleIns = new Insertion(this);
				middleIns->setName(makeMiddleName(i->getName(), jt->first));
				middleIns->setValue(i->getName(), i->getId());
				middleIns->setValue(jt->first, jt->second);
				insertions.push_back(middleIns);
				idMap->erase(jt);
			}
		}
	}

	// Create the new insertion and push it to the global stack.
	Insertion *ins = new Insertion(this);
	for (vector<StackElem>::reverse_iterator it = vec.rbegin();
			it != vec.rend(); it++) {
		switch (it->type) {
			case StackElem::ELEM_BEGIN:
				assert(*it == vec.back());
				ins->setName(it->str);
				for (map<string, string>::const_iterator jt = it->attrs.begin();
						jt != it->attrs.end(); jt++) {
					ins->setValue(jt->first, jt->second);
				}
				break;
			case StackElem::ELEM_END:
				assert(*it == vec.front());
				break;
			case StackElem::INSERTION:
				ins->setValue(it->ins->getName(), it->ins->getId());
				break;
			case StackElem::PCDATA:
				ins->setValue(DATA_NAME, it->str);
				break;
			default:
				throw MappingException(EXC_ARGS);
		}
	}
	stack.push_back(StackElem::createInsertion(ins));
} // }}}

void NaiveMapping::insert() {
	rollup(); // get root-element insertion out of stack into inserion-vector
	vector<string> statements;
	for (vector<Insertion *>::const_iterator it = insertions.begin();
			it != insertions.end(); it++) {
		Insertion *ins = *it;
		ins->setAttributeAbsences();
		statements.push_back(ins->toStatement());
	}
	for_each(statements.begin(), statements.end(), DB::executeModification);
}

void NaiveMapping::makeXPathExpr(string *query, const string &last,
		const XPath::Node &node) {
}

void NaiveMapping::makeNextLevelQuery(string *query, vector<string> *sortBy,
		const string &from, const string &to) { // {{{
	unsigned int q = getQuantifier(DTDGraph::ELEMENT_PREFIX + from,
			DTDGraph::ELEMENT_PREFIX + to);
	string leftTbl = DTDGraph::ELEMENT_PREFIX + from;
	string rightTbl = DTDGraph::ELEMENT_PREFIX + to;
	if (q == DTDGraph::Q_ONCE) {

		string leftId = leftTbl +"."+ rightTbl;
		string rightId = rightTbl +"."+ ID_NAME;

		*query = "JOIN ("+ *query +"), "+ rightTbl
			+" ON "+ leftId +"="+ rightId;


	} else if (q == DTDGraph::Q_OPT) {

		string leftId = leftTbl +"."+ rightTbl;
		string leftIdPresent = leftId + PRESENT_SUFFIX;
		string rightId = rightTbl +"."+ ID_NAME;

		*query = "SELECT FROM ("+ *query +") WHERE "+ leftIdPresent +"=1";
		*query = "JOIN ("+ *query +"), "+ rightTbl
			+" ON "+ leftId +"="+ rightId;

	} else if (q == DTDGraph::Q_MULT) {

		string middleTbl = makeMiddleName(leftTbl, rightTbl);
		string leftId = leftTbl +"."+ ID_NAME;
		string middleLeftId = middleTbl +"."+ leftTbl;
		string middleRightId = middleTbl +"."+ rightTbl;
		string rightId = rightTbl +"."+ ID_NAME;

		*query = "JOIN ("+ *query +"), "+ middleTbl +" ON "
			+ leftId +"="+ middleLeftId;
		*query = "JOIN ("+ *query +"), "+ rightTbl +" ON "
			+ middleRightId +"="+ rightId;

	}
	sortBy->push_back(rightTbl +"."+ ID_NAME);
} // }}}

string NaiveMapping::getXPathQuery(const XPath &path) {
	XPath::iterator it = path.begin();

	string query = DTDGraph::ELEMENT_PREFIX + it->getNode();
	vector<string> sortBy;
	const string *last = &it->getNode();
	sortBy.push_back(query +"."+ ID_NAME);

	if (it->hasExpr())
		makeXPathExpr(&query, *last, *it);

	for (it++; it != path.end(); it++) {
		if (it->hasExpr())
			makeXPathExpr(&query, *last, *it);
		makeNextLevelQuery(&query, &sortBy, *last, it->getNode());
		last = &it->getNode();
	}

	vector<string>::const_iterator jt = sortBy.begin();
	query = "SORT ("+ query +") BY "+ *jt;
	for (jt++; jt != sortBy.end(); jt++) {
		query += ',';
		query += *jt;
	}
	query += ';';
	return query;
}

XMLElement NaiveMapping::buildXMLElement(const string &elem,
		const DB::Iterator &tp) {
	vector<const DTDGraph::Child *> children = graph.getChildren(elem);
	XMLElement e(stripPrefix(elem));

	for (vector<const DTDGraph::Child *>::const_iterator it = children.begin();
			it != children.end(); it++) {
		const DTDGraph::Child &child = **it;

		if (DTDGraph::isElement(child.dest)) {
			string query;
			if (child.quantifier == DTDGraph::Q_ONCE
					|| child.quantifier == DTDGraph::Q_OPT) {
				DB::Value idValue = tp[make_pair(elem, child.dest)];
				string idStr = idValue.toString();
				query = "SELECT FROM "+ child.dest
					+" WHERE "+ child.dest +'.'+ ID_NAME +"="+ idStr +';';
			} else if (child.quantifier == DTDGraph::Q_MULT) {
				DB::Value idValue = tp[make_pair(elem, ID_NAME)];
				string idStr = idValue.toString();
				string middle = makeMiddleName(elem, child.dest);
				query = "SELECT FROM "+ middle +" WHERE "
					+ middle +'.'+ elem +"="+ idStr;
				query = "JOIN ("+ query +"), "+ child.dest +" ON "
					+ middle +'.'+ child.dest +'='+ child.dest +'.'+ ID_NAME;
				query = "SORT ("+ query +") BY "+ child.dest +'.'+ ID_NAME +';';
			} else 
				throw MappingException(EXC_ARGS);
			vector<XMLElement> elems = buildXMLFragment(child.dest, query);
			for (vector<XMLElement>::const_iterator jt = elems.begin();
					jt != elems.end(); jt++)
				e.addElement(*jt);
		} else if (DTDGraph::isAttribute(child.dest)) {
			if (child.quantifier == DTDGraph::Q_ONCE) {
				DB::Value v = tp[make_pair(elem, child.dest)];
				e.addAttribute(stripPrefix(child.dest), v.toString());
			} else if (child.quantifier == DTDGraph::Q_OPT) {
				DB::Value v = tp[make_pair(elem, child.dest + PRESENT_SUFFIX)];
				if (v.getInt() != 0) {
					DB::Value v = tp[make_pair(elem, child.dest)];
					e.addAttribute(stripPrefix(child.dest), v.toString());
				}
			} else 
				throw MappingException(EXC_ARGS);
		} else if (DTDGraph::isPCData(child.dest)) {
			DB::Value v = tp[make_pair(elem, DATA_NAME)];
			e.setPCData(v.toString());
		} else
			throw MappingException(EXC_ARGS);

	}
	return e;
}

vector<XMLElement> NaiveMapping::buildXMLFragment(const string &elem,
		const string &query) {
	vector<XMLElement> v;

	DB::QueryResult r = DB::executeQuery(query.c_str());
	for (DB::Iterator it = r.getIterator(); it.next(); ) {
		XMLElement e = buildXMLElement(elem, it);
		v.push_back(e);
	}
	return v;
}

vector<XMLElement> NaiveMapping::search(const XPath &path) {
	if (path.size() == 0)
		throw XPath::PathException("Path is empty", EXC_ARGS);

	vector<string> sortBy;
	string last = DTDGraph::ELEMENT_PREFIX + path[path.size()-1].getNode();
	string query = getXPathQuery(path);

	return buildXMLFragment(last, query);
}

}

