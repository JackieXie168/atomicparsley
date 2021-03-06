dnl Process this file with autoconf to produce a configure script.



dnl Some intro checks and defines
AC_INIT(AtomicParsley,AP_VER,pu@me.com)
AC_REVISION ($Id: configure.ac,v 1.14 2006/11/23 12:00:00 PU Exp $)
AC_PREREQ(2.50)
AC_CONFIG_SRCDIR(src/AtomicParsley.cpp)
AC_CONFIG_HEADER(src/config.h)

PACKAGE=atomicparsley
AP_MAJOR_VERSION=0
AP_MINOR_VERSION=9
AP_MICRO_VERSION=2

AP_VER=\"$AP_MAJOR_VERSION.$AP_MINOR_VERSION.$AP_MICRO_VERSION\"
AC_DEFINE_UNQUOTED(AP_VER,$AP_VER,[Define program version])

dnl---------------------------------------------------------
dnl Checks for programs.
AC_PROG_CXX
AC_PROG_MAKE_SET

dnl---------------------------------------------------------
dnl Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([stdio.h string.h stdlib.h time.h math.h signal.h \
                  wchar.h \
                  stddef.h \
                  sys/types.h sys/stat.h errno.h \
                  zlib.h])

AC_CHECK_HEADERS(getopt.h, [HAVE_GETOPT_H=1; AC_SUBST(HAVE_GETOPT_H)])

AC_CHECK_HEADERS(linux/cdrom.h)

dnl---------------------------------------------------------
dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST

dnl---------------------------------------------------------
dnl Checks for library functions.
AC_CHECK_FUNCS([fseeko fsetpos memset memcmp remove rename sranddev sscanf strdup strerror strftime strncmp strncasecmp strrchr strsep strstr  strtol wmemset])
AC_FUNC_MALLOC
AC_CHECK_FUNCS(lroundf)

dnl---------------------------------------------------------
dnl Checks for libraries.
AC_CHECK_LIB(z, deflateEnd, [HAVE_LIBZ=1; AC_SUBST(HAVE_LIBZ)])

dnl---------------------------------------------------------
dnl OS checks.
dnl from aclocal.m4
AC_OS_VERSION
os_name=$AP_OS_NAME
AC_SUBST(os_name)
AC_DEFINE_UNQUOTED(os_name, $AP_OS_NAME, [OS Platform name])
if test "$os_name" = "Darwin" ; then
HAVE_DARWIN_PLATFORM="true"
AC_SUBST(HAVE_DARWIN_PLATFORM)
fi

dnl---------------------------------------------------------
dnl Mac OS X Universal Build
AC_ARG_ENABLE(universal, dnl
[  --disable-universal     build a universal binary on Mac OS X [default=yes]],
              universal=$enableval, universal=no)
if test "$universal" = "yes" ; then
AC_CHECK_DARWIN_UNIVERSAL_SDK
AC_SUBST(universal)
fi

dnl---------------------------------------------------------
AC_ARG_ENABLE(debug, dnl
[  --disable-debug_build   do not build a debug version [default=yes]],
              debug=$enableval, debug=no)
if test "$debug" = "yes" ; then
AC_DEFINE_UNQUOTED(DEBUG, $debug, [build binary with debug output])
AC_SUBST(debug)
fi

dnl---------------------------------------------------------
dnl Done
echo "Creating files"
AC_OUTPUT([Makefile src/Makefile])
echo



dnl Pretty-print status message
echo "+----------------------------------------------+"
echo "|                  SUCCESS                     |"
echo "+----------------------------------------------+"
echo "  AtomicParsley has been configured, you should"
echo "  now type 'make' to compile AtomicParsley."
echo
echo "+----------------------------------------------+"
echo "|             YOUR CONFIGURATION               |"
echo "+----------------------------------------------+"

echo "  Version:      $AP_MAJOR_VERSION.$AP_MINOR_VERSION.$AP_MICRO_VERSION"

if test "$universal" = "no" ; then
	echo "  MacOSX universal build:       disabled"
else
	echo "  MacOSX universal build:       enabled"
fi	

if test "$debug" = "no" ; then
	echo "  debug build:       disabled"
else
	echo "  debug build:       enabled"
fi

echo
