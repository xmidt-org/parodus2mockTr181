dnl AC_PREREQ([2.65])
AC_INIT(webpageneric, 1.0, thomson_silvera@cable.comcast.com,webpageneric)
AM_INIT_AUTOMAKE([foreign no-dist-gzip dist-bzip2 1.9])
LT_INIT

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_PROG_CXX

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T
AC_HEADER_STDBOOL

AC_ENABLE_SHARED
AC_DISABLE_STATIC

SUBDIRS="include"


PKG_CHECK_MODULES([LIBNOPOLL], [nopoll >= 0.2.7])
PKG_CHECK_MODULES([LIBMSGPACK], [msgpack >= 1.1.0])
PKG_CHECK_MODULES([LIBCJSON], [cjson >= 1.0])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
