## Makefile.am
## SPDX-License-Identifier: BSD-3-Clause-Clear

AM_CFLAGS = -Wall
AM_CPPFLAGS = -I$(top_srcdir)/include

LDADD = $(top_builddir)/lib/libmemorypool.la

sbin_PROGRAMS = fpga-test

fpga_test_SOURCES = options.c uart_declarations.h \
			metadata_page.h \
			fpga_test.c fpga_test.h
