/*
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

/*
 * Flex-source-file for query language lexical analyzer.
 */

%{

#include "constants.h"
#include "mem.h"
#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* XXX Workaround see parser.y */
#define id			(ids[current])
extern int current;
extern mid_t ids[];

%}

whitespace	[ \t\n]+
float		[-]{0,1}[0-9]+(\.)[0-9]+[F|f]
double		[-]{0,1}[0-9]+(\.)[0-9]+
int		[-]{0,1}[0-9]+
uint		[0-9]+[U|u]
long		[-]{0,1}[0-9]+[L|l]
ulong		[0-9]+[U|u][L|l]
string		"'"+("\\'"|[^'])*+"'"
symbol		[a-zA-Z_][a-zA-Z0-9_]*

%%

"INT"		{ return TOK_TYPE_INT; }
"UINT"		{ return TOK_TYPE_UINT; }
"LONG"		{ return TOK_TYPE_LONG; }
"ULONG"		{ return TOK_TYPE_ULONG; }
"FLOAT"		{ return TOK_TYPE_FLOAT; }
"DOUBLE"	{ return TOK_TYPE_DOUBLE; }
"STRING"	{ return TOK_TYPE_STRING; }
"BYTES"		{ return TOK_TYPE_BYTES; }

"CREATE"	{ return TOK_CREATE; }
"DROP"		{ return TOK_DROP; }
"TABLE"		{ return TOK_TABLE; }
"INDEX"		{ return TOK_INDEX; }
"VIEW"		{ return TOK_VIEW; }
"SELECT"	{ return TOK_SELECT; }
"PROJECT"	{ return TOK_PROJECT; }
"UPDATE"	{ return TOK_UPDATE; }
"UNION"		{ return TOK_UNION; }
"DELETE"	{ return TOK_DELETE; }
"INSERT"	{ return TOK_INSERT; }
"JOIN"		{ return TOK_JOIN; }
"SORT"		{ return TOK_SORT; }

"*"		{ return TOK_WILDCARD; }
"FROM"		{ return TOK_FROM; }
"WHERE"		{ return TOK_WHERE; }
"AS"		{ return TOK_AS; }
"ON"		{ return TOK_ON; }
"OVER"		{ return TOK_OVER; }
"BY"		{ return TOK_BY; }
"ASC"		{ return TOK_ASC; }
"DESC"		{ return TOK_DESC; }
"SET"		{ return TOK_SET; }
"VALUES"	{ return TOK_VALUES; }
"INTO"		{ return TOK_INTO; }
"PRIMARY KEY"	{ return TOK_PRIMARY_KEY; }
"FOREIGN KEY"	{ return TOK_FOREIGN_KEY; }

"AND"		{ return TOK_AND; }
"OR"		{ return TOK_OR; }

"="		{ return TOK_EQ; }
"<="		{ return TOK_LEQ; }
">="		{ return TOK_GEQ; }
"<"		{ return TOK_LT; }
">"		{ return TOK_GT; }
"!="		{ return TOK_NEQ; }

";"		{ return TOK_EOQ; }

{symbol}	{
			char *ptr;
			
			if (id == -1)
				id = gnew();
			ptr = gmalloc(strlen(qltext) + 1, id);
			strcpy(ptr, qltext);

			qllval.string_ptr = ptr;
			return TOK_SYMBOL;
		}

{string}	{
			char *s, *ptr;
			int i, j, len;

			/* a string is "'blabla'" (without ""); we want to 
			 * skip the leading ' and the trailing '. */
			s = qltext + 1;
			len = strlen(s) - 1;
			s[len] = '\0';

			if (id == -1)
				id = gnew();
			ptr = gmalloc(len + 1, id);
			for (i = 0, j = 0; i < len; i++, j++) {
				if (s[i] == '\\' && s[i+1] == '\'')
					i++;
				ptr[j] = s[i];
			}
			ptr[j] = '\0';

			qllval.string_ptr = ptr;
			return TOK_STRING;
		}

{float}		{
			db_float_t val;
			
			sscanf(qltext, DB_FLOAT_FMT, &val);

			qllval.float_val = val;
			return TOK_FLOAT;
		}

{double}		{
			db_double_t val;
			
			sscanf(qltext, DB_DOUBLE_FMT, &val);

			qllval.double_val = val;
			return TOK_DOUBLE;
		}

{int}		{
			db_int_t val;
			
			sscanf(qltext, DB_INT_FMT, &val);

			qllval.int_val = val;
			return TOK_INT;
		}

{uint}		{
			db_uint_t val;
			
			sscanf(qltext, DB_UINT_FMT, &val);

			qllval.uint_val = val;
			return TOK_UINT;
		}

{long}		{
			db_long_t val;
			
			sscanf(qltext, DB_LONG_FMT, &val);

			qllval.long_val = val;
			return TOK_LONG;
		}

{ulong}		{
			db_ulong_t val;
			
			sscanf(qltext, DB_ULONG_FMT, &val);

			qllval.ulong_val = val;
			return TOK_ULONG;
		}

{whitespace}	{ }

.		{ return *qltext; }

%%

/* What is this function for? */
int qlwrap(void)
{
	return 1;
}

/* This frees some memory... I have no clue why. See
 * http://flex.sourceforge.net/manual/Memory-leak-_002d-16386-bytes-allocated-
 * by-malloc_002e.html#faq%2dmemory%2dleak */
void dql_scanner_cleanup(void)
{
	yy_delete_buffer(YY_CURRENT_BUFFER);
	yy_init = 1;
}

