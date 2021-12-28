//SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * memory-pool-tools -- memory pool testing utilities.
 *
 * sbin/host_test.c - implementation of host testing tool.
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

#include "host_test.h"

/*
 * struct mempool_thread_state - thread state
 * @id: thread ID
 * @thread: thread descriptor
 * @env: application options
 * @input_portion: input data portion
 * @output_portion: output data portion
 * @err: code of error
 */
struct mempool_thread_state {
	int id;
	pthread_t thread;
	struct mempool_host_test_environment *env;
	void *input_portion;
	void *output_portion;
	int err;
};

void *ThreadFunc(void *arg)
{
	struct mempool_thread_state *state = (struct mempool_thread_state *)arg;

	if (!state)
		pthread_exit((void *)1);

	MEMPOOL_INFO("Thread %d, input %p, output %p\n",
		     state->id,
		     state->input_portion,
		     state->output_portion);

	pthread_exit((void *)0);
}

int main(int argc, char *argv[])
{
	struct mempool_host_test_environment environment;
	struct mempool_thread_state *pool = NULL;
	struct mempool_thread_state *cur;
	void *input_addr = NULL;
	void *output_addr = NULL;
	off_t file_size;
	int i;
	void *res;
	int err = 0;

	environment.input_file.fd = -1;
	environment.input_file.stream = NULL;
	environment.input_file.name = NULL;
	environment.output_file.fd = -1;
	environment.output_file.stream = NULL;
	environment.output_file.name = NULL;
	environment.threads.count = 0;
	environment.threads.portion_size = 0;
	environment.show_debug = MEMPOOL_FALSE;

	parse_options(argc, argv, &environment);

	MEMPOOL_DBG(environment.show_debug,
		    "options have been parsed\n");

	if (environment.threads.count == 0) {
		MEMPOOL_INFO("Nothing can be done: "
			     "threads.count %d\n",
			     environment.threads.count);
		goto finish_execution;
	}

	file_size = (off_t)environment.threads.count *
			environment.threads.portion_size;

	MEMPOOL_INFO("Open files...\n");

	environment.input_file.fd = open(environment.input_file.name,
					 O_RDONLY, 0664);
	if (environment.input_file.fd == -1) {
		err = -ENOENT;
		MEMPOOL_ERR("fail to open file: %s\n",
			    strerror(errno));
		goto finish_execution;
	}

	environment.output_file.fd = open(environment.output_file.name,
					  O_CREAT | O_RDWR, 0664);
	if (environment.output_file.fd == -1) {
		err = -ENOENT;
		MEMPOOL_ERR("fail to open file: %s\n",
			    strerror(errno));
		goto close_files;
	}

	err = ftruncate(environment.output_file.fd, file_size);
	if (err) {
		MEMPOOL_ERR("fail to prepare output file: %s\n",
			    strerror(errno));
		goto close_files;
	}

	input_addr = mmap(0, file_size, PROT_READ,
			  MAP_SHARED|MAP_POPULATE,
			  environment.input_file.fd, 0);
	if (input_addr == MAP_FAILED) {
		input_addr = NULL;
		MEMPOOL_ERR("fail to mmap input file: %s\n",
			    strerror(errno));
		goto munmap_memory;
	}

	output_addr = mmap(0, file_size, PROT_READ|PROT_WRITE,
			  MAP_SHARED|MAP_POPULATE,
			  environment.output_file.fd, 0);
	if (output_addr == MAP_FAILED) {
		output_addr = NULL;
		MEMPOOL_ERR("fail to mmap output file: %s\n",
			    strerror(errno));
		goto munmap_memory;
	}

	MEMPOOL_INFO("Create threads...\n");

	pool = calloc(environment.threads.count,
			sizeof(struct mempool_thread_state));
	if (!pool) {
		err = -ENOMEM;
		MEMPOOL_ERR("fail to allocate threads pool: %s\n",
			    strerror(errno));
		goto close_files;
	}

	for (i = 0; i < environment.threads.count; i++) {
		cur = &pool[i];

		cur->id = i;
		cur->env = &environment;
		cur->input_portion = (char *)input_addr +
				(i * environment.threads.portion_size);
		cur->output_portion = (char *)output_addr +
				(i * environment.threads.portion_size);
		cur->err = 0;

		err = pthread_create(&cur->thread, NULL,
				     ThreadFunc, (void *)cur);
		if (err) {
			MEMPOOL_ERR("fail to create thread %d: %s\n",
				    i, strerror(errno));
			for (i--; i >= 0; i--) {
				pthread_join(pool[i].thread, &res);
			}
			goto free_threads_pool;
		}
	}

	MEMPOOL_INFO("Waiting threads...\n");

	for (i = 0; i < environment.threads.count; i++) {
		cur = &pool[i];

		err = pthread_join(cur->thread, &res);
		if (err) {
			MEMPOOL_ERR("fail to finish thread %d: %s\n",
				    i, strerror(errno));
			continue;
		}

		if ((long)res != 0) {
			MEMPOOL_ERR("thread %d has failed: res %lu\n",
				    i, (long)res);
			continue;
		}

		if (cur->err != 0) {
			MEMPOOL_ERR("thread %d has failed: err %d\n",
				    i, cur->err);
		}
	}

	MEMPOOL_INFO("Threads have been destroyed...\n");

	MEMPOOL_DBG(environment.show_debug,
		    "operation has been executed\n");

free_threads_pool:
	if (pool)
		free(pool);

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
