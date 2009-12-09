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

#ifndef HANDLER_HPP
#define HANDLER_HPP

#include "Mapping.hpp"
#include "NaiveMapping.hpp"
#include "XMLStringUtils.hpp"
#include <xercesc/sax/SAXParseException.hpp>
#include <xercesc/sax2/Attributes.hpp>
#include <xercesc/sax2/DefaultHandler.hpp>
#include <xercesc/util/XMLString.hpp>
#include <sstream>

namespace dingsbumsxml {

using std::stringstream;
using xercesc::SAXParseException;
using xercesc::Attributes;
using xercesc::DefaultHandler;
using xercesc::Locator;
using xercesc::XMLString;

template <class MAPPING_TYPE>
class Handler : public DefaultHandler {
	public:
		Handler() : graph(0), skipChars(false) {
		}
		
		~Handler() {
			delete graph;
			delete mapping;
		}
		
		void startDTD(const XMLCh *name, const XMLCh *publicId,
				const XMLCh *systemId) {
			graph = new DTDGraph();
		}

		void elementDecl(const XMLCh *eName, const XMLCh *model) {
			graph->addElementNode(eName, model);
		}

		void attributeDecl(const XMLCh *eName, const XMLCh *aName,
				const XMLCh *type, const XMLCh *mode, const XMLCh *value) {
			graph->addAttributeNode(eName, aName, type, mode, value);
		}

		void endDTD() {
			mapping = new MAPPING_TYPE(*graph);
		}

		void startElement(const XMLCh *uri, const XMLCh *localname,
				const XMLCh *qname, const Attributes &attrs) {
			string *name = newString(localname);
			map<string, string> attrMap;
			for (unsigned int i = 0; i < attrs.getLength(); i++) {
				string *key = newString(attrs.getLocalName(i));
				if (key->find(DTDGraph::DB_TYPE_PREFIX) != 0) {
					string *val = newString(attrs.getValue(i));
					attrMap[*key] = *val;
					delete val;
				}
				delete key;
			}
			mapping->insertElementBegin(*name, attrMap);
			charStream.str(string(""));
			delete name;
		}
		
		void endElement(const XMLCh *uri, const XMLCh *localname,
				const XMLCh *qname) {
			string *elem = newString(localname);
			elem->insert(0, DTDGraph::ELEMENT_PREFIX);
			vector<const DTDGraph::Child *> children
				= graph->getChildren(*elem);
			bool hasPCDataChild = false;
			for (vector<const DTDGraph::Child *>::const_iterator it
					= children.begin(); it != children.end(); it++) {
				if ((*it)->dest == string("#PCDATA")) {
					hasPCDataChild = true;
				}
			}

			if (hasPCDataChild) {
				string str(charStream.str());
				mapping->insertPCData(str);
			}

			mapping->insertElementEnd(*elem);
			delete elem;
		}
		
		void characters(const XMLCh *chars, unsigned int length) {
			if (!skipChars)
				charStream << chars;
		}

		void startEntity(const XMLCh *name) {
			skipChars = true;
			charStream << "&" << name << ";";
		}

		void endEntity(const XMLCh *name) {
			skipChars = false;
		}

		void warning(const SAXParseException &exc) {
			std::cerr << "Warning: " << exc.getMessage() << std::endl;
		}

		void error(const SAXParseException &exc) {
			std::cerr << "Error: " << exc.getMessage() << std::endl;
		}

		void fatalError(const SAXParseException &exc) {
			std::cerr << "Fatal error: " << exc.getMessage() << std::endl;
		}

		const DTDGraph *getDTDGraph() const {
			return graph;
		}

		MAPPING_TYPE *getMapping() const {
			return mapping;
		}

	private:
		DTDGraph *graph;
		MAPPING_TYPE *mapping;
		bool skipChars;
		stringstream charStream;
};

}

#endif

