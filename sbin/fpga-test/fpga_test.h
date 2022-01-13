//SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * memory-pool-tools -- memory pool testing utilities.
 *
 * sbin/fpga_test.h - FPGA testing tool declarations.
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

#ifndef _FPGA_TEST_TOOL_H
#define _FPGA_TEST_TOOL_H

#ifdef fpgatest_fmt
#undef fpgatest_fmt
#endif

#include "version.h"

#define fpgatest_fmt(fmt) "fpga-test: " MEMPOOL_TOOLS_VERSION ": " fmt

#include "memory_pool_constants.h"
#include "memory_pool_tools.h"

#define FPGA_TEST_INFO(show, fmt, ...) \
	do { \
		if (show) { \
			fprintf(stdout, fpgatest_fmt(fmt), ##__VA_ARGS__); \
		} \
	} while (0)

/* options.c */
void print_version(void);
void print_usage(void);
void parse_options(int argc, char *argv[],
		   struct mempool_test_environment *env);

#endif /* _FPGA_TEST_TOOL_H */
