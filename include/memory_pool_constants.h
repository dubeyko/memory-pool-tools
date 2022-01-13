//SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * memory-pool-tools -- memory pool testing utilities.
 *
 * include/memory_pool_constants.c - memory pool tools constants.
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

#ifndef _MEMPOOL_CONSTANTS_H
#define _MEMPOOL_CONSTANTS_H

#define MEMPOOL_TRUE (1)
#define MEMPOOL_FALSE (0)

#define MEMPOOL_BITS_PER_BYTE	(8)

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

#endif /* _MEMPOOL_CONSTANTS_H */
