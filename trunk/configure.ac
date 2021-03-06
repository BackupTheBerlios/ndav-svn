# Process this file with autoconf to produce a configure script.

PACKAGE=ndav
MAJOR_VERSION=0
MINOR_VERSION=2
MICRO_VERSION=1
#INTERFACE_AGE=6
BINARY_AGE=0

VERSION=$MAJOR_VERSION.$MINOR_VERSION.$MICRO_VERSION

AC_INIT([ndav], [0.2.1], [meand@users.berlios.de], ,
	[http://developer.berlios.de/projects/ndav/])

AC_CANONICAL_SYSTEM

AM_INIT_AUTOMAKE([1.9])

AC_CONFIG_SRCDIR(xml/ndav.css)
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL

# Checks for libraries.
dnl Check libxml2.
AC_PATH_PROG(XML_CONFIG, xml2-config, no)
if test "$XML_CONFIG" = "no"; then
  AC_MSG_ERROR([libxml2 not installed - please install first])
fi
CPPFLAGS="$CPPFLAGS `$XML_CONFIG --cflags`"

AC_CHECK_HEADERS(libxml/parser.h)
LIBS="$LIBS `$XML_CONFIG --libs`"

dnl Check for presence of libmhash.
AC_CHECK_HEADERS(mhash.h)
AC_CHECK_LIB(mhash, mhash_init, [LIBMHASH=-lmhash])
AC_SUBST(LIBMHASH)

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([errno.h fcntl.h stdlib.h string.h unistd.h libgen.h \
		  getopt.h time.h sys/cdefs.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST

# Checks for library functions.
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([getpass memset strchr strerror strncasecmp getopt basename time])

dnl Determine the situation for digests
digest_default="yes"

AC_ARG_ENABLE(digest, [  --enable-digest         support digest authentication (yes)],, enable_digest=$digest_default)

AC_SUBST(CFLAGS)

AC_MSG_CHECKING([if digest autentication is available])
if test "x$enable_digest" = xyes \
   && test "x$ac_cv_header_mhash_h" = xyes \
   && test "x$ac_cv_lib_mhash_mhash_init" = xyes; then
	AC_DEFINE(USE_DIGEST, 1, [Define to 1 for digest authentication.])
	AC_MSG_RESULT(yes)
   else
	LIBMHASH=
	AC_MSG_RESULT(no)
fi

AC_CONFIG_FILES([Makefile xml/Makefile \
	xml/handbok.mk \
	xml/handbok.xml:xml/handbok.in.xml])
AC_OUTPUT
