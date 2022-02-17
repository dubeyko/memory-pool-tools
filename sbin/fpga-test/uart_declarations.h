//SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * memory-pool-tools -- memory pool testing utilities.
 *
 * sbin/uart_declarations.h - UART declarations.
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

#ifndef _UART_DECLARATIONS_H
#define _UART_DECLARATIONS_H

/* Header magics */
enum {
	MEMPOOL_PC2FPGA_MAGIC = 0x55,
	MEMPOOL_FPGA2PC_MAGIC = 0xAA,
};

/* Operation types */
enum {
	MEMPOOL_SEND_MANAGEMENT_PAGE	= 0x1,
	MEMPOOL_POLL_MANAGEMENT_PAGE	= 0x2,
	MEMPOOL_WRITE_INPUT_DATA	= 0x3,
	MEMPOOL_READ_RESULT		= 0x4,
};

#define MEMPOOL_INPUT_DATA_BASE_ADDRESS		(0x2000)
#define MEMPOOL_MANAGEMENT_PAGE_BASE_ADDRESS	(0x3000)

/*
 * struct mempool_uart_preamble - UART packet preamble
 * @magic: header magic
 * @operation_type: operation type
 * @length: payload length
 * @crc32: payload's checksum
 * @address: memory page address
 */
struct mempool_uart_preamble {
/* 0x0000 */
	unsigned char magic;
	unsigned char operation_type;
	unsigned short length;

/* 0x0004 */
	unsigned int crc32;

/* 0x0008 */
	unsigned long long address;

/* 0x0010 */
} __attribute__((packed));

/*
 * struct mempool_uart_footer - UART packet footer
 * @magic: footer magic
 * @operation_type: operation type
 * @crc32: payload's checksum
 */
struct mempool_uart_footer {
/* 0x0000 */
	unsigned char magic;
	unsigned char operation_type;
	unsigned short padding;

/* 0x0004 */
	unsigned int crc32;

/* 0x0008 */
} __attribute__((packed));

/*
 * struct mempool_uart_answer - FPGA UART answer preamble
 * @magic: answer magic
 * @result: operation result
 * @length: payload length
 * @crc32: payload's checksum
 */
struct mempool_uart_answer {
/* 0x0000 */
	unsigned char magic;
	unsigned char result;
	unsigned short length;

/* 0x0004 */
	unsigned int crc32;

/* 0x0008 */
} __attribute__((packed));

#endif /* _UART_DECLARATIONS_H */
