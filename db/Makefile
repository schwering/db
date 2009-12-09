SRCS	= attr.c err.c ixmngt.c rlalg.c btree.c expr.c arraylist.c rlmngt.c \
	  cache.c hashset.c mem.c scanner.c verif.c ddl.c hashtable.c \
	  parser.c sort.c view.c dml.c io.c printer.c str.c \
	  fgnkey.c linkedlist.c sp.c db.c
HDRS	= attr.h err.h ixmngt.h rlalg.h btree.h expr.h arraylist.h rlmngt.h \
	  cache.h hashset.h mem.h verif.h ddl.h hashtable.h \
	  parser.h sort.h view.h dml.h io.h printer.h str.h  \
	  fgnkey.h constants.h linkedlist.h sp.h db.h
OBJS	= attr.o err.o ixmngt.o rlalg.o btree.o expr.o arraylist.o rlmngt.o \
	  cache.o hashset.o mem.o scanner.o verif.o ddl.o hashtable.o \
	  parser.o sort.o view.o dml.o io.o printer.o str.o \
	  fgnkey.o linkedlist.o sp.o db.o

include ../Makefile.inc

all: libdb.a

depend: $(HDRS) $(SRCS)
	$(CDEPEND) $(HDRS) $(SRCS)

libdb.a: $(OBJS)
	ar cru $@ $^
	ranlib $@

libdb.so: $(OBJS)
	$(CC) -shared -Wl,-soname,$@ -o $@ $^

parser.h: parser.c
parser.c: parser.y
	$(BISON) -p ql $*.y --defines=$*.h -o $*.c

scanner.c: scanner.l
	$(FLEX) -Pql -o$*.c $*.l

clean:
	rm -rf *.o scanner.c parser.c parser.h libdb.a

stats: clean
	wc -lc *.[chly] | sort -n

# DO NOT DELETE

attr.o: dml.h block.h constants.h parser.h expr.h io.h hashtable.h
ixmngt.o: btree.h block.h cache.h constants.h parser.h io.h hashtable.h
rlalg.o: btree.h block.h cache.h constants.h parser.h io.h hashtable.h
btree.o: block.h cache.h constants.h parser.h
expr.o: dml.h block.h constants.h parser.h expr.h
arraylist.o: mem.h
rlmngt.o: io.h block.h constants.h parser.h hashtable.h
cache.o: block.h
verif.o: ddl.h dml.h block.h constants.h parser.h expr.h
ddl.o: dml.h block.h constants.h parser.h expr.h
sort.o: rlalg.h btree.h block.h cache.h constants.h parser.h io.h hashtable.h
view.o: dml.h block.h constants.h parser.h expr.h
dml.o: block.h constants.h parser.h expr.h dml.h
io.o: block.h constants.h parser.h hashtable.h
printer.o: block.h rlalg.h btree.h cache.h constants.h parser.h io.h
printer.o: hashtable.h
str.o: mem.h
fgnkey.o: io.h block.h constants.h parser.h hashtable.h
constants.o: parser.h
linkedlist.o: mem.h
sp.o: dml.h block.h constants.h parser.h expr.h
attr.o: attr.h dml.h block.h constants.h parser.h expr.h io.h hashtable.h
attr.o: rlmngt.h str.h mem.h
err.o: err.h
ixmngt.o: ixmngt.h btree.h block.h cache.h constants.h parser.h io.h
ixmngt.o: hashtable.h attr.h dml.h expr.h err.h mem.h rlmngt.h str.h
rlalg.o: rlalg.h btree.h block.h cache.h constants.h parser.h io.h
rlalg.o: hashtable.h err.h ixmngt.h mem.h sort.h
btree.o: btree.h block.h cache.h constants.h parser.h mem.h str.h
expr.o: expr.h dml.h block.h constants.h parser.h attr.h io.h hashtable.h
expr.o: err.h linkedlist.h mem.h rlmngt.h str.h
arraylist.o: arraylist.h mem.h
rlmngt.o: rlmngt.h io.h block.h constants.h parser.h hashtable.h err.h
rlmngt.o: fgnkey.h ixmngt.h btree.h cache.h mem.h str.h
cache.o: cache.h block.h mem.h
hashset.o: hashset.h mem.h
mem.o: mem.h hashset.h str.h
scanner.o: constants.h parser.h mem.h
verif.o: verif.h ddl.h dml.h block.h constants.h parser.h expr.h attr.h io.h
verif.o: hashtable.h err.h hashset.h ixmngt.h btree.h cache.h rlmngt.h str.h
verif.o: mem.h sort.h rlalg.h view.h
ddl.o: ddl.h dml.h block.h constants.h parser.h expr.h err.h fgnkey.h io.h
ddl.o: hashtable.h ixmngt.h btree.h cache.h mem.h rlmngt.h str.h verif.h
ddl.o: view.h
hashtable.o: hashtable.h
parser.o: arraylist.h mem.h db.h ddl.h dml.h block.h constants.h parser.h
parser.o: expr.h err.h sort.h rlalg.h btree.h cache.h io.h hashtable.h
sort.o: sort.h rlalg.h btree.h block.h cache.h constants.h parser.h io.h
sort.o: hashtable.h attr.h dml.h expr.h err.h mem.h
view.o: view.h dml.h block.h constants.h parser.h expr.h mem.h str.h
view.o: hashtable.h
dml.o: dml.h block.h constants.h parser.h expr.h attr.h io.h hashtable.h db.h
dml.o: err.h ixmngt.h btree.h cache.h mem.h printer.h rlalg.h rlmngt.h sp.h
dml.o: verif.h ddl.h view.h
io.o: io.h block.h constants.h parser.h hashtable.h cache.h err.h mem.h
printer.o: printer.h block.h rlalg.h btree.h cache.h constants.h parser.h
printer.o: io.h hashtable.h err.h
str.o: str.h mem.h
fgnkey.o: fgnkey.h io.h block.h constants.h parser.h hashtable.h btree.h
fgnkey.o: cache.h ixmngt.h rlmngt.h str.h mem.h
linkedlist.o: linkedlist.h mem.h
sp.o: sp.h dml.h block.h constants.h parser.h expr.h db.h err.h linkedlist.h
sp.o: mem.h str.h
db.o: db.h block.h constants.h parser.h ddl.h dml.h expr.h mem.h printer.h
db.o: rlalg.h btree.h cache.h io.h hashtable.h rlmngt.h
