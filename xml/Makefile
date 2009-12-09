CPPFLAGS	+= -Ixercesc/include
LDFLAGS		+= -Lxercesc/lib -lxerces-c

CPPFLAGS	+= -I../db
LDFLAGS		+= -L../db -ldb

OBJS		+= test.o db.o NaiveMapping.o DTDGraph.o XMLStringUtils.o\
			XMLElement.o XPath.o

include ../Makefile.inc

test: ../db/libdb.a $(OBJS)
	$(CXX) $(LDFLAGS) $(OBJS) -o test

../db/libdb.a: ../db/*.[chyl]
	$(MAKE) -C ../db libdb.a

depend: *.cpp *.hpp
	$(CPPDEPEND) *.cpp *.hpp


clean:
	rm -f *.o test

# DO NOT DELETE

DTDGraph.o: DTDGraph.hpp db.hpp Exception.hpp XMLStringUtils.hpp
NaiveMapping.o: NaiveMapping.hpp DTDGraph.hpp db.hpp Exception.hpp
NaiveMapping.o: Mapping.hpp XMLElement.hpp XPath.hpp
XMLElement.o: XMLElement.hpp
XMLStringUtils.o: XMLStringUtils.hpp
XPath.o: XPath.hpp Exception.hpp
db.o: db.hpp Exception.hpp
test.o: db.hpp Exception.hpp DTDGraph.hpp NaiveMapping.hpp Mapping.hpp
test.o: XMLElement.hpp XPath.hpp Handler.hpp XMLStringUtils.hpp
DTDGraph.o: db.hpp Exception.hpp
Handler.o: Mapping.hpp db.hpp Exception.hpp DTDGraph.hpp XMLElement.hpp
Handler.o: XPath.hpp NaiveMapping.hpp XMLStringUtils.hpp
Mapping.o: db.hpp Exception.hpp DTDGraph.hpp XMLElement.hpp XPath.hpp
NaiveMapping.o: DTDGraph.hpp db.hpp Exception.hpp Mapping.hpp XMLElement.hpp
NaiveMapping.o: XPath.hpp
XPath.o: Exception.hpp
db.o: Exception.hpp
