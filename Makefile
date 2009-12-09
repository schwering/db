include Makefile.inc
CFLAGS	+= -Idb
LDFLAGS += -Ldb -ldb 

all: terminal spc
terminal: db/libdb.a
spc: db/libdb.a
test: db/libdb.a

db/libdb.a: db/*.[chyl]
	$(MAKE) -C db libdb.a

sps: spc sp/*.sps
	./spc sp/min.sps
	./spc sp/max.sps
	./spc sp/avg.sps
	./spc sp/count.sps
	./spc sp/CreateDBs.sps

clean:
	$(MAKE) -C db clean
	rm -f *.o terminal test spc

loc: clean
	wc -lc */*.[chly] */*.sps xml/*.cpp xml/*.hpp | sort -n

