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
 * These functions perform semantic checks on DDL/DML statements.
 * The code was tedious to write and is very ugly.
 */

#ifndef __VERIF_H__
#define __VERIF_H__

#include "ddl.h"
#include "dml.h"
#include <stdbool.h>

bool ddl_stmt_verify(struct ddl_stmt *ptr); 
bool dml_modi_verify(struct dml_modi *ptr); 
bool dml_sp_verify(struct dml_sp *ptr); 
bool dml_query_verify(struct dml_query *ptr); 

#endif

