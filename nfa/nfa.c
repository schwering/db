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
 * Calculates Nondeterministic Finite Automaton that determines the valid
 * items of a contextfree grammer.
 * Then the NFA's epsilon transitions are eliminated and the resulting
 * NFA is converted into a Deterministic Finite Automaton (DFA) with the
 * powerset construction.
 * The code is not really nice...
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SOURCE_FILE	"../db/sp.c"
//#include SOURCE_FILE
#ifdef ALPHABET_SIZE
	#define HAVE_GENERATED_CODE
#endif

#ifndef SOURCE_FILE
#error "no SOURCE_FILE definition"
#endif

#define EPSILON			"\0"
#define START			"Start"
#define DOT_SYMBOL		'$'
#define BUFSIZE			4096
#define SYMSIZE			64
#define MAXSYMCNT		64
#define IS_NONTERMINAL(str)	(*(str) >= 'A' && *(str) <= 'Z')
#define IS_TERMINAL(str)	(!(IS_NONTERMINAL(str)))
#define IS_WHITE(c)		((c)==' '||(c)=='\t'||(c)=='\n'||(c)=='\r')

#define GENERATED_CODE_BEGIN	"/* BEGIN OF GENERATED CODE -- DO NOT EDIT */"
#define GENERATED_CODE_END	"/* END OF GENERATED CODE -- DO NOT EDIT */"


#ifndef HAVE_GENERATED_CODE
static struct rule { /* rule: V -> X for a nonterminal V and a word of
			terminals and nonterminals X */
	const char * const v;
	const char * const x;
	const char * const funcname;
} rules[] = {
#if 0
	{ "S", "Cc", NULL },
	{ "Cc", "aa bb", NULL },
	{ "Cc", "aa Cc bb", NULL }
#else
	{ "Start", "procedure symbol ( Argdecllist ) Body",
		"rdc_procedure_args" }, 
	{ "Start", "procedure symbol ( ) Body",
		"rdc_procedure_void" }, 
	{ "Argdecllist", "Argdecllist , Decl", "rdc_argdecls" }, 
	{ "Argdecllist", "Decl", "rdc_argdecl" }, 
	{ "Body", "begin Decllist Linelist end", "rdc_body" },
	{ "Decllist", "Decllist Decl ;", "rdc_decls" }, 
	{ "Decllist", "Decl ;", "rdc_decl" }, 
	{ "Decl", "int symbol", "RDC_DECL(T_INT)" }, 
	{ "Decl", "float symbol", "RDC_DECL(T_FLOAT)" }, 
	{ "Decl", "string symbol", "RDC_DECL(T_STRING)" }, 
	{ "Decl", "tuple symbol", "RDC_DECL(T_TUPLE)" }, 
	{ "Decl", "auto symbol", "RDC_DECL(T_AUTO)" }, 
	{ "Block", "Line", "rdc_single_line_block" }, 
	{ "Block", "do Linelist end", "rdc_mult_line_block" }, 
	{ "Linelist", "Linelist Line", "rdc_lines" }, 
	{ "Linelist", "Line", "rdc_line" }, 
	{ "Line", "! symbol ( Arglist ) ;", "rdc_funccall_args" }, 
	{ "Line", "! symbol ( ) ;", "rdc_funccall_void" }, 
	{ "Line", "symbol : = Expr ;", "rdc_assign" }, 
	{ "Line", "return Expr ;", "rdc_return" }, 
	{ "Line", "if ( Expr ) Block", "rdc_if" }, 
	{ "Line", "while ( Expr ) Block", "rdc_while" }, 
	{ "Line", "foreach ( symbol in Expr ) Block", "rdc_foreach" },
	{ "Expr", "( Expr )", "RDC_FWD(2, 3)" }, 
	{ "Expr", "( Expr + Expr )", "RDC_EXPR(O_ADD)" }, 
	{ "Expr", "( Expr - Expr )", "RDC_EXPR(O_SUB)" }, 
	{ "Expr", "( - Expr )", "rdc_expr_minus" }, 
	{ "Expr", "( Expr * Expr )", "RDC_EXPR(O_MULT)" }, 
	{ "Expr", "( Expr / Expr )", "RDC_EXPR(O_DIV)" }, 
	{ "Expr", "( Expr or Expr )", "RDC_EXPR(O_OR)" }, 
	{ "Expr", "( Expr and Expr )", "RDC_EXPR(O_AND)" }, 
	{ "Expr", "( Expr = Expr )", "RDC_EXPR(O_EQ)" }, 
	{ "Expr", "( Expr ! = Expr )", "RDC_EXPR(O_NEQ)" }, 
	{ "Expr", "( Expr < = Expr )", "RDC_EXPR(O_LEQ)" }, 
	{ "Expr", "( Expr < Expr )", "RDC_EXPR(O_LT)" }, 
	{ "Expr", "( Expr > Expr )", "RDC_EXPR(O_GT)" }, 
	{ "Expr", "( Expr > = Expr )", "RDC_EXPR(O_GEQ)" }, 
	{ "Expr", "! symbol ( Arglist )", "rdc_funccall_args" }, 
	{ "Expr", "! symbol ( )", "rdc_funccall_void"  }, 
	{ "Expr", "symbol", "rdc_symbol" }, 
	{ "Expr", "intval", "rdc_int" }, 
	{ "Expr", "floatval", "rdc_float" }, 
	{ "Expr", "stringval", "rdc_string" }, 
	{ "Expr", "tupleval", NULL }, 
	{ "Arglist", "Arglist , Expr", "rdc_args" }, 
	{ "Arglist", "Expr", "rdc_arg" } 
#endif
};
#endif

#define RCNT	((unsigned int)((sizeof rules) / sizeof(struct rule)))

static struct nstate { /* NFA state */
	struct rule *rl;
	int pos;
	bool start;
} nstates[BUFSIZE];

static unsigned int nstate_cnt = 0;

static struct ntransition { /* NFA transition */
	struct nstate *from, *to;
	char a[SYMSIZE];
} ntransitions[BUFSIZE];

static unsigned int ntransition_cnt = 0;

static struct dstate { /* DFA state (powerset construction) */
	struct nstate **nstates;
	int cnt;
} dstates[BUFSIZE];

static unsigned int dstate_cnt = 0;

static struct dtransition { /* DFA transition (powerset construction) */
	struct dstate *from, *to;
	char a[SYMSIZE];
} dtransitions[BUFSIZE];

static unsigned int dtransition_cnt = 0;

#ifndef HAVE_GENERATED_CODE
struct action {
	enum { ERROR = -1, SHIFT, REDUCE, ACCEPT } action;
	struct rule *rl;
};
#endif

static bool get_symbol(char *buf, const struct rule *rl, int pos)
{
	const char *s, *t;

	for (s = rl->x; *s && pos > 0; s++)
		if (IS_WHITE(*s))
			pos--;

	if (!*s)
		return false;

	for (t = s; *t && !IS_WHITE(*t); t++)
		;

	strncpy(buf, s, t - s);
	buf[t-s] = '\0';
	return true;
}

#ifndef HAVE_GENERATED_CODE
static int rulelen(const struct rule *rl)
{
	const char *s;
	int i;

	i = 0;
	for (s = rl->x; *s; s++)
		if (IS_WHITE(*s))
			i++;
	return i+1;
}
#endif

static const char *nstatestr(const struct nstate *st)
{
	static char buf[BUFSIZE];
	char sym[SYMSIZE];
	int i;

	assert(st != NULL);

	sprintf(buf, "%s -> ", st->rl->v);
	for (i = 0; i < st->pos && get_symbol(sym, st->rl, i); i++)
		sprintf(buf+strlen(buf), "%s ", sym);

	sprintf(buf+strlen(buf), "%c ", DOT_SYMBOL);
	for (i = st->pos; get_symbol(sym, st->rl, i); i++)
		sprintf(buf+strlen(buf), "%s ", sym);
	return &buf[0];
}

static const char *dstatestr(const struct dstate *st)
{
	static char buf[BUFSIZE];
	int i;

	assert(st != NULL);
	assert(st->cnt > 0);

	buf[0] = '\0';
	for (i = 0; i < st->cnt-1; i++)
		sprintf(buf + strlen(buf), "%s  |  ",
				nstatestr(st->nstates[i]));
	sprintf(buf + strlen(buf), "%s", nstatestr(st->nstates[i]));
	return &buf[0];
}

static const char *transstr(char *a)
{
	static char buf[SYMSIZE];

	if (!strcmp(a, EPSILON))
		return "eps";
	else {
		sprintf(buf, "'%s'", a);
		return &buf[0];
	}
}

static char alphabet[MAXSYMCNT][SYMSIZE];
static int alphabet_cnt = 0;

static bool is_in_alphabet(const char *s)
{
	int i;

	for (i = 0; i < alphabet_cnt; i++)
		if (!strcmp(alphabet[i], s))
			return true;
	return false;
}

static void determine_alphabet()
{
	static bool already_determined = false;
	int i;

	if (already_determined)
		return;

	for (i = 0; i < RCNT; i++) {
		struct rule *rl;
		char sym[SYMSIZE];
		int j;

		rl = &rules[i];
		if (!is_in_alphabet(rl->v))
			strcpy(alphabet[alphabet_cnt++], rl->v);
		for (j = 0; get_symbol(sym, rl, j); j++)
			if (!is_in_alphabet(sym))
				strcpy(alphabet[alphabet_cnt++], sym);
	}
	already_determined = true;
}

/* CFG-items to NFA */

static struct nstate *find_nstate(struct rule *rl, int pos)
{
	int i;

	for (i = 0; i < nstate_cnt; i++)
		if (nstates[i].rl == rl && nstates[i].pos == pos)
			return &nstates[i];
	return NULL;
}

static struct rule *next_rule(const char *v, int *i)
{
	for (; *i < RCNT; (*i)++)
		if (!strcmp(rules[*i].v, v))
			return &rules[*i];
	return NULL;
}

static struct nstate *new_nstate(struct rule *rl, int pos, bool start)
{
	struct nstate *st;
	int i;

	if ((st = find_nstate(rl, pos)) != NULL)
		return st;

	i = nstate_cnt++;
	nstates[i].rl = rl;
	nstates[i].pos = pos;
	nstates[i].start = start;
	return &nstates[i];
}

static struct ntransition *new_ntransition(struct nstate *from, const char *a,
		struct nstate *to)
{
	int i;

	for (i = 0; i < ntransition_cnt; i++)
		if (ntransitions[i].from == from
				&& !strcmp(ntransitions[i].a, a)
				&& ntransitions[i].to == to)
			return NULL;
	i = ntransition_cnt++;
	ntransitions[i].from = from;
	strcpy(ntransitions[i].a, a);
	ntransitions[i].to = to;
	return &ntransitions[i];
}

static void follow(struct nstate *st)
{
	char a[SYMSIZE];
	struct nstate *dst;

	assert(st != NULL);

	if (!get_symbol(a, st->rl, st->pos))
		return;

	if (IS_NONTERMINAL(a)) {
		struct rule *rl;
		int i;

		for (i = 0; (rl = next_rule(a, &i)) != NULL; i++) {
			dst = new_nstate(rl, 0, false);
			if (new_ntransition(st, EPSILON, dst))
				follow(dst);
		}
	}

	dst = new_nstate(st->rl, st->pos+1, false);
	if (new_ntransition(st, a, dst))
		follow(dst);
}

static bool contains_nstate(struct nstate **buf, int len, struct nstate *st)
{
	int i;

	for (i = 0; i < len; i++)
		if (buf[i] == st)
			return true;
	return false;
}

static bool contains_ntransition(struct ntransition *buf, int len,
		struct ntransition *trans)
{
	int i;

	for (i = 0; i < len; i++)
		if (buf[i].from == trans->from
				&& !strcmp(buf[i].a, trans->a)
				&& buf[i].to == trans->to)
			return true;
	return false;
}

static void directly_reached_nstates(struct nstate *st, const char *a, 
		struct nstate **buf, int *offset_ptr)
{
	int i;

	if (!strcmp(a, EPSILON) && !contains_nstate(buf, *offset_ptr, st)) {
		assert(*offset_ptr < BUFSIZE);
		buf[(*offset_ptr)++] = st;
	}

	for (i = 0; i < ntransition_cnt; i++) {
		struct ntransition *trans;

		trans = &ntransitions[i];
		if (trans->from == st && !strcmp(trans->a, a)) {
			if (!contains_nstate(buf, *offset_ptr, trans->to)) {
				assert(*offset_ptr < BUFSIZE);
				buf[(*offset_ptr)++] = trans->to;
				if (!strcmp(a, EPSILON)) {
					directly_reached_nstates(trans->to, a,
							buf, offset_ptr);
				}
			}
		}
	}
}

static void reached_nstates(struct nstate *st, const char *a,
		struct nstate **buf, int *cnt_ptr) {
	struct nstate *est[BUFSIZE]; /* via epsilon reached states */
	struct nstate *rst[BUFSIZE]; /* total reached states */
	int ecnt, rcnt, i;

	ecnt = 0;
	rcnt = 0;

	/* all direct epsilon successors of st */
	directly_reached_nstates(st, EPSILON, est, &ecnt);
	
	/* all a successors of the st and its epsilon successors */
	for (i = 0; i < ecnt; i++)
		directly_reached_nstates(est[i], a, rst, &rcnt);

	/* add their epsilon successors */
	for (i = 0; i < rcnt; i++)
		directly_reached_nstates(rst[i], EPSILON, buf, cnt_ptr);
}

static void eliminate_epsilon_ntransitions(void)
{
	struct ntransition new_ntransitions[BUFSIZE];
	int i, cur_trans;

	determine_alphabet();

	for (i = 0, cur_trans = 0; i < nstate_cnt; i++) {
		struct nstate *st;
		int j;

		st = &nstates[i];

		if (st->start) {
			struct nstate *rst[BUFSIZE]; /* epsilon successors */
			int rcnt;

			rcnt = 0;
			reached_nstates(st, EPSILON, rst, &rcnt);
			for (j = 0; j < rcnt; j++)
				rst[j]->start = true;
		}

		for (j = 0; j < alphabet_cnt; j++) {
			struct nstate *rst[BUFSIZE]; /* reached states */
			int k, rcnt;
			const char *a;

			a = alphabet[j];
			rcnt = 0;
			reached_nstates(st, a, rst, &rcnt);
			for (k = 0; k < rcnt; k++) {
				struct ntransition trans;

				trans.from = st;
				strcpy(trans.a, a);
				trans.to = rst[k];
				if (!contains_ntransition(new_ntransitions,
							cur_trans, &trans)) {
					assert(cur_trans < BUFSIZE);
					new_ntransitions[cur_trans++] = trans;
				}
			}
		}
	}

	memcpy(ntransitions, new_ntransitions,
			BUFSIZE * sizeof(struct ntransition));
	ntransition_cnt = cur_trans;
}

static void start_nfa(void)
{
	const char *s = START;
	int i;
	struct rule *rl;

	for (i = 0; (rl = next_rule(s, &i)) != NULL; i++) {
		struct nstate *st;

		if ((st = new_nstate(rl, 0, true)) != NULL)
			follow(st);
	}
}

/* NFA to DFA via powerset construction */

static bool is_psc_start_state(struct dstate *st)
{
	int i;

	for (i = 0; i < st->cnt; i++)
		if (!st->nstates[i]->start)
			return false;
	return true;
}

static struct dstate *find_dstate(struct nstate **states, int cnt)
{
	int i;

	for (i = 0; i < dstate_cnt; i++) {
		if (dstates[i].cnt == cnt) { /* might be equal */
			bool equal;
			int j;

			equal = true;
			for (j = 0; j < dstates[i].cnt; j++)
				equal &= contains_nstate(states, cnt,
						dstates[i].nstates[j]);
			if (equal)
				return &dstates[i];
		}
	}
	return NULL;
}

static struct dstate *save_dstate(struct nstate **states, int cnt)
{
	struct dstate *st;
	int i;

	assert(states != NULL);
	assert(cnt > 0);

	if ((st = find_dstate(states, cnt)) != NULL)
		return st;

	dstates[dstate_cnt].nstates = malloc(cnt * sizeof(struct nstate *));
	for (i = 0; i < cnt; i++)
		dstates[dstate_cnt].nstates[i] = states[i];
	dstates[dstate_cnt].cnt = cnt;
	return &dstates[dstate_cnt++];
}

static void save_dtransition(struct dstate *from, const char *a,
		struct dstate *to)
{
	dtransitions[dtransition_cnt].from = from;
	strcpy(dtransitions[dtransition_cnt].a, a);
	dtransitions[dtransition_cnt].to = to;
	dtransition_cnt++;
}

static void psc_follow(struct dstate *st)
{
	int i;

	determine_alphabet();
	for (i = 0; i < alphabet_cnt; i++) {
		struct nstate *buf[BUFSIZE];
		struct dstate *to;
		int j, cnt;
		bool follow;
		const char *a;

		a = alphabet[i];
		cnt = 0;
		for (j = 0; j < st->cnt; j++)
			reached_nstates(st->nstates[j], a, buf, &cnt);

		if (cnt > 0) {
			follow = (find_dstate(buf, cnt) == NULL);
			to = save_dstate(buf, cnt);
			save_dtransition(st, a, to);
			if (follow)
				psc_follow(to);
		}
	}
}

static void powerset_construction(void)
{
	struct nstate *buf[BUFSIZE];
	struct dstate *st;
	int i, bufi;

	bufi = 0;
	for (i = 0; i < nstate_cnt; i++)
		if (nstates[i].start)
			buf[bufi++] = &nstates[i];
	st = save_dstate(buf, bufi);
	psc_follow(st);
}

/* goto- and action-table */

static void goto_table(FILE *out)
{
	int entry[dstate_cnt][BUFSIZE];
	int i, alph_cnt;

	determine_alphabet();
	for (i = 0; i < dstate_cnt; i++) {
		int j;

		for (j = 0; j < alphabet_cnt; j++) {
			const char *a;
			int k, st_ix;
			struct dstate *st;

			a = alphabet[j];
			st_ix = -1;
			st = NULL;
			for (k = 0; k < dtransition_cnt; k++) {
				struct dtransition *trans;

				trans = &dtransitions[k];
				if (trans->from == &dstates[i]
						&& !strcmp(trans->a, a)) {
					assert(st == NULL);
					st = trans->to;
				}
			}
			for (k = 0; k < dstate_cnt; k++) {
				if (&dstates[k] == st) {
					st_ix = k;
					break;
				}
			}
			entry[i][j] = st_ix;
		}
	}

	/* print C code */
	fprintf(out, "#define ALPHABET_SIZE\t"\
			"((ssize_t)((sizeof alphabet) / sizeof(alphabet[0])))");
	fprintf(out, "\n\n");
	fprintf(out, "const char *alphabet[%d] = {\n", alphabet_cnt);
	for (i = 0; i+1 < alphabet_cnt; i++)
		fprintf(out, "\t\"%s\",\n", alphabet[i]);
	fprintf(out, "\t\"%s\"\n", alphabet[i]);
	fprintf(out, "};\n\n");
	alph_cnt = i;
	fprintf(out, "const short goto_table[%u][%u] = {\n",
			dstate_cnt, alph_cnt);
	for (i = 0; i < dstate_cnt; i++) {
		int j;

		fprintf(out, "\t{ ");
		for (j = 0; j+1 < alph_cnt; j++)
			fprintf(out, "%d, ", entry[i][j]);
		fprintf(out, "%d ", entry[i][j]);
		fprintf(out, "},\n");
	}
	fprintf(out, "};\n\n");
}

static bool item_is_complete(struct dstate *st)
{
	bool complete;
	int i;

	assert(st != NULL);
	assert(st->cnt > 0);

	complete = false;
	for (i = 0; i < st->cnt; i++) {
		int len;

		len = rulelen(st->nstates[i]->rl);
		complete |= (st->nstates[i]->pos == len);
	}
	if (complete && st->cnt != 1) {
		fprintf(stderr, "Violating state: %s\n", dstatestr(st));
		for (i = 0; i < st->cnt; i++)
			fprintf(stderr, "\t%s (%d, %d)\n",
					nstatestr(st->nstates[i]),
					st->nstates[i]->pos,
					rulelen(st->nstates[i]->rl));
	}
	assert(!complete || st->cnt == 1);
	return complete;
}

static void action_table(FILE *out)
{
	struct action actions[dstate_cnt];
	int i;

	for (i = 0; i < dstate_cnt; i++) {
		struct action a;

		if (!item_is_complete(&dstates[i])) {
			a.action = SHIFT;
		} else if (!strcmp(dstates[i].nstates[0]->rl->v, START)) {
			a.action = ACCEPT;
		       	a.rl = dstates[i].nstates[0]->rl;
		} else {
			a.action = REDUCE;
		       	a.rl = dstates[i].nstates[0]->rl;
		}
		actions[i] = a;
	}

	/* print C code */
	fprintf(out, "static const struct rule {\n");
	fprintf(out, "\tconst char * const v;\n");
	fprintf(out, "\tconst char * const x;\n");
	fprintf(out, "\tcontainer_t (*func)(context_t *ctx, int argc, "\
			"container_t argv[]);\n");
	fprintf(out, "} rules[%u] = {\n", RCNT);
	for (i = 0; i < RCNT; i++) {
		fprintf(out, "\t{ \"%s\", \"%s\", %s }", rules[i].v, rules[i].x,
				(rules[i].funcname != NULL)
				? rules[i].funcname : "NULL");
		if (i+1 < RCNT)
			fprintf(out, ",");
		fprintf(out, "\n");
	}
	fprintf(out, "};\n\n");
	fprintf(out, "static int rulelen(const struct rule *rl)\n");
	fprintf(out, "{\n");
	fprintf(out, "\tconst char *s;\n");
	fprintf(out, "\tint i;\n\n");
	fprintf(out, "\ti = 0;\n");
	fprintf(out, "\tfor (s = rl->x; *s; s++)\n");
	fprintf(out, "\t\tif (*s==' '||*s=='\\t'||*s=='\\r'||*s=='\\n')\n");
	fprintf(out, "\t\t\ti++;\n");
	fprintf(out, "\treturn i+1;\n");
	fprintf(out, "}\n\n");
	fprintf(out, "static const struct action {\n");
	fprintf(out, "\tenum { ERROR = -1, SHIFT, REDUCE, ACCEPT } action;\n");
	fprintf(out, "\tint ruleix;\n");
	fprintf(out, "} action_table[%d] = {\n", dstate_cnt);
	for (i = 0; i < dstate_cnt; i++) {
		struct action *a;

		a = &actions[i];
		fprintf(out, "\t{ ");
		if (a->action == SHIFT) {
			fprintf(out, "SHIFT, -1");
		} else if (a->action == ACCEPT) {
			int rule_ix;

			for (rule_ix = 0; rule_ix < RCNT; rule_ix++)
				if (a->rl == &rules[rule_ix])
					break;
			fprintf(out, "ACCEPT, %d", rule_ix);
		} else if (a->action == REDUCE) {
			int rule_ix;

			for (rule_ix = 0; rule_ix < RCNT; rule_ix++)
				if (a->rl == &rules[rule_ix])
					break;
			fprintf(out, "REDUCE, %d", rule_ix);
		}
		fprintf(out, " }");
		if (i+1 < dstate_cnt)
			fprintf(out, ",");
		fprintf(out, "\n");
	}
	fprintf(out, "};\n\n");
}

/* drawing functions and main() */

static void draw_nfa_eps(void)
{
	FILE *fp;
	int i;

	fp = fopen("nfa_eps.dot", "w");
	fprintf(fp, "digraph {\n");
	fprintf(fp, "0 [label=\"Start\"]\n");
	for (i = 0; i < nstate_cnt; i++) {
		if (nstates[i].start)
			fprintf(fp, "0 -> %d\n", (int)&nstates[i]);
		fprintf(fp, "%d [label=\"%s\"]\n", (int)&nstates[i],
				nstatestr(&nstates[i]));
	}
	for (i = 0; i < ntransition_cnt; i++) {
		fprintf(fp, "%d -> %d [label=\" %s\"]\n",
				(int)ntransitions[i].from, 
				(int)ntransitions[i].to,
				transstr(ntransitions[i].a));
	}
	fprintf(fp, "}\n");
	fclose(fp);
}

static void draw_nfa(void)
{
	FILE *fp;
	int i;

	fp = fopen("nfa.dot", "w");
	fprintf(fp, "digraph {\n");
	fprintf(fp, "0 [label=\"Start\"]\n");
	for (i = 0; i < nstate_cnt; i++) {
		if (nstates[i].start)
			fprintf(fp, "0 -> %d\n", (int)&nstates[i]);
		fprintf(fp, "%d [label=\"%s\"]\n", (int)&nstates[i],
				nstatestr(&nstates[i]));
	}
	for (i = 0; i < ntransition_cnt; i++) {
		fprintf(fp, "%d -> %d [label=\" %s\"]\n",
				(int)ntransitions[i].from, 
				(int)ntransitions[i].to,
				transstr(ntransitions[i].a));
	}
	fprintf(fp, "}\n");
	fclose(fp);
}

static void draw_dfa(void)
{
	FILE *fp;
	int i;

	fp = fopen("dfa.dot", "w");
	fprintf(fp, "digraph {\n");
	fprintf(fp, "0 [label=\"Start\"]\n");
	for (i = 0; i < dstate_cnt; i++) {
		if (is_psc_start_state(&dstates[i]))
			fprintf(fp, "0 -> %d\n", (int)&dstates[i]);
		fprintf(fp, "%d [label=\"%s\"]\n", (int)&dstates[i],
				dstatestr(&dstates[i]));
	}
	for (i = 0; i < dtransition_cnt; i++) {
		fprintf(fp, "%d -> %d [label=\" %s\"]\n",
				(int)dtransitions[i].from,
				(int)dtransitions[i].to,
				transstr(dtransitions[i].a));
	}
	fprintf(fp, "}\n");
	fclose(fp);
}

static void draw_dfa_small(void)
{
	FILE *fp;
	int i;

	fp = fopen("dfa_small.dot", "w");
	fprintf(fp, "digraph {\n");
	fprintf(fp, "0 [label=\"Start\"]\n");
	for (i = 0; i < dstate_cnt; i++) {
		if (is_psc_start_state(&dstates[i]))
			fprintf(fp, "0 -> %d\n", (int)&dstates[i]);
		fprintf(fp, "%d [label=\"%d\"]\n", (int)&dstates[i], i);
	}
	for (i = 0; i < dtransition_cnt; i++) {
		fprintf(fp, "%d -> %d [label=\" %s\"]\n",
				(int)dtransitions[i].from,
				(int)dtransitions[i].to,
				transstr(dtransitions[i].a));
	}
	fprintf(fp, "}\n");
	fclose(fp);
}

static char **read_file(void)
{
	FILE *fp;
	char **lines;
	size_t size, bufsize;

	fp = fopen(SOURCE_FILE, "r");
	if (!fp)
		return NULL;

	bufsize = 100;
	lines = malloc(bufsize * sizeof(char *));
	for (size = 0; !feof(fp); size++) {
		size_t cnt, lncnt;
		int c;
		char *line;

		if (size+1 >= bufsize) {
			bufsize += 100;
			lines = realloc(lines, bufsize * sizeof(char *));
		}

		lncnt = 10;
		line = malloc(lncnt);
		for (cnt = 0; (c = getc(fp)) != EOF; cnt++) {
			if (cnt+2 >= lncnt) {
				lncnt += 10;
				line = realloc(line, lncnt);
			}
			line[cnt] = c;
			if (c == '\n')
				break;
		}
		line[cnt] = '\0';
		lines[size] = line;
	}
	if (size > 0 && strlen(lines[size-1]) == 0)
		size--;
	lines[size] = NULL;
	fclose(fp);
	return lines;
}

static bool create_backup(void)
{
	return system("cp \"" SOURCE_FILE "\" \"" SOURCE_FILE ".bak\"") == 0;
}

static bool write_file(char **lines)
{
	FILE *fp;

	assert(lines != NULL);

	fp = fopen(SOURCE_FILE, "w");
	if (!fp)
		return false;

	for (; *lines; lines++) {
		if (strcmp(*lines, GENERATED_CODE_BEGIN) == 0) {
			do {
				lines++;
			} while (strcmp(*lines, GENERATED_CODE_END));
			fprintf(fp, GENERATED_CODE_BEGIN "\n\n");
			action_table(fp);
			goto_table(fp);
			fprintf(fp, GENERATED_CODE_END "\n");
		} else {
			fprintf(fp, "%s\n", *lines);
		}
	}
	fclose(fp);
	return true;
}

int main(int argc, char *argv[])
{
	char **lines;

	printf("Calculating NFA ... ");
	start_nfa();
	printf("done\n");

	printf("Count of states in NEA = %d\n", nstate_cnt);
	printf("Count of transitions in NEA = %d\n", ntransition_cnt);

	printf("Drawing NFA with epsilon transitions ... ");
	draw_nfa_eps();
	printf("done\n");

	printf("Eliminating epsilon transitions ... ");
	eliminate_epsilon_ntransitions();
	printf("done\n");

	printf("Count of states in eps-free NEA = %d\n", nstate_cnt);
	printf("Count of transitions in eps-free NEA = %d\n", ntransition_cnt);

	printf("Powerset construction ... ");
	powerset_construction();
	printf("done\n");

	printf("Count of states in DEA = %d\n", dstate_cnt);
	printf("Count of transitions in DEA = %d\n", dtransition_cnt);

	printf("Drawing NFA ... ");
	draw_nfa();
	printf("done\n");

	printf("Drawing DFA ... ");
	draw_dfa();
	printf("done\n");

	printf("Drawing small DFA ... ");
	draw_dfa_small();
	printf("done\n");

	if ((lines = read_file()) == NULL) {
		fprintf(stderr, "Could not read destination file.\n");
		exit(1);
	}

	if (!create_backup()) {
		fprintf(stderr, "Could not backup destination file.\n");
		exit(1);
	}

	if (!write_file(lines)) {
		fprintf(stderr, "Could not write destination file.\n");
		exit(1);
	}

	return 0;
}

