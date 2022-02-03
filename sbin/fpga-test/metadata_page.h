//SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * memory-pool-tools -- memory pool testing utilities.
 *
 * sbin/metadata_page.h - metadata page declarations.
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

#ifndef _METADATA_PAGE_H
#define _METADATA_PAGE_H

/*
 * struct mempool_metadata_record - record descriptor
 * @granularity: size of item in bytes
 * @capacity: number of items in one record
 */
struct mempool_metadata_record {
/* 0x0000 */
	unsigned int granularity;
	unsigned int capacity;

/* 0x0008 */
} __attribute__((packed));

/*
 * struct mempool_metadata_page - portion/page descriptor
 * @type: record type
 * @count: number of records in portion
 * @capacity: max possible number of records in portion
 */
struct mempool_metadata_page {
/* 0x0000 */
	struct mempool_metadata_record type;

/* 0x0008 */
	unsigned int count;
	unsigned int capacity;

/* 0x0010 */
} __attribute__((packed));

/*
 * struct mempool_metadata_key - key descriptor
 * @mask: bitmap defines items that are selected as keys
 */
struct mempool_metadata_key {
/* 0x0000 */
	unsigned long long mask;

/* 0x0008 */
} __attribute__((packed));

/*
 * struct mempool_metadata_value - value descriptor
 * @mask: bitmap defines items that are selected as values
 */
struct mempool_metadata_value {
/* 0x0000 */
	unsigned long long mask;

/* 0x0008 */
} __attribute__((packed));

/*
 * struct mempool_metadata_condition - condition of keys selection
 * @min: lower bound
 * @max: upper bound
 */
struct mempool_metadata_condition {
/* 0x0000 */
	unsigned long long min;
	unsigned long long max;

/* 0x0010 */
} __attribute__((packed));

/*
 * struct mempool_metadata_algorithm - algorithm descriptor
 * @code: algorithm ID
 * @start: starting index in the portion
 * @end: ending index in the portion
 */
struct mempool_metadata_algorithm {
/* 0x0000 */
	unsigned long long code;

/* 0x0008 */
	unsigned int start;
	unsigned int end;

/* 0x0010 */
} __attribute__((packed));

/*
 * struct mempool_metadata_request - request descriptor
 * @portion: page descriptor
 * @key: key descriptor
 * @value: value descriptor
 * @condition: condition descriptor
 * @algorithm: algorithm descriptor
 */
struct mempool_metadata_request {
/* 0x0000 */
	struct mempool_metadata_page portion;

/* 0x0010 */
	struct mempool_metadata_key key;
	struct mempool_metadata_value value;

/* 0x0020 */
	struct mempool_metadata_condition condition;

/* 0x0030 */
	struct mempool_metadata_algorithm algorithm;

/* 0x0040 */
} __attribute__((packed));

/*
 * struct mempool_metadata_result - result descriptor
 * @err: error code
 * @state: result state
 * @address: result address
 * @portion: page descriptor
 */
struct mempool_metadata_result {
/* 0x0000 */
	int err;
	int state;

/* 0x0008 */
	unsigned long long address;

/* 0x0010 */
	struct mempool_metadata_page portion;

/* 0x0020 */
} __attribute__((packed));

/*
 * struct mempool_metadata_management - management structure
 * @request: request descriptor
 * @result: result descriptor
 */
struct mempool_metadata_management {
/* 0x0000 */
	struct mempool_metadata_request request;

/* 0x0040 */
	struct mempool_metadata_result result;
	unsigned char padding[0x20];

/* 0x0080 */
} __attribute__((packed));

#endif /* _METADATA_PAGE_H */
