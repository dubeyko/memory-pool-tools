//SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * memory-pool-tools -- memory pool testing utilities.
 *
 * sbin/fpga_test.c - implementation of FPGA testing tool.
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

#define _LARGEFILE64_SOURCE
#define __USE_FILE_OFFSET64
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fs.h>
#include <getopt.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <paths.h>
#include <limits.h>
#include <errno.h>
#include <pthread.h>

#include "fpga_test.h"

int main(int argc, char *argv[])
{
	struct mempool_test_environment environment;
	void *input_addr = NULL;
	off_t file_size;
	unsigned int portion_size;
	int err = 0;

	environment.input_file.fd = -1;
	environment.input_file.stream = NULL;
	environment.input_file.name = NULL;
	environment.item.granularity = 1;
	environment.record.capacity = 1;
	environment.portion.capacity = 0;
	environment.portion.count = 0;
	environment.key.mask = 0;
	environment.value.mask = 0;
	environment.condition.min = 0;
	environment.condition.max = ULLONG_MAX;
	environment.algorithm.id = MEMPOOL_UNKNOWN_ALGORITHM;
	environment.show_debug = MEMPOOL_FALSE;

	parse_options(argc, argv, &environment);

	MEMPOOL_DBG(environment.show_debug,
		    "options have been parsed\n");

	if (environment.portion.count > environment.portion.capacity) {
		err = -ERANGE;
		MEMPOOL_ERR("invalid portion descriptor: "
			    "count %d, capacity %d\n",
			    environment.portion.count,
			    environment.portion.capacity);
		goto finish_execution;
	}

	portion_size = (unsigned int)environment.item.granularity *
			environment.record.capacity;
	portion_size *= environment.portion.capacity;

/* TODO: Add logic */

	MEMPOOL_DBG(environment.show_debug,
		    "operation has been executed\n");


	if (input_addr) {
		err = munmap(input_addr, file_size);
		if (err) {
			MEMPOOL_ERR("fail to unmap input file: %s\n",
				    strerror(errno));
		}
	}

	if (environment.input_file.fd != -1)
		close(environment.input_file.fd);

finish_execution:
	exit(err ? EXIT_FAILURE : EXIT_SUCCESS);
}
