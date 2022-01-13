//SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * memory-pool-tools -- memory pool testing utilities.
 *
 * include/memory_pool_tools.c - memory pool tools' declarations.
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

#ifndef _MEMORY_POOL_TOOLS_H
#define _MEMORY_POOL_TOOLS_H

#ifdef pr_fmt
#undef pr_fmt
#endif

#include "version.h"

#define pr_fmt(fmt) MEMPOOL_TOOLS_VERSION ": " fmt

#include <sys/ioctl.h>

#include "memory_pool_constants.h"

#define MEMPOOL_ERR(fmt, ...) \
	fprintf(stderr, pr_fmt("%s:%d:%s(): " fmt), \
		__FILE__, __LINE__, __func__, ##__VA_ARGS__)

#define MEMPOOL_WARN(fmt, ...) \
	fprintf(stderr, pr_fmt("WARNING: " fmt), ##__VA_ARGS__)

#define MEMPOOL_INFO(fmt, ...) \
	fprintf(stdout, fmt, ##__VA_ARGS__)

#define MEMPOOL_FILE_INFO(stream, fmt, ...) \
	fprintf(stream, fmt, ##__VA_ARGS__)

#define MEMPOOL_DBG(show, fmt, ...) \
	do { \
		if (show) { \
			fprintf(stderr, pr_fmt("%s:%d:%s(): " fmt), \
				__FILE__, __LINE__, __func__, ##__VA_ARGS__); \
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

/*
 * struct mempool_test_environment - test's environment
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
struct mempool_test_environment {
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

static inline
int check_granularity(int granularity)
{
	switch (granularity) {
	case 1:
	case 2:
	case 4:
	case 8:
	case 16:
	case 32:
	case 64:
	case 128:
	case 256:
	case 512:
	case 1024:
		return MEMPOOL_TRUE;
	}

	return MEMPOOL_FALSE;
}

static inline
int convert_string2algorithm(const char *str)
{
	if (strcmp(str, MEMPOOL_KEY_VALUE_ALGORITHM_STR) == 0)
		return MEMPOOL_KEY_VALUE_ALGORITHM;
	else if (strcmp(str, MEMPOOL_SORT_ALGORITHM_STR) == 0)
		return MEMPOOL_SORT_ALGORITHM;
	else if (strcmp(str, MEMPOOL_SELECT_ALGORITHM_STR) == 0)
		return MEMPOOL_SELECT_ALGORITHM;
	else if (strcmp(str, MEMPOOL_TOTAL_ALGORITHM_STR) == 0)
		return MEMPOOL_TOTAL_ALGORITHM;
	else
		return MEMPOOL_UNKNOWN_ALGORITHM;
}

#endif /* _MEMORY_POOL_TOOLS_H */
