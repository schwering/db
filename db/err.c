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

#include "err.h"
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define STACK_SIZE	20

struct stack_elem {
	int err_code;
	const char *name;
	const char *file;
	int line;
	const char *function;
};

size_t stack_size = 0;

struct stack_elem stack[STACK_SIZE];

void errset(unsigned int no, const char *name, const char *file, int line,
		const char *function)
{
	int i;

	for (i = STACK_SIZE - 2; i >= 0; i--)
		memcpy(&stack[i+1], &stack[i], sizeof(struct stack_elem));
	stack[0].err_code = no;
	stack[0].name = name;
	stack[0].file = file;
	stack[0].line = line;
	stack[0].function = function;
	if (stack_size < STACK_SIZE)
		stack_size++;
}

void errclear(void)
{
	size_t i;

	for (i = 0; i+1 < stack_size; i++)
		memcpy(&stack[i], &stack[i+1], sizeof(struct stack_elem));
	if (stack_size > 0)
		stack_size--;
}

void errclearall(void)
{
	stack_size = 0;
}

int errnumber(unsigned int i)
{
	return (i < stack_size) ? stack[i].err_code : -1;
}

void errprint(void)
{
	size_t i;

	printf("Stack trace:\n");
	if (stack_size == 0)
		printf("\t(You're lucky, no errors in stack)\n");
	for (i = 0; i < stack_size; i++) {
		printf("\t%s (%d) at %s:%d in %s()\n",
				stack[i].name, stack[i].err_code,
				stack[i].file, stack[i].line,
				stack[i].function);
	}
}

