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

#ifndef XMLELEMENT_HPP
#define XMLELEMENT_HPP

#include <iostream>
#include <string>
#include <vector>

namespace dingsbumsxml {

using std::ostream;
using std::string;
using std::vector;

class XMLElement {
	public:
		class XMLAttribute {
			public:
				XMLAttribute(const string &name) : name(name) { }
				XMLAttribute(const string &name, const string &value)
					: name(name), value(value) { }
				
				const string &getName() const { return name; }
				void setValue(const string &val) { value = val; }
				const string &getValue() const { return value; }

				friend ostream &operator<<(ostream &out, const XMLAttribute &e);

			private:
				string name;
				string value;
		};

		XMLElement() { }
		XMLElement(const string &name) : name(name) { }
		XMLElement(const string &name, const string &data)
			: name(name), data(data) { }

		const string &getName() const { return name; }
		void setPCData(const string &data) { this->data = data; }
		const string &getPCData() const { return data; }
		void addElement(const XMLElement &elem) { elems.push_back(elem); }
		const vector<XMLElement> &getElements() const { return elems; }
		void addAttribute(const string &name, const string &value) {
			attrs.push_back(XMLAttribute(name, value)); }
		void addAttribute(const XMLAttribute &attr) { attrs.push_back(attr); }
		const vector<XMLAttribute> &getAttributes() const { return attrs; }

		friend ostream &operator<<(ostream &out, const XMLElement &e);

	private:
		string name;
		string data;
		vector<XMLElement> elems;
		vector<XMLAttribute> attrs;
};

}

#endif

