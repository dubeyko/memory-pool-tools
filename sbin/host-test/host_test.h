//SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * memory-pool-tools -- memory pool testing utilities.
 *
 * sbin/host_test.h - host testing tool declarations.
 *
 * Copyright (c) 2021-2022 Viacheslav Dubeyko <slava@dubeyko.com>
 *                         Igor Kauranen <aatx12@gmail.com>
 *                         Evgenii Bushtyrev <eugene@bushtyrev.com>
 * All rights reserved.
 *
 * Authors: Vyacheslav Dubeyko <slava@dubeyko.com>
 *          Igor Kauranen <aatx12@gmail.com>
 *          Evgenii Bushtyrev <eugene@bushtyrev.com>
 */

#ifndef _HOST_TEST_TOOL_H
#define _HOST_TEST_TOOL_H

#ifdef hosttest_fmt
#undef hosttest_fmt
#endif

#include "version.h"

#define hosttest_fmt(fmt) "host-test: " MEMPOOL_TOOLS_VERSION ": " fmt

#include "memory_pool_constants.h"
#include "memory_pool_tools.h"

#define HOST_TEST_INFO(show, fmt, ...) \
	do { \
		if (show) { \
			fprintf(stdout, hosttest_fmt(fmt), ##__VA_ARGS__); \
		} \
	} while (0)

/* options.c */
void print_version(void);
void print_usage(void);
void parse_options(int argc, char *argv[],
		   struct mempool_test_environment *env);

#endif /* _HOST_TEST_TOOL_H */
