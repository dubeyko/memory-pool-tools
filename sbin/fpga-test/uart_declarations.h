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
	MEMPOOL_WRITE_4K_INPUT_DATA	= 0x3,
	MEMPOOL_READ_4K_RESULT		= 0x4,
};

#define MEMPOOL_INPUT_DATA_BASE_ADDRESS		(0x2000)

/*
 * struct mempool_uart_preamble - UART packet preamble
 * @magic: header magic
 * @address: memory page address
 * @operation_type: operation type
 */
struct mempool_uart_preamble {
	unsigned char magic;
	unsigned long long address;
	unsigned char operation_type;
}  __attribute__((packed));

#endif /* _UART_DECLARATIONS_H */