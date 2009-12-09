# Dingsbums 1: Relational Database Management System

Dingsbums 1 (German for thingy) is a small relational database management system written in C and a little C++ for Linux.

I wrote Dingsbums for fun (or educational purposes) in 2006 and 2007; it is not intended for real-world use.
It has a relatively wide range of features, but none of them should be expected to be implemented efficiently or stable:

* Relational operators selection, projection, join, union as well as sorting.
* Primary keys and foreign keys, implemented using a B+ tree index structure.
* Views in their most simple form.
* SQL-like language for data definition, manipulation and querying.
* Stored procedures.
* XML-to-relational mapping.
* PHP plugin.

(I also wrote another, equally useless DBMS in Ada, [dingsbums 6](https://github.com/schwering/db6), in case you're interested.)

Contact: schwering at gmail dot com


## Build

```shell
# Compile the sources first:
$ make
# Compile the stored proceudres:
$ make sps
# Run a sample file:
$ ./terminal @Queries
# Or could the interactive terminal:
$ ./terminal
```


## Structure

Directories:

* `data/`	place for database-files, index-files etc.
* `db/`		core database library written in C
* `help/`	contains some help files; used by the terminal
* `nfa/`	parser generator used for the stored procedures parser
* `php/`	source of the PHP extension
* `sp/`		standard stored procedures
* `xml/`	source of the xml-to-relational mapping written in C++


Single files:

* `terminal.c`	the sourcecode of the interactive terminal; compile with `make terminal`
* `test.c`	shows how to use the db.h interface of the database in C; compile with `make test`
* `spc.c`	the sourcecode stored procedure compiler; compile with `make spc`
* `Queries`	sample queries that show most features
* `Queries2`	sample queries that demonstrate foreign keys


## APIs

* `db/db.h`	C interface of dingsbums
* `xml/db.hpp`	C++ wrapper wraps the db.h C functions

## Known Bugs

Known bugs:

* FOREIGN KEY to non-existing relation yields Bus Error.
* Stored Procedures: parser is LR(0), should be LR(1) (hadn't read the lecture on compiler construction at the time).
* Stored Procedures: B_LIST to B_LIST.
* Stored Procedures: don't use db.h as interface.
* Stored Procedures: could have a better type system.
* Stored Procedures: relation information function (via io.h).

Wanted optimisations:

* Sorting should use indices. To this end, ixmngt.c needs a function or two to scan an interval (ix_min() and ix_max() from btree.c would help).
* Statistics system: better_xattr() could consider the size of the relation, but then the statistics of the relations need to be stored.
* Query optimiser.

Wanted features:

* Numbers: standard-, minimal-, maximal-values; auto increment.
* Compound indices.
* Something similar to embedded SQL.

