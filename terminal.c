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
 * A terminal for interactive query input.
 */

#include <db.h>
#include <err.h>
#include <hashset.h>
#include <hashtable.h>
#include <mem.h>
#include <sort.h>
#include <str.h>

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>

#ifdef MALLOC_TRACE
#include <mcheck.h>
#endif

#define BUFSIZE		2048
#define HISTSIZE	20

#define IS_WHITE(c)	(c == ' ' || c == '\r' || c == '\n' || c == '\t')
#define TO_LOWER(c)	((c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c)
#define IS_ALPHA(c)	((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
#define IS_NUM(c)	(c >= '0' && c <= '9')

static void prompt(void)
{
	printf("$ ");
}

static void trim(char *s)
{
	int i = strlen(s) - 1;
	while (i >= 0 && (s[i] == '\r' || s[i] == '\n'))
		s[i--] = '\0';
}

static bool is_num(const char *str)
{
	const char *s;

	for (s = str; '0' <= *s && *s <= '9'; s++)
		;
	return *s == '\0' && s > str;
}

static void print_help_index(void)
{
	DIR *dir;
	struct dirent *ent;
	char **files;
	int i, cnt;

	dir = opendir("help");
	if (!dir) {
		fprintf(stderr, "No help available.\n");
		return;
	}

	files = NULL;
	cnt = 0;
	while ((ent = readdir(dir)) != NULL) {
		int len;

		len = strlen(ent->d_name);
		if (len > 4 && !strncmp(ent->d_name+len-4, ".hlp", 4)) {
			char *s;

			ent->d_name[len - 4] = '\0';
			for (s = ent->d_name; *s; s++)
				if (*s == '_')
					*s = ' ';

			files = realloc(files, ++cnt * sizeof(char *));
			files[cnt-1] = malloc((strlen(ent->d_name)+1)
					* sizeof(char));
			strcpy(files[cnt-1], ent->d_name);
		}
	}
	selection_sort((void **)files, cnt,
			(int (*)(const void *, const void *))strcmp);

	printf("The following help sections are available:\n");
	for (i = 0; i < cnt; i++) {
		printf("\t* %s\n", files[i]);
		free(files[i]);
	}
	free(files);
	printf("Try `help <section>' for more information.\n");
	closedir(dir);
}

static void print_general_help(void)
{
	printf("This is %s %s.\n", DB_NAME, DB_VERSION);
	printf("This program is at early development, so don't expect much.\n");
	printf("\n");
	printf("AVAILABLE DATABASE COMMANDS:\n");
	printf("\t* CREATE and DROP TABLE\n");
	printf("\t* CREATE and DROP INDEX\n");
	printf("\t* CREATE and DROP VIEW\n");
	printf("\t* INSERT\n");
	printf("\t* UPDATE\n");
	printf("\t* DELETE\n");
	printf("\t* SELECT\n");
	printf("\t* PROEJCT\n");
	printf("\t* JOIN\n");
	printf("\t* UNION\n");
	printf("\t* SORT\n");
	printf("\t* AVG, VAR, COUNT, MAX, MIN, SUM\n");
	printf("Try typing `help <command>' for more information (e.g. `help "\
			"create index').\n");
	printf("\n");
	printf("FURTHER TERMINAL COMMANDS:\n");
	printf("\t* @<filename>\texecute batch file\n");
	printf("\t* !<command>\texecute shell command\n");
	printf("\t* #<n>\t\texecute last <n>th command (1 <= n <= %d)\n", HISTSIZE);
	printf("\t* ##\t\tprint command history\n");
	printf("\t* help\t\tyou've already found it\n");
	printf("\t* help-index\ttell what help is available\n");
	printf("\t* copying\tlicense information\n");
	printf("\t* store V\tstore the count of affected tuples of the "\
			"last statement\n");
	printf("\t* echo V\tprint the value of the respective variable\n");
	printf("\t* assert V R W\tcheck that V and W stand "\
			"in relation R\n");
	printf("\t* profiling-on\tenables profiling mode (see below)\n");
	printf("\t* profiling-off\tdisables profiling mode (default)\n");
	printf("\t* errors\tprint error trace\n");
	printf("\t* clearerrors\tdelete all registered errors\n");
#ifdef MEMDEBUG
	printf("\t* memory\tprints memory information\n");
#endif
	printf("\t* exit\t\tclose open files and exit cleanly\n");
	printf("\n");
	printf("In the profiling mode the execution time is determined and "\
			"printed.");
	printf("\n");
	printf("This is program is free software under a two-clause BSD-style "\
			"license.\n");
	printf("Type `copying' for more information.\n");
	printf("\n");
	printf("schwering@gmail.com\n");
}

static void print_help(const char *cmd)
{
	const char *s;
	char name[32], path[64], c;
	FILE *fp;

	if (cmd == NULL) {
		print_general_help();
		return;
	}

	s = cmd;
	do {
		if ((s - cmd) > 32) {
			fprintf(stderr, "Too long help keyword: `%s'\n", cmd);
			return;
		}
		if (*s == '/' || *s == '\\' || *s == '.') {
			fprintf(stderr, "Invalid character in help keyword: "\
					"`%s'\n", cmd);
			return;
		}
		name[s-cmd] = (IS_WHITE(*s)) ? '_' : TO_LOWER(*s);
	} while (*s++);

	strcpy(path, "help/");
	strcpy(path+strlen(path), name);
	strcpy(path+strlen(path), ".hlp");

	fp = fopen(path, "r");
	if (fp == NULL) {
		fprintf(stderr, "No help for keyword exists: `%s' (%s)\n",
				cmd, path);
		return;
	}

	while (fread(&c, sizeof(char), 1, fp) == 1)
		fputc(c, stdout);
	fclose(fp);
}

static void print_copying(void)
{
	printf(
"Copyright (c) 2006, 2007 Christoph Schwering <schwering@gmail.com>\n"\
"\n"\
"Permission to use, copy, modify, and distribute this software for any\n"\
"purpose with or without fee is hereby granted, provided that the above\n"\
"copyright notice and this permission notice appear in all copies.\n"\
"\n"\
"THE SOFTWARE IS PROVIDED \"AS IS\" AND THE AUTHOR DISCLAIMS ALL WARRANTIES\n"\
"WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF\n"\
"MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR\n"\
"ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES\n"\
"WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN\n"\
"ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF\n"\
"OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.\n"
	);
}

static unsigned long last_tpcnt = 0;
static mid_t symbol_mem_id = -1;
static struct hashtable *symbol_table = NULL;

static void store_symbol(char *symbol)
{
	char *key;
	unsigned long *val;

	if (symbol_table == NULL) {
		symbol_mem_id = gnew();
		symbol_table = table_init(17,
				(int (*)(void *))strhash,
				(bool (*)(void *, void *))strequals);
		assert(symbol_table != NULL);
	}

	key = copy_gc(symbol, strsize(symbol), symbol_mem_id);
	val = copy_gc(&last_tpcnt, sizeof(unsigned long), symbol_mem_id);
	table_insert(symbol_table, key, val);
}

static void store_count(char *cmd)
{
	char *symbol, *stmt;
	unsigned long tpcnt;
	DB_RESULT result;
	DB_ITERATOR iter;

	symbol = cmd;
	for (stmt = cmd; *stmt; stmt++)
		if (IS_WHITE(*stmt))
			break;
	*stmt = '\0';
	stmt++;

	tpcnt  = 0;
	result = db_parse(stmt);
	if (db_success(result) && db_is_query(result)) {
		iter = db_iterator(result);
		while (db_next_buf(iter) != NULL)
			tpcnt++;
		db_free_iterator(iter);
		last_tpcnt = tpcnt;
		store_symbol(symbol);
	} else {
		fprintf(stderr, "Statement could not be executed or was no"\
				"query: %s", stmt);
	}
	db_free_result(result);
}

static bool load_symbol(char *symbol, unsigned long *dest)
{
	unsigned long *ptr;

	assert(dest != NULL);

	if (symbol_table != NULL
			&& (ptr = table_search(symbol_table, symbol)) != NULL) {
		*dest = *ptr;
		return true;
	} else
		return false;
}

static bool parse_expr(char *symbol, unsigned long *dest)
{
	char *str, operator, *left_operand, *right_operand;
	long unsigned left_operand_val, right_operand_val;

	left_operand = symbol;
	operator = 0;
	right_operand = NULL;
	for (str = symbol; *str; str++) {
		if (*str == '+' || *str == '-' || *str == '*' || *str == '/') {
			operator = *str;
			*str = '\0';
			right_operand = str + 1;
			break;
		}
	}

	if (IS_NUM(*left_operand))
		sscanf(left_operand, "%lu", &left_operand_val);
	else if (!load_symbol(left_operand, &left_operand_val))
		return false;

	if (operator != 0) {
		if (IS_NUM(*right_operand))
			sscanf(right_operand, "%lu", &right_operand_val);
		else if (!load_symbol(right_operand, &right_operand_val))
			return false;
	}

	switch (operator) {
		case '+':
			*dest = left_operand_val + right_operand_val;
			break;
		case '-':
			*dest = left_operand_val - right_operand_val;
			break;
		case '*':
			*dest = left_operand_val * right_operand_val;
			break;
		case '/':
			*dest = left_operand_val / right_operand_val;
			break;
		default:
			*dest = left_operand_val;
	}
	return true;
}

static void echo_symbol(char *symbol)
{
	unsigned long val;

	assert(symbol != NULL);

	if (load_symbol(symbol, &val))
		printf("%s = %lu\n", symbol, val);
	else
		fprintf(stderr, "Unknown variable: %s\n", symbol);
}

static void compile_assertion(const char *assertion)
{
	char left_operand_str[128], right_operand_str[128];
	char operator[128];
	unsigned long left_operand, right_operand;
	int i;
	bool b;

	assert(assertion != NULL);
	assert(strlen(assertion) < 128);

	i = sscanf(assertion, "%s %s %s", left_operand_str, operator,
			right_operand_str);
	if (i != 3) {
		fprintf(stderr, "Wrong format of assertion.\n");
		return;
	}

	if (!parse_expr(left_operand_str, &left_operand)) {
		fprintf(stderr, "Unknown variable or expression: %s\n",
				left_operand_str);
		return;
	}
	if (!parse_expr(right_operand_str, &right_operand)) {
		fprintf(stderr, "Unknown variable or expression: %s\n",
				right_operand_str);
		return;
	}

	if (!strcmp(operator, "=") || !strcmp(operator, "=="))
		b = (left_operand == right_operand);
	else if (!strcmp(operator, "!="))
		b = (left_operand != right_operand);
	else if (!strcmp(operator, "<"))
		b = (left_operand < right_operand);
	else if (!strcmp(operator, "<="))
		b = (left_operand <= right_operand);
	else if (!strcmp(operator, ">"))
		b = (left_operand > right_operand);
	else if (!strcmp(operator, ">="))
		b = (left_operand >= right_operand);
	else
		b = false;
	if (!b) {
		fprintf(stderr, "Assertion failed: %lu %s %lu\n",
				left_operand, operator, right_operand);
		fprintf(stderr, "Press any key to continue.\n");
		getc(stdin);
	}
}

static void free_symbol_table(void)
{
	if (symbol_table != NULL) {
		table_free(symbol_table);
		gc(symbol_mem_id);
	}
}

static void profile(const char *q)
{
	DB_RESULT result;
	bool success;
	DB_ITERATOR iter;
	clock_t start, end;
	double time;
	unsigned long tpcnt;

	start = clock();

	tpcnt  = 0;
	result = db_parse(q);
	if ((success = db_success(result))) {
		if (db_is_query(result)) {
			iter = db_iterator(result);
			while (db_next_buf(iter) != NULL)
				tpcnt++;
			db_free_iterator(iter);
		} else if (db_is_modification(result)) {
			tpcnt = db_tpcount(result);
		}
	}
	db_free_result(result);

	end = clock();

	time = ((double)(end - start)) / CLOCKS_PER_SEC;
	printf("CPU time: %f (%s, %lu tuples affected)\n", time,
			(success) ? "successful" : "failed",
			(success) ? tpcnt : 0L);
	if (!success)
		errprint();
	last_tpcnt = success ? tpcnt : 0;
}

static void query(const char *q)
{
	DB_RESULT result;

	errclearall();
	result = db_parse(q);
	if (db_success(result))
		last_tpcnt = db_print(result);
	else {
		printf("An error occured while processing the "
				"statement:\n");
		errprint();
		last_tpcnt = 0;
	}
	db_free_result(result);
}

static bool batch(const char *fn);

static bool interpret(char *cmd)
{
	static bool profiling = false;

	if (!strcasecmp(cmd, "exit"))
		return false;
	else if (!strcasecmp(cmd, "help"))
		print_help(NULL);
	else if (strstr(cmd, "help ") == cmd)
		print_help(cmd + strlen("help "));
	else if (!strcasecmp(cmd, "help-index"))
		print_help_index();
	else if (!strcasecmp(cmd, "copying"))
		print_copying();
#ifdef MEMDEBUG
	else if (!strcasecmp(cmd, "memory"))
		memprint();
#endif
	else if (!strcasecmp(cmd, "errors"))
		errprint();
	else if (!strcasecmp(cmd, "clearerrors"))
		errclearall();
	else if (!strcasecmp(cmd, "profiling-off"))
		profiling = false;
	else if (!strcasecmp(cmd, "profiling-on"))
		profiling = true;
	else if (strstr(cmd, "store ") == cmd)
		store_symbol(cmd + strlen("store "));
	else if (strstr(cmd, "count ") == cmd)
		store_count(cmd + strlen("count "));
	else if (strstr(cmd, "echo ") == cmd)
		echo_symbol(cmd + strlen("echo "));
	else if (strstr(cmd, "assert ") == cmd)
		compile_assertion(cmd + strlen("assert "));
	else if (cmd[0] == '@')
		return batch(cmd+1);
	else {
		if (!profiling)
			query(cmd);
		else
			profile(cmd);
	}

	return true;
}

static bool batch(const char *fn)
{
	FILE *fp;
	char buf[BUFSIZE];

	fp = fopen(fn, "r");
	if (fp == NULL) {
		perror("Error");
		return true;
	}
	while (fgets(buf, BUFSIZE, fp) == buf) {
		trim(buf);
		if (strlen(buf) == 0 || buf[0] == '#')
			continue;
		prompt();
		printf("%s\n", buf);
		if (!interpret(buf)) {
			fclose(fp);
			return false;
		}
	}
	fclose(fp);
	return true;
}

static void bye(void)
{
#ifdef MEMDEBUG
	extern struct hashset **chunks;
	extern int chunk_cnt;
	int i;
#endif

	db_cleanup();
#ifdef MEMDEBUG
	for (i = 0; i < chunk_cnt; i++) {
		printf("Not yet free()ed: id=%d\n", i);
		hashset_free(chunks[i]);
	}
	memprint();
#endif
}

int main(int argc, char *argv[])
{
	char buf[BUFSIZE];
	char hist[HISTSIZE][BUFSIZE];
	int listcnt = 0, i;

	if (atexit(bye) != 0) {
		fprintf(stderr, "Cannot set exit function\n");
		exit(1);
	}

#ifdef MALLOC_TRACE
	setenv("MALLOC_TRACE", "malloc_trace", 1);
	mtrace();
#endif

	printf("%s %s\n", DB_NAME, DB_VERSION);

	for (i = 1; i < argc; i++) {
		prompt();
		printf("%s\n", argv[i]);
		if (!interpret(argv[i]))
			goto exit;
	}

	for (;;) {
		int i;
		bool add_to_history = true;

		prompt();
		if (fgets(buf, BUFSIZE, stdin) != buf) {
			perror("\nError");
			continue;
		}
		trim(buf);

command_interpretation:
		if (strlen(buf) == 0) { /* empty input */
			continue;
		} else if (buf[0] == '!') { /* exec shell cmd */
			system(buf+1);
		} else if (buf[0] == '#' && buf[1] == '#') { /* print history */
			add_to_history = false;
			for (i = listcnt-1; i >= 0; i--)
				printf("  #%d\t%s\n", i+1, hist[i]);
		} else if (buf[0] == '#' && is_num(buf+1)) { /* exec history */
			add_to_history = false;
			i = atoi(buf+1) - 1;
			if (i < 0 || i >= listcnt) {
				fprintf(stderr, "Error: index out of range\n");
			} else {
				memcpy(buf, hist[i], BUFSIZE);
				prompt();
				printf("%s\n", buf);
				goto command_interpretation;
			}
		} else {
			if (!interpret(buf))
				goto exit;
		}

		if (add_to_history) {
			for (i = HISTSIZE - 1; i > 0; i--)
				memcpy(hist[i], hist[i-1], BUFSIZE);
			memcpy(hist[0], buf, BUFSIZE);
			if (listcnt < HISTSIZE)
				listcnt++;
		}
	}

exit:
	free_symbol_table();
	db_cleanup();

#ifdef CACHE_STATS
	cache_print_stats();
#endif

	printf("Bye.\n");
	return 0;
}

