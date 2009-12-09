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

#ifndef MY_VERY_OWN_EXCEPTION_HPP
#define MY_VERY_OWN_EXCEPTION_HPP

#include <iostream>

namespace dingsbumsxml {

#define EXC_ARGS	__FILE__,__LINE__

class Exception {
	public:
		virtual ~Exception() { }
		virtual const char *getMessage() const = 0;
		virtual const char *getFile() const = 0;
		virtual unsigned int getLine() const = 0;
		virtual void print() const = 0;
	protected:
		Exception() { }
};

template <class T>
class CustomException : public Exception {
	public:
		CustomException() : msg(0), file(0), line(0) { }
		CustomException(const char *file, unsigned int line)
			: msg(0), file(file), line(line) { }
		CustomException(const char *msg, const char *file, unsigned int line)
			: msg(msg), file(file), line(line) { }

		const char *getMessage() const { return msg ? msg : ""; }
		const char *getFile() const { return file ? file : ""; }
		unsigned int getLine() const { return line; }

		void print() const { 
			const char *f = (file != 0) ? file : "(no file set)";
			const char *m = (msg != 0) ? msg : "(no msg set)";
			std::cerr << "Exception at " << f << ':' << line << ": " << m
				<< std::endl;;
		}

	private:
		const char *msg;
		const char *file;
		unsigned int line;
};

}

#endif

