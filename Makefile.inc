CFLAGS		+= -std=c99 -pedantic -Wall -W -Wno-unused-parameter \
		-Wstrict-prototypes -O3 -mtune=i686 -pipe -Wstrict-aliasing=2
CFLAGS		+= -fPIC

#CFLAGS		+= -DCACHE_STATS		# enable small cache stats
#CFLAGS		+= -DMEMDEBUG			# enable memory tracking 
#CFLAGS		+= -O0 -g -DMALLOC_TRACE	# enable GNU malloc tracing
#CFLAGS		+= -DNO_CACHE			# disable caching in io/btree
#LDFALGS	+= -lmcheck



CPPFLAGS	+= -Wall

BISON		= bison
FLEX		= flex
DEPEND		= makedepend
CDEPEND		= $(DEPEND) -Y. -- $(CFLAGS) --
CPPDEPEND	= $(DEPEND) -Y. -- $(CFLAGS) --

