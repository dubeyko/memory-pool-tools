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

/*
 * struct mempool_file_descriptor - file descriptor
 * @fd: file descriptor
 * @stream: file stream
 * @name: file name
 */
struct mempool_file_descriptor {
	int fd;
	FILE *stream;
	const char *name;
};

/*
 * struct mempool_threads_descriptor - threads descriptor
 * @count: number of threads
 * @portion_size: data portion size in bytes for every thread
 */
struct mempool_threads_descriptor {
	int count;
	int portion_size;
};

/*
 * struct mempool_item_descriptor - item descriptor
 * @granularity: size of item in bytes
 */
struct mempool_item_descriptor {
	int granularity;
};

/*
 * struct mempool_record_descriptor - record descriptor
 * @capacity: number of items in record
 */
struct mempool_record_descriptor {
	int capacity;
};

/*
 * struct mempool_portion_descriptor - portion/page descriptor
 * @capacity: maximum number of records in one portion
 * @count: real number of records in one portion
 */
struct mempool_portion_descriptor {
	int capacity;
	int count;
};

/*
 * struct mempool_key_descriptor - key descriptor
 * @mask: bitmap defines items in record are selected as key
 */
struct mempool_key_descriptor {
	unsigned long long mask;
};

/*
 * struct mempool_value_descriptor - value descriptor
 * @mask: bitmap defines items in record are selected as value
 */
struct mempool_value_descriptor {
	unsigned long long mask;
};

/*
 * struct mempool_condition_descriptor - condition descriptor
 * @min: lower bound
 * @max: upper bound
 */
struct mempool_condition_descriptor {
	unsigned long long min;
	unsigned long long max;
};

/*
 * struct mempool_algorithm_descriptor - algorithm descriptor
 * @id: algorithm ID
 */
struct mempool_algorithm_descriptor {
	int id;
};

/* algorithm ID */
enum {
	MEMPOOL_UNKNOWN_ALGORITHM,
	MEMPOOL_KEY_VALUE_ALGORITHM,
	MEMPOOL_SORT_ALGORITHM,
	MEMPOOL_SELECT_ALGORITHM,
	MEMPOOL_TOTAL_ALGORITHM,
	MEMPOOOL_ALGORITHM_ID_MAX
};

#define MEMPOOL_KEY_VALUE_ALGORITHM_STR		"KEY-VALUE"
#define MEMPOOL_SORT_ALGORITHM_STR		"SORT"
#define MEMPOOL_SELECT_ALGORITHM_STR		"SELECT"
#define MEMPOOL_TOTAL_ALGORITHM_STR		"TOTAL"

/*
 * struct mempool_host_test_environment - host test's environment
 * @input_file: input file
 * @output_file: output file
 * @threads: threads descriptor
 * @item: item descriptor
 * @record: record descriptor
 * @portion: portion descriptor
 * @key: key descriptor
 * @value: value descriptor
 * @condition: condition descriptor
 * @show_debug: show debug messages
 */
struct mempool_host_test_environment {
	struct mempool_file_descriptor input_file;
	struct mempool_file_descriptor output_file;
	struct mempool_threads_descriptor threads;
	struct mempool_item_descriptor item;
	struct mempool_record_descriptor record;
	struct mempool_portion_descriptor portion;
	struct mempool_key_descriptor key;
	struct mempool_value_descriptor value;
	struct mempool_condition_descriptor condition;
	struct mempool_algorithm_descriptor algorithm;

	int show_debug;
};

/* options.c */
void print_version(void);
void print_usage(void);
void parse_options(int argc, char *argv[],
		   struct mempool_host_test_environment *env);

#endif /* _HOST_TEST_TOOL_H */
