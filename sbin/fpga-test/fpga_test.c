//SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * memory-pool-tools -- memory pool testing utilities.
 *
 * sbin/fpga_test.c - implementation of FPGA testing tool.
 *
 * Copyright (c) 2021 Viacheslav Dubeyko <slava@dubeyko.com>
 *                    Igor Kauranen <aatx12@gmail.com>
 *                    Evgenii Bushtyrev <eugene@bushtyrev.com>
 * All rights reserved.
 *
 * Authors: Vyacheslav Dubeyko <slava@dubeyko.com>
 *          Igor Kauranen <aatx12@gmail.com>
 *          Evgenii Bushtyrev <eugene@bushtyrev.com>
 */

#include <stdio.h>

#include "memory_pool_tools.h"

static void print_version(void)
{
	MEMPOOL_INFO("fpga-test, part of %s\n", MEMPOOL_TOOLS_VERSION);
}

int main()
{
	print_version();
	return 0;
}
