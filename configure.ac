#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
# SPDX-License-Identifier: BSD-3-Clause-Clear

AC_PREREQ([2.69])
AC_INIT(memory-pool-tools, 0.06, slava@dubeyko.com)
AM_INIT_AUTOMAKE
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

# Checks for programs.
AC_PROG_CXX
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_LIBTOOL
AC_PROG_RANLIB

# Checks for libraries.
AC_CHECK_LIB([pthread], [pthread_create],
		[AC_DEFINE([HAVE_LIBPTHREAD], 1,
			   [Define to 1 if you have the 'pthread' library (-lthread).])],
		[AC_MSG_ERROR([PTHREAD library not found])])
AC_CHECK_HEADERS([pthread.h])

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h limits.h paths.h stdlib.h string.h sys/ioctl.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_CHECK_MEMBERS([struct stat.st_rdev])

# Checks for library functions.
AC_HEADER_MAJOR
AC_FUNC_MALLOC
AC_CHECK_FUNCS([clock_gettime memmove memset strerror])

AC_CONFIG_FILES([Makefile
                 include/Makefile
                 lib/Makefile
                 sbin/Makefile
                 sbin/fpga-test/Makefile
                 sbin/host-test/Makefile])
AC_OUTPUT
