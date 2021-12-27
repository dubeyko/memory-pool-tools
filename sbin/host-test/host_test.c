//SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * memory-pool-tools -- memory pool testing utilities.
 *
 * sbin/host_test.c - implementation of host testing tool.
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

#define _LARGEFILE64_SOURCE
#define __USE_FILE_OFFSET64
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <getopt.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <paths.h>
#include <limits.h>

#include "host_test.h"

int main(int argc, char *argv[])
{
	struct mempool_host_test_environment environment;
	int err = 0;

	environment.input_file.fd = -1;
	environment.input_file.stream = NULL;
	environment.input_file.name = NULL;
	environment.output_file.fd = -1;
	environment.output_file.stream = NULL;
	environment.output_file.name = NULL;
	environment.threads.count = 0;
	environment.threads.portion_size = 0;
	environment.show_debug = MEMPOOL_FALSE;

	parse_options(argc, argv, &environment);

	MEMPOOL_DBG(environment.show_debug,
		    "options have been parsed\n");

/* TODO: add logic */

	MEMPOOL_DBG(environment.show_debug,
		    "operation has been executed\n");

	exit(err ? EXIT_FAILURE : EXIT_SUCCESS);
}
