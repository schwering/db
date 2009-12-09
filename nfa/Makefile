CFLAGS += -Wall

nfa: nfa.c
nfa2: nfa2.c

dots: nfa
	./nfa

nfa.jpg: dots
	dot -Tjpg nfa.dot >nfa.jpg

dfa.jpg: dots
	dot -Tjpg dfa.dot >dfa.jpg

nfa_eps.jpg: dots
	dot -Tjpg nfa_eps.dot >nfa_eps.jpg

dfa_small.jpg: dots
	dot -Tjpg dfa_small.dot >dfa_small.jpg

clean:
	rm -f *.dot *.jpg tables.c

