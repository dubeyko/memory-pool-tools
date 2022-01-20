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
#include <termios.h>

#include "uart_declarations.h"
#include "fpga_test.h"

static
int mempool_open_channel_to_fpga(struct mempool_test_environment *env)
{
	MEMPOOL_DBG(env->show_debug,
		    "env %p\n",
		    env);

	env->uart_channel.fd = open(env->uart_channel.name,
				    O_RDWR | O_NOCTTY | O_NDELAY);
	if (env->uart_channel.fd == -1) {
		MEMPOOL_ERR("fail to open UART channel: %s\n",
			    strerror(errno));
		return -EFAULT;
	}

	MEMPOOL_DBG(env->show_debug,
		    "UART channel has been opened\n");

	return 0;
}

static int
mempool_configure_communication_parameters(struct mempool_test_environment *env)
{
	struct termios config;

	MEMPOOL_DBG(env->show_debug,
		    "env %p\n",
		    env);

	if (tcgetattr(env->uart_channel.fd, &config) < 0) {
		MEMPOOL_ERR("fail to get current configuration: %s\n",
			    strerror(errno));
		return -EFAULT;
	}

	//
	// Input flags - Turn off input processing
	//
	// convert break to null byte, no CR to NL translation,
	// no NL to CR translation, don't mark parity errors or breaks
	// no input parity check, don't strip high bit off,
	// no XON/XOFF software flow control
	//
	config.c_iflag &= ~(IGNBRK | BRKINT | ICRNL |
				INLCR | PARMRK | INPCK | ISTRIP | IXON);

	//
	// Output flags - Turn off output processing
	//
	// no CR to NL translation, no NL to CR-NL translation,
	// no NL to CR translation, no column 0 CR suppression,
	// no Ctrl-D suppression, no fill characters, no case mapping,
	// no local output processing
	//
	// config.c_oflag &= ~(OCRNL | ONLCR | ONLRET |
	//                     ONOCR | ONOEOT| OFILL | OLCUC | OPOST);
	config.c_oflag = 0;

	//
	// No line processing
	//
	// echo off, echo newline off, canonical mode off,
	// extended input processing off, signal chars off
	//
	config.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN | ISIG);

	//
	// Turn off character processing
	//
	// clear current char size mask, no parity checking,
	// no output processing, force 8 bit input
	//
	config.c_cflag &= ~(CSIZE | PARENB);
	config.c_cflag |= CS8;

	//
	// One input byte is enough to return from read()
	// Inter-character timer off
	//
	config.c_cc[VMIN]  = 1;
	config.c_cc[VTIME] = 0;

	//
	// Communication speed (simple version, using the predefined
	// constants)
	//
	if (cfsetispeed(&config, B115200) < 0 ||
	    cfsetospeed(&config, B115200) < 0) {
		MEMPOOL_ERR("fail to set speed of communication: %s\n",
			    strerror(errno));
		return -EFAULT;
	}

	if (tcsetattr(env->uart_channel.fd, TCSAFLUSH, &config) < 0) {
		MEMPOOL_ERR("fail to set configuration of communication: %s\n",
			    strerror(errno));
		return -EFAULT;
	}

	MEMPOOL_DBG(env->show_debug,
		    "UART channel has been configured\n");

	return 0;
}

static
int mempool_close_channel_to_fpga(struct mempool_test_environment *env)
{
	MEMPOOL_DBG(env->show_debug,
		    "env %p\n",
		    env);

	if (env->uart_channel.fd != -1) {
		close(env->uart_channel.fd);
	} else {
		MEMPOOL_ERR("UART channel's file descriptor invalid\n");
		return -ERANGE;
	}

	MEMPOOL_DBG(env->show_debug,
		    "UART channel has been closed\n");

	return 0;
}

static
int mempool_send_preamble(struct mempool_test_environment *env,
			  unsigned char magic,
			  unsigned long long base_address,
			  int page_index,
			  unsigned char operation_type)
{
	struct mempool_uart_preamble preamble = {0};
	ssize_t written_bytes;
	size_t bytes_count = sizeof(struct mempool_uart_preamble);
	int err;

	MEMPOOL_DBG(env->show_debug,
		    "env %p\n",
		    env);

	preamble.magic = magic;
	preamble.address = base_address + page_index;
	preamble.operation_type = operation_type;

	written_bytes = write(env->uart_channel.fd,
			      &preamble, bytes_count);
	if (written_bytes < 0) {
		MEMPOOL_ERR("fail to send preamble into FPGA: %s\n",
			    strerror(errno));
		return -EFAULT;
	} else if (written_bytes != bytes_count) {
		MEMPOOL_ERR("written_bytes %zd != bytes_count %zu\n",
			    written_bytes, bytes_count);
		return -EFAULT;
	}

	MEMPOOL_DBG(env->show_debug,
		    "preamble has been sent to FPGA\n");

	return err;
}

static
int __mempool_write_data_into_fpga(struct mempool_test_environment *env,
				   void *input_addr, off_t file_size)
{
	ssize_t written_bytes = 0;
	int err = -ENODATA;

	MEMPOOL_DBG(env->show_debug,
		    "input_addr %p, file_size %lu\n",
		    input_addr, file_size);

	while (written_bytes < file_size) {
		off_t bytes_count;
		ssize_t sent_bytes;
		off_t page_index = written_bytes / MEMPOOL_PAGE_SIZE;

		bytes_count = file_size - written_bytes;
		if (bytes_count > MEMPOOL_PAGE_SIZE)
			bytes_count = MEMPOOL_PAGE_SIZE;

		err = mempool_send_preamble(env,
					    MEMPOOL_PC2FPGA_MAGIC,
					    MEMPOOL_INPUT_DATA_BASE_ADDRESS,
					    page_index,
					    MEMPOOL_WRITE_4K_INPUT_DATA);
		if (err) {
			MEMPOOL_ERR("fail to send preable into FPGA: err %d\n",
				    err);
			goto finish_write_data_into_fpga;
		}

		sent_bytes = write(env->uart_channel.fd,
				   (unsigned char *)input_addr + written_bytes,
				   bytes_count);
		if (sent_bytes < 0) {
			err = -EFAULT;
			MEMPOOL_ERR("fail to write into FPGA: %s\n",
				    strerror(errno));
			goto finish_write_data_into_fpga;
		} else if (sent_bytes != bytes_count) {
			err = -EFAULT;
			MEMPOOL_ERR("sent_bytes %zd != bytes_count %lu\n",
				    sent_bytes, bytes_count);
			goto finish_write_data_into_fpga;
		}

		err = tcdrain(env->uart_channel.fd);
		if (err) {
			err = -EFAULT;
			MEMPOOL_ERR("wait function failed: %s\n",
				    strerror(errno));
			goto finish_write_data_into_fpga;
		}

		written_bytes += bytes_count;
	};

	MEMPOOL_DBG(env->show_debug,
		    "data stream has been sent to FPGA\n");

finish_write_data_into_fpga:
	return err;
}


static
int mempool_write_data_into_fpga(struct mempool_test_environment *env,
				 void *input_addr, off_t file_size)
{
	int err = 0;

	MEMPOOL_DBG(env->show_debug,
		    "input_addr %p, file_size %lu\n",
		    input_addr, file_size);

	err = mempool_open_channel_to_fpga(env);
	if (err) {
		MEMPOOL_ERR("fail to open channel to FPGA: "
			    "err %d\n", err);
		return err;
	}

	err = mempool_configure_communication_parameters(env);
	if (err) {
		MEMPOOL_ERR("fail to configure communication parameters: "
			    "err %d\n", err);
		goto close_channel;
	}

	err = __mempool_write_data_into_fpga(env, input_addr, file_size);
	if (err) {
		MEMPOOL_ERR("fail to write data into FPGA: "
			    "file_size %lu, err %d\n",
			    file_size, err);
		goto close_channel;
	}

close_channel:
	mempool_close_channel_to_fpga(env);

	MEMPOOL_DBG(env->show_debug,
		    "write operation has been finished: "
		    "err %d\n", err);

	return err;
}

static
int mempool_read_result_from_fpga(struct mempool_test_environment *env,
				  void *output_addr, off_t file_size)
{
	MEMPOOL_ERR("unsupported algorithm\n");
	return -EOPNOTSUPP;
}

static
int mempool_fpga_key_value_algorithm(struct mempool_test_environment *env)
{
	MEMPOOL_ERR("unsupported algorithm %#x\n",
		    env->algorithm.id);
	return -EOPNOTSUPP;
}

static
int mempool_fpga_sort_algorithm(struct mempool_test_environment *env)
{
	MEMPOOL_ERR("unsupported algorithm %#x\n",
		    env->algorithm.id);
	return -EOPNOTSUPP;
}

static
int mempool_fpga_select_algorithm(struct mempool_test_environment *env)
{
	MEMPOOL_ERR("unsupported algorithm %#x\n",
		    env->algorithm.id);
	return -EOPNOTSUPP;
}

static
int mempool_fpga_total_algorithm(struct mempool_test_environment *env)
{
	MEMPOOL_ERR("unsupported algorithm %#x\n",
		    env->algorithm.id);
	return -EOPNOTSUPP;
}

static
int mempool_execute_algorithm_by_fpga(struct mempool_test_environment *env)
{
	int err = 0;

	MEMPOOL_DBG(env->show_debug,
		    "algorithm %#x\n",
		    env->algorithm.id);

	switch (env->algorithm.id) {
	case MEMPOOL_KEY_VALUE_ALGORITHM:
		err = mempool_fpga_key_value_algorithm(env);
		if (err) {
			MEMPOOL_ERR("key-value algorithm failed: "
				    "err %d\n", err);
		}
		break;

	case MEMPOOL_SORT_ALGORITHM:
		err = mempool_fpga_sort_algorithm(env);
		if (err) {
			MEMPOOL_ERR("sort algorithm failed: "
				    "err %d\n", err);
		}
		break;

	case MEMPOOL_SELECT_ALGORITHM:
		err = mempool_fpga_select_algorithm(env);
		if (err) {
			MEMPOOL_ERR("select algorithm failed: "
				    "err %d\n", err);
		}
		break;

	case MEMPOOL_TOTAL_ALGORITHM:
		err = mempool_fpga_total_algorithm(env);
		if (err) {
			MEMPOOL_ERR("total algorithm failed: "
				    "err %d\n", err);
		}
		break;

	default:
		err = -EOPNOTSUPP;
		MEMPOOL_ERR("unknown algorithm %#x\n",
			    env->algorithm.id);
		break;
	}

	MEMPOOL_DBG(env->show_debug,
		    "algorithm %#x has been finished: "
		    "err %d\n",
		    env->algorithm.id, err);

	return err;
}

int main(int argc, char *argv[])
{
	struct mempool_test_environment environment;
	void *input_addr = NULL;
	void *output_addr = NULL;
	off_t file_size;
	unsigned int portion_size;
	int err = 0;

	environment.input_file.fd = -1;
	environment.input_file.stream = NULL;
	environment.input_file.name = NULL;
	environment.output_file.fd = -1;
	environment.output_file.stream = NULL;
	environment.output_file.name = NULL;
	environment.uart_channel.fd = -1;
	environment.uart_channel.stream = NULL;
	environment.uart_channel.name = NULL;
	environment.threads.count = 0;
	environment.threads.portion_size = 0;
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

	if (environment.input_file.name) {
		MEMPOOL_INFO("Open input file...\n");

		environment.input_file.fd = open(environment.input_file.name,
						 O_RDONLY, 0664);
		if (environment.input_file.fd == -1) {
			err = -ENOENT;
			MEMPOOL_ERR("fail to open file: %s\n",
				    strerror(errno));
			goto finish_execution;
		}

		if (portion_size != environment.threads.portion_size) {
			err = -ERANGE;
			MEMPOOL_ERR("invalid request: "
				    "portion_size %d, granularity %d, "
				    "record_capacity %d, portion_capacity %d\n",
				    environment.threads.portion_size,
				    environment.item.granularity,
				    environment.record.capacity,
				    environment.portion.capacity);
			goto close_files;
		}

		file_size = (off_t)environment.threads.count *
				environment.threads.portion_size;

		MEMPOOL_INFO("Mmap input file...\n");

		input_addr = mmap(0, file_size, PROT_READ,
				  MAP_SHARED|MAP_POPULATE,
				  environment.input_file.fd, 0);
		if (input_addr == MAP_FAILED) {
			input_addr = NULL;
			MEMPOOL_ERR("fail to mmap input file: %s\n",
				    strerror(errno));
			goto munmap_memory;
		}

		MEMPOOL_INFO("Write data into FPGA...\n");

		err = mempool_write_data_into_fpga(&environment,
						   input_addr,
						   file_size);
		if (err) {
			MEMPOOL_ERR("fail to write data into FPGA board: "
				    "file_size %lu, err %d\n",
				    file_size, err);
			goto munmap_memory;
		}
	} else if (environment.output_file.name) {
		MEMPOOL_INFO("Open output file...\n");

		environment.output_file.fd = open(environment.output_file.name,
						  O_CREAT | O_RDWR, 0664);
		if (environment.output_file.fd == -1) {
			err = -ENOENT;
			MEMPOOL_ERR("fail to open file: %s\n",
				    strerror(errno));
			goto finish_execution;
		}

		if (portion_size != environment.threads.portion_size) {
			err = -ERANGE;
			MEMPOOL_ERR("invalid request: "
				    "portion_size %d, granularity %d, "
				    "record_capacity %d, portion_capacity %d\n",
				    environment.threads.portion_size,
				    environment.item.granularity,
				    environment.record.capacity,
				    environment.portion.capacity);
			goto close_files;
		}

		file_size = (off_t)environment.threads.count *
				environment.threads.portion_size;

		err = ftruncate(environment.output_file.fd, file_size);
		if (err) {
			MEMPOOL_ERR("fail to prepare output file: %s\n",
				    strerror(errno));
			goto close_files;
		}

		MEMPOOL_INFO("Mmap output file...\n");

		output_addr = mmap(0, file_size, PROT_READ|PROT_WRITE,
				  MAP_SHARED|MAP_POPULATE,
				  environment.output_file.fd, 0);
		if (output_addr == MAP_FAILED) {
			output_addr = NULL;
			MEMPOOL_ERR("fail to mmap output file: %s\n",
				    strerror(errno));
			goto close_files;
		}

		MEMPOOL_INFO("Read result from FPGA...\n");

		err = mempool_read_result_from_fpga(&environment,
						    output_addr,
						    file_size);
		if (err) {
			MEMPOOL_ERR("fail to read result from FPGA board: "
				    "file_size %lu, err %d\n",
				    file_size, err);
			goto munmap_memory;
		}
	} else {
		MEMPOOL_INFO("Start executing algorithm...\n");

		err = mempool_execute_algorithm_by_fpga(&environment);
		if (err) {
			MEMPOOL_ERR("fail to execute an algorithm by FPGA: "
				    "err %d\n", err);
			goto finish_execution;
		}
	}

	MEMPOOL_DBG(environment.show_debug,
		    "operation has been executed\n");

munmap_memory:
	if (input_addr) {
		err = munmap(input_addr, file_size);
		if (err) {
			MEMPOOL_ERR("fail to unmap input file: %s\n",
				    strerror(errno));
		}
	}

	if (output_addr) {
		err = munmap(output_addr, file_size);
		if (err) {
			MEMPOOL_ERR("fail to unmap output file: %s\n",
				    strerror(errno));
		}
	}

close_files:
	if (environment.input_file.fd != -1)
		close(environment.input_file.fd);

	if (environment.output_file.fd != -1)
		close(environment.output_file.fd);

finish_execution:
	exit(err ? EXIT_FAILURE : EXIT_SUCCESS);
}
