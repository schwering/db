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

#ifndef XPATH_HPP
#define XPATH_HPP

#include "Exception.hpp"
#include <string>
#include <vector>

namespace dingsbumsxml {

using std::string;
using std::vector;

class XPath {
	public:
		class Expr {
			public:
				enum ComparType { LT, LEQ, EQ, NEQ, GEQ, GT };

				Expr(const string &attr, ComparType compar, const string &value)
					: attr(attr), compar(compar), value(value) { }

				const string &getAttrbute() const { return attr; }
				ComparType getComparision() const { return compar; }
				const string &getValue() const { return value; }

			private:
				string attr;
				ComparType compar;
				string value;
		};

		class Node {
			public:
				Node() : node(""), expr(0) { }
				Node(const string &node) : node(node), expr(0) { }
				Node(const string &node, const Expr &expr)
					: node(node), expr(new Expr(expr)) { }
				Node(const Node &node)
					: node(node.node), expr(0) {
					if (node.expr)
						expr = new Expr(*node.expr);
				}

				Node &operator=(const Node &node) {
					if (this == &node)
						return *this;
					this->node = node.node;
					this->expr = (node.expr) ? new Expr(*node.expr) : 0;
					return *this;
				}

				~Node() { if (expr) delete expr; }

				const string &getNode() const { return node; }
				bool hasExpr() const { return expr != 0; }
				const Expr &getExpr() const { return *expr; }

			private:
				string node;
				Expr *expr;
		};

		typedef CustomException<XPath> PathException;

		typedef vector<Node>::const_iterator iterator;
		typedef vector<Node>::const_reverse_iterator reverse_iterator;

		XPath() { }

		void push_back(const Node &node) { nodes.push_back(node); }
		iterator begin() const { return nodes.begin(); }
		iterator end() const { return nodes.end(); }
		reverse_iterator rbegin() const { return nodes.rbegin(); }
		reverse_iterator rend() const { return nodes.rend(); }
		unsigned int size() const { return nodes.size(); }
		const Node &operator[](unsigned int i) const { return nodes[i]; }

		static XPath parse(const string &s);

	private:
		vector<Node> nodes;
};

}

#endif

