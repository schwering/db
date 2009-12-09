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
 * Stored Procedure compiler.
 */

#include <sp.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
	int i;

	if (argc <= 1) {
		printf("Usage: %s <source-filename>\n",
				argv[0]);
		return 2;
	}

	for (i = 1; i < argc; i++) {
		unsigned int size;
		FILE *fp;
		char *src, *prog;

		src = argv[i];
		fp = fopen(src, "r");
		if (!fp) {
			printf("Invalid source file %s.\n", src);
			return 1;
		}
		prog = NULL;
		size = 0;
		while (!feof(fp)) {
			prog = realloc(prog, (size + 100 + 1) * sizeof(char));
			size += fread(prog + size, sizeof(char), 100, fp);
			prog[size] = '\0';
		}
		printf("---------------------------------------------------\n");
		printf("Program code:\n");
		printf("%s\n", prog);
		printf("---------------------------------------------------\n");
		printf("\n");
		printf("Compiling %s ... \n", src);
		if (sp_compile(prog))
			printf("done\n");
		else {
			printf("failed\n");
			errprint();
			fclose(fp);
			return 1;
		}
		printf("---------------------------------------------------\n");
		fclose(fp);
	}
	return 0;
}

