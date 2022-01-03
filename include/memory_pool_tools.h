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

#endif /* _MEMORY_POOL_TOOLS_H */
