//SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * memory-pool-tools -- memory pool testing utilities.
 *
 * sbin/host_test.c - implementation of host testing tool.
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

static
int is_bit_set(unsigned long long mask, int bit, int capacity)
{
	unsigned long long bmap = mask;
	int check_bit;

	if (bit >= sizeof(unsigned long long) * MEMPOOL_BITS_PER_BYTE)
		return MEMPOOL_FALSE;

	check_bit = capacity - bit - 1;

	return (bmap >> check_bit) & 1;
}

static
int mempool_copy(struct mempool_thread_state *state,
		 unsigned long long mask,
		 int record_index, size_t *written_bytes)
{
	unsigned int record_size;
	unsigned char *input;
	unsigned char *output;
	int i;

	MEMPOOL_DBG(state->env->show_debug,
		    "thread %d, mask %#llx, "
		    "record_index %d, written_bytes %zu\n",
		    state->id, mask, record_index, *written_bytes);

	if (!state->input_portion || !state->output_portion) {
		MEMPOOL_ERR("fail to copy key: "
			    "thread %d, input_portion %p, output_portion %p\n",
			    state->id,
			    state->input_portion,
			    state->output_portion);
		return -ERANGE;
	}

	if (state->env->portion.count > state->env->portion.capacity) {
		MEMPOOL_ERR("invalid portion descriptor: "
			    "thread %d, count %d, capacity %d\n",
			    state->id,
			    state->env->portion.count,
			    state->env->portion.capacity);
		return -ERANGE;
	}

	if (record_index >= state->env->portion.count) {
		MEMPOOL_ERR("out of range: "
			    "thread %d, record_index %d, count %d\n",
			    state->id,
			    record_index,
			    state->env->portion.count);
		return -ERANGE;
	}

	record_size = (unsigned int)state->env->record.capacity *
					state->env->item.granularity;

	for (i = 0; i < state->env->record.capacity; i++) {
		input = (unsigned char *)state->input_portion;
		input += (unsigned int)record_index * record_size;
		input += (unsigned int)i * state->env->item.granularity;

		output = (unsigned char *)state->output_portion;
		output += *written_bytes;

		if (is_bit_set(mask, i, state->env->record.capacity)) {
			memcpy(output, input, state->env->item.granularity);
			*written_bytes += state->env->item.granularity;
		}
	}

	return 0;
}

static inline
int mempool_copy_key(struct mempool_thread_state *state,
		     int record_index, size_t *written_bytes)
{
	MEMPOOL_DBG(state->env->show_debug,
		    "thread %d, record_index %d, written_bytes %zu\n",
		    state->id,
		    record_index,
		    *written_bytes);

	return mempool_copy(state, state->env->key.mask,
			    record_index, written_bytes);
}

static inline
int mempool_copy_value(struct mempool_thread_state *state,
		       int record_index, size_t *written_bytes)
{
	MEMPOOL_DBG(state->env->show_debug,
		    "thread %d, record_index %d, written_bytes %zu\n",
		    state->id,
		    record_index,
		    *written_bytes);

	return mempool_copy(state, state->env->value.mask,
			    record_index, written_bytes);
}

static
int mempool_key_value_algorithm(struct mempool_thread_state *state)
{
	unsigned int record_size;
	unsigned int portion_bytes;
	size_t written_bytes = 0;
	int i;
	int err;

	MEMPOOL_DBG(state->env->show_debug,
		    "thread %d, input %p, output %p\n",
		    state->id,
		    state->input_portion,
		    state->output_portion);

	record_size = (unsigned int)state->env->record.capacity *
					state->env->item.granularity;
	portion_bytes = record_size * state->env->portion.capacity;

	memset(state->output_portion, 0, portion_bytes);

	for (i = 0; i < state->env->portion.count; i++) {
		if ((written_bytes + record_size) > portion_bytes) {
			MEMPOOL_ERR("out of space: "
				    "thread %d, written_bytes %zu, "
				    "portion_bytes %u\n",
				    state->id,
				    written_bytes,
				    portion_bytes);
			return -E2BIG;
		}

		err = mempool_copy_key(state, i, &written_bytes);
		if (err) {
			MEMPOOL_ERR("fail to copy key: "
				    "thread %d, record_index %d, "
				    "written_bytes %zu, err %d\n",
				    state->id, i, written_bytes, err);
			return err;
		}

		err = mempool_copy_value(state, i, &written_bytes);
		if (err) {
			MEMPOOL_ERR("fail to copy value: "
				    "thread %d, record_index %d, "
				    "written_bytes %zu, err %d\n",
				    state->id, i, written_bytes, err);
			return err;
		}
	}

	return 0;
}

static
int mempool_sort_algorithm(struct mempool_thread_state *state)
{
	MEMPOOL_ERR("unsupported algorithm %#x: "
		    "thread %d, input %p, output %p\n",
		    state->env->algorithm.id,
		    state->id,
		    state->input_portion,
		    state->output_portion);
	return -EOPNOTSUPP;
}

static
int mempool_total_algorithm(struct mempool_thread_state *state)
{
	MEMPOOL_ERR("unsupported algorithm %#x: "
		    "thread %d, input %p, output %p\n",
		    state->env->algorithm.id,
		    state->id,
		    state->input_portion,
		    state->output_portion);
	return -EOPNOTSUPP;
}

void *ThreadFunc(void *arg)
{
	struct mempool_thread_state *state = (struct mempool_thread_state *)arg;

	if (!state)
		pthread_exit((void *)1);

	MEMPOOL_DBG(state->env->show_debug,
		    "thread %d, input %p, output %p, algorithm %#x\n",
		    state->id,
		    state->input_portion,
		    state->output_portion,
		    state->env->algorithm.id);

	state->err = 0;

	switch (state->env->algorithm.id) {
	case MEMPOOL_KEY_VALUE_ALGORITHM:
		state->err = mempool_key_value_algorithm(state);
		if (state->err) {
			MEMPOOL_ERR("key-value algorithm failed: "
				    "thread %d, input %p, output %p, err %d\n",
				    state->id,
				    state->input_portion,
				    state->output_portion,
				    state->err);
		}
		break;

	case MEMPOOL_SORT_ALGORITHM:
		state->err = mempool_sort_algorithm(state);
		if (state->err) {
			MEMPOOL_ERR("sort algorithm failed: "
				    "thread %d, input %p, output %p, err %d\n",
				    state->id,
				    state->input_portion,
				    state->output_portion,
				    state->err);
		}
		break;

	case MEMPOOL_TOTAL_ALGORITHM:
		state->err = mempool_total_algorithm(state);
		if (state->err) {
			MEMPOOL_ERR("total algorithm failed: "
				    "thread %d, input %p, output %p, err %d\n",
				    state->id,
				    state->input_portion,
				    state->output_portion,
				    state->err);
		}
		break;

	default:
		state->err = -EOPNOTSUPP;
		MEMPOOL_ERR("unknown algorithm %#x: "
			    "thread %d, input %p, output %p\n",
			    state->env->algorithm.id,
			    state->id,
			    state->input_portion,
			    state->output_portion);
		break;
	}

	MEMPOOL_DBG(state->env->show_debug,
		    "algorithm %#x has been finished: "
		    "thread %d, input %p, output %p, err %d\n",
		    state->env->algorithm.id,
		    state->id,
		    state->input_portion,
		    state->output_portion,
		    state->err);

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
	unsigned int portion_size;
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
	environment.item.granularity = 1;
	environment.record.capacity = 1;
	environment.portion.capacity = 0;
	environment.portion.count = 0;
	environment.key.mask = 0;
	environment.value.mask = 0;
	environment.algorithm.id = MEMPOOL_UNKNOWN_ALGORITHM;
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

	if (portion_size != environment.threads.portion_size) {
		err = -ERANGE;
		MEMPOOL_ERR("invalid request: "
			    "portion_size %d, granularity %d, "
			    "record_capacity %d, portion_capacity %d\n",
			    environment.threads.portion_size,
			    environment.item.granularity,
			    environment.record.capacity,
			    environment.portion.capacity);
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
