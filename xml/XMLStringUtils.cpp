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

#include "XMLStringUtils.hpp"
#include <xercesc/util/XMLString.hpp>

namespace dingsbumsxml {

ostream &operator<<(ostream &stream, const XMLCh *str) {
	if (str != NULL) {
		char *chars = XMLString::transcode(str);
		ostream &retval = stream << chars;
		XMLString::release(&chars);
		return retval;
	} else
		return stream << "NULL";
}

string *newString(const XMLCh *str) {
	char *chars = XMLString::transcode(str);
	string *result = new string(chars);
	XMLString::release(&chars);
	return result;
}

}

