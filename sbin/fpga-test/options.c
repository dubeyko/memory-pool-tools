//SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * memory-pool-tools -- memory pool testing utilities.
 *
 * sbin/options.c - parsing command line options functionality.
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

#include <sys/types.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fpga_test.h"

/************************************************************************
 *                    Options parsing functionality                     *
 ************************************************************************/

void print_version(void)
{
	MEMPOOL_INFO("fpga-test, part of %s\n", MEMPOOL_TOOLS_VERSION);
}

void print_usage(void)
{
	FPGA_TEST_INFO(MEMPOOL_TRUE, "FPGA test tool\n\n");
	MEMPOOL_INFO("Usage: fpga-test  <options>\n");
	MEMPOOL_INFO("Options:\n");
	MEMPOOL_INFO("\t [-d|--debug]\t\t  show debug output.\n");
	MEMPOOL_INFO("\t [-h|--help]\t\t  display help message and exit.\n");
	MEMPOOL_INFO("\t [-i|--input-file]\t\t  write data from input file "
		     "into FPGA board.\n");
	MEMPOOL_INFO("\t [-o|--output-file]\t\t  extract result from FPGA "
		     "board into output file.\n");
	MEMPOOL_INFO("\t [-U|--uart-device]\t\t  define UART device name.\n");
	MEMPOOL_INFO("\t [-t|--fpga-core number=value, "
		     "portion-size=value]\t\t  define FPGA cores info.\n");
	MEMPOOL_INFO("\t [-I|--item granularity=value]\t\t  "
		     "define size of item in bytes.\n");
	MEMPOOL_INFO("\t [-r|--record capacity=value]\t\t  "
		     "define number of items in record.\n");
	MEMPOOL_INFO("\t [-p|--portion capacity=value,count=value]\t\t  "
		     "define number of records in portion.\n");
	MEMPOOL_INFO("\t [-k|--key mask=value]\t\t  define key.\n");
	MEMPOOL_INFO("\t [-v|--value mask=value]\t\t  define value.\n");
	MEMPOOL_INFO("\t [-c|--condition min=value,max=value]\t\t  "
		     "define condition.\n");
	MEMPOOL_INFO("\t [-a|--algorithm]\t\t  define algorithm "
		     "[KEY-VALUE|SORT|SELECT|TOTAL].\n");
	MEMPOOL_INFO("\t [-V|--version]\t\t  print version and exit.\n");
}

void parse_options(int argc, char *argv[],
		   struct mempool_test_environment *env)
{
	int c;
	int oi = 1;
	char *p;
	char sopts[] = "a:c:dhi:I:o:p:k:r:t:U:v:V";
	static const struct option lopts[] = {
		{"algorithm", 1, NULL, 'a'},
		{"condition", 1, NULL, 'c'},
		{"debug", 0, NULL, 'd'},
		{"help", 0, NULL, 'h'},
		{"input-file", 1, NULL, 'i'},
		{"output-file", 1, NULL, 'o'},
		{"uart-device", 1, NULL, 'U'},
		{"item", 1, NULL, 'I'},
		{"portion", 1, NULL, 'p'},
		{"key", 1, NULL, 'k'},
		{"record", 1, NULL, 'r'},
		{"fpga-core", 1, NULL, 't'},
		{"value", 1, NULL, 'v'},
		{"version", 0, NULL, 'V'},
		{ }
	};
	enum {
		ITEM_GRANULARITY_OPT = 0,
	};
	char *const item_tokens[] = {
		[ITEM_GRANULARITY_OPT]		= "granularity",
		NULL
	};
	enum {
		PORTION_CAPACITY_OPT = 0,
		PORTION_COUNT_OPT,
	};
	char *const portion_tokens[] = {
		[PORTION_CAPACITY_OPT]		= "capacity",
		[PORTION_COUNT_OPT]		= "count",
		NULL
	};
	enum {
		KEY_MASK_OPT = 0,
	};
	char *const key_tokens[] = {
		[KEY_MASK_OPT]			= "mask",
		NULL
	};
	enum {
		RECORD_CAPACITY_OPT = 0,
	};
	char *const record_tokens[] = {
		[RECORD_CAPACITY_OPT]		= "capacity",
		NULL
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
	enum {
		VALUE_MASK_OPT = 0,
	};
	char *const value_tokens[] = {
		[VALUE_MASK_OPT]		= "mask",
		NULL
	};
	enum {
		CONDITION_MIN_OPT = 0,
		CONDITION_MAX_OPT,
	};
	char *const condition_tokens[] = {
		[CONDITION_MIN_OPT]		= "min",
		[CONDITION_MAX_OPT]		= "max",
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
				MEMPOOL_ERR("input file is absent\n");
				print_usage();
				exit(EXIT_SUCCESS);
			}
			break;
		case 'o':
			env->output_file.name = optarg;
			if (!env->output_file.name) {
				MEMPOOL_ERR("output file is absent\n");
				print_usage();
				exit(EXIT_SUCCESS);
			}
			break;
		case 'U':
			env->uart_channel.name = optarg;
			if (!env->uart_channel.name) {
				MEMPOOL_ERR("UART device is not defined\n");
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
					MEMPOOL_ERR("invalid threads option\n");
					print_usage();
					exit(EXIT_FAILURE);
				};
			};
			break;
		case 'I':
			p = optarg;
			while (*p != '\0') {
				char *value;
				int granularity;

				switch (getsubopt(&p, item_tokens, &value)) {
				case ITEM_GRANULARITY_OPT:
					granularity = atoi(value);
					if (!check_granularity(granularity)) {
						MEMPOOL_ERR("invalid granularity\n");
						print_usage();
						exit(EXIT_FAILURE);
					}
					env->item.granularity = granularity;
					break;
				default:
					MEMPOOL_ERR("invalid item option\n");
					print_usage();
					exit(EXIT_FAILURE);
				};
			};
			break;
		case 'r':
			p = optarg;
			while (*p != '\0') {
				char *value;

				switch (getsubopt(&p, record_tokens, &value)) {
				case RECORD_CAPACITY_OPT:
					env->record.capacity = atoi(value);
					break;
				default:
					MEMPOOL_ERR("invalid record option\n");
					print_usage();
					exit(EXIT_FAILURE);
				};
			};
			break;
		case 'p':
			p = optarg;
			while (*p != '\0') {
				char *value;

				switch (getsubopt(&p, portion_tokens, &value)) {
				case PORTION_CAPACITY_OPT:
					env->portion.capacity = atoi(value);
					break;
				case PORTION_COUNT_OPT:
					env->portion.count = atoi(value);
					break;
				default:
					MEMPOOL_ERR("invalid portion option\n");
					print_usage();
					exit(EXIT_FAILURE);
				};
			};
			break;
		case 'k':
			p = optarg;
			while (*p != '\0') {
				char *value;

				switch (getsubopt(&p, key_tokens, &value)) {
				case KEY_MASK_OPT:
					env->key.mask = atoll(value);
					break;
				default:
					MEMPOOL_ERR("invalid key option\n");
					print_usage();
					exit(EXIT_FAILURE);
				};
			};
			break;
		case 'v':
			p = optarg;
			while (*p != '\0') {
				char *value;

				switch (getsubopt(&p, value_tokens, &value)) {
				case VALUE_MASK_OPT:
					env->value.mask = atoll(value);
					break;
				default:
					MEMPOOL_ERR("invalid value option\n");
					print_usage();
					exit(EXIT_FAILURE);
				};
			};
			break;
		case 'a':
			env->algorithm.id = convert_string2algorithm(optarg);
			if (env->algorithm.id < MEMPOOL_KEY_VALUE_ALGORITHM ||
			    env->algorithm.id > MEMPOOL_TOTAL_ALGORITHM) {
				MEMPOOL_ERR("invalid algorithm\n");
				print_usage();
				exit(EXIT_SUCCESS);
			}
			break;
		case 'c':
			p = optarg;
			while (*p != '\0') {
				char *value;

				switch (getsubopt(&p, condition_tokens, &value)) {
				case CONDITION_MIN_OPT:
					env->condition.min = atoll(value);
					break;
				case CONDITION_MAX_OPT:
					env->condition.max = atoll(value);
					break;
				default:
					MEMPOOL_ERR("invalid condition option\n");
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
