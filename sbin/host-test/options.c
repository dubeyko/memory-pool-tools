//SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * memory-pool-tools -- memory pool testing utilities.
 *
 * sbin/options.c - parsing command line options functionality.
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

#include <sys/types.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

#include "host_test.h"

/************************************************************************
 *                    Options parsing functionality                     *
 ************************************************************************/

void print_version(void)
{
	MEMPOOL_INFO("host-test, part of %s\n", MEMPOOL_TOOLS_VERSION);
}

void print_usage(void)
{
	HOST_TEST_INFO(MEMPOOL_TRUE, "host test tool\n\n");
	MEMPOOL_INFO("Usage: host-test  <options>\n");
	MEMPOOL_INFO("Options:\n");
	MEMPOOL_INFO("\t [-d|--debug]\t\t  show debug output.\n");
	MEMPOOL_INFO("\t [-h|--help]\t\t  display help message and exit.\n");
	MEMPOOL_INFO("\t [-i|--input-file]\t\t  define input file.\n");
	MEMPOOL_INFO("\t [-o|--output-file]\t\t  define output file.\n");
	MEMPOOL_INFO("\t [-t|--thread number=value, "
		     "portion-size=value]\t\t  define threads.\n");
	MEMPOOL_INFO("\t [-V|--version]\t\t  print version and exit.\n");
}

void parse_options(int argc, char *argv[],
		   struct mempool_host_test_environment *env)
{
	int c;
	int oi = 1;
	char *p;
	char sopts[] = "dhi:o:t:V";
	static const struct option lopts[] = {
		{"debug", 0, NULL, 'd'},
		{"help", 0, NULL, 'h'},
		{"input-file", 1, NULL, 'i'},
		{"output-file", 1, NULL, 'o'},
		{"thread", 1, NULL, 't'},
		{"version", 0, NULL, 'V'},
		{ }
	};
	enum {
		THREAD_COUNT_OPT = 0,
		THREAD_PORTION_SIZE_OPT,
	};
	char *const threads_tokens[] = {
		[THREAD_COUNT_OPT]		= "number",
		[THREAD_PORTION_SIZE_OPT]	= "portion-size",
		NULL
	};

	while ((c = getopt_long(argc, argv, sopts, lopts, &oi)) != EOF) {
		switch (c) {
		case 'd':
			env->show_debug = MEMPOOL_TRUE;
			break;
		case 'h':
			print_usage();
			exit(EXIT_SUCCESS);
		case 'i':
			env->input_file.name = optarg;
			if (!env->input_file.name) {
				print_usage();
				exit(EXIT_SUCCESS);
			}
			break;
		case 'o':
			env->output_file.name = optarg;
			if (!env->output_file.name) {
				print_usage();
				exit(EXIT_SUCCESS);
			}
			break;
		case 't':
			p = optarg;
			while (*p != '\0') {
				char *value;

				switch (getsubopt(&p, threads_tokens, &value)) {
				case THREAD_COUNT_OPT:
					env->threads.count = atoi(value);
					break;
				case THREAD_PORTION_SIZE_OPT:
					env->threads.portion_size = atoi(value);
					break;
				default:
					print_usage();
					exit(EXIT_FAILURE);
				};
			};
			break;
		case 'V':
			print_version();
			exit(EXIT_SUCCESS);
		default:
			print_usage();
			exit(EXIT_FAILURE);
		}
	}
}
