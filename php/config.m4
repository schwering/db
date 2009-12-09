PHP_ARG_ENABLE(dingsbums, wrapper for dingsbums db functions,
[ --enable-dingsbums  Enable dingsbums support])

if test "$PHP_DINGSBUMS" = "yes"; then
  AC_DEFINE(HAVE_DINGSBUMS, 1, [Whether you have Dingsbums])
  PHP_NEW_EXTENSION(dingsbums, dingsbums.c, $ext_shared)
  CFLAGS=-I..
  export CFLAGS
  LDFLAGS=-L.. -ldb
  export LDFLAGS
fi

