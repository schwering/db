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
#include "DTDGraph.hpp"
#include "NaiveMapping.hpp"
#include "Handler.hpp"
#include "XPath.hpp"
#include "XMLStringUtils.hpp"
#include <fstream>
#include <iostream>
#include <vector>
#include <xercesc/sax2/SAX2XMLReader.hpp>
#include <xercesc/sax2/XMLReaderFactory.hpp>
#include <xercesc/validators/DTD/DTDValidator.hpp>

using namespace std;
using namespace xercesc;
using namespace dingsbumsxml;

int main(int argc, char *argv[])
{
#ifdef MALLOC_TRACE
	setenv("MALLOC_TRACE", "malloc_trace", 1);
	mtrace();
#endif

	string path;
	if (argc >= 2) {
		path = argv[1];
	} else {
		cout << "Enter your favourite XPath: ";
		cin >> path;
	}

	try {
		XMLPlatformUtils::Initialize();
	} catch (const XMLException& exc) {
		cout << "Error during initialization: " << exc.getMessage() << endl;
		return 1;
	}

	char* xmlFile = "catalog.xml";
	SAX2XMLReader* parser = XMLReaderFactory::createXMLReader();
	parser->setFeature(XMLUni::fgSAX2CoreValidation, true);
	parser->setFeature(XMLUni::fgXercesDynamic, false);

	Handler<NaiveMapping> handler;
	parser->setContentHandler(&handler);
	parser->setLexicalHandler(&handler);
	parser->setDeclarationHandler(&handler);
	parser->setErrorHandler(&handler);
	try {
		parser->parse(xmlFile);

		Mapping *mapping = handler.getMapping();
		mapping->create();
		mapping->insert();
		vector<XMLElement> elems = mapping->search(XPath::parse(path));
		ofstream xmlOut("bla.xml");
		for (vector<XMLElement>::const_iterator it = elems.begin();
				it != elems.end(); it++)
			xmlOut << *it << endl;
		xmlOut.close();

		ofstream binOut("blup.bin");
		binOut << *handler.getDTDGraph();
		binOut.close();

		DTDGraph graph;
		ifstream binIn("blup.bin");
		binIn >> graph;
		binIn.close();

		ofstream f("blup.dot");
		graph.draw(f);
		f.close();
		
		cout << (graph == *handler.getDTDGraph()) << endl;
	} catch (const XMLException& exc) {
		cout << "Exception message is: " << exc.getMessage() << endl;
		return -1;
	} catch (const SAXParseException& exc) {
		cout << "Exception message is: " << exc.getMessage() << endl;
		return -1;
	} catch (const Exception &exc) {
		exc.print();
		return -1;
	} catch (...) {
		cout << "Unexpected exception: " << endl;
		return -1;
	}


	delete parser;
	XMLPlatformUtils::Terminate();
	return 0;
}

