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
 * struct mempool_thread_queue - thread's queue
 * @lock: queue's lock
 * @state: queue's state
 * @bound: bound of a range
 * @record: temporary buffer for record
 */
struct mempool_thread_queue {
	pthread_mutex_t lock;
	int state;
	unsigned long long bound;
	void *record;
};

/*
 * Queue states
 */
enum {
	MEMPOOL_QUEUE_UNKNOWN_STATE,
	MEMPOOL_QUEUE_QUICKSORT_IN_PROGRESS,
	MEMPOOL_QUEUE_READY_FOR_EXCHANGE,
	MEMPOOL_QUEUE_PLEASE_TAKE_RECORD,
	MEMPOOL_QUEUE_NO_FREE_SPACE,
	MEMPOOL_QUEUE_FAILED,
	MEMPOOL_QUEUE_STATE_MAX
};

/*
 * struct mempool_thread_state - thread state
 * @id: thread ID
 * @thread: thread descriptor
 * @env: application options
 * @input_portion: input data portion
 * @output_portion: output data portion
 * @pool: pool of threads
 * @err: code of error
 */
struct mempool_thread_state {
	int id;
	pthread_t thread;
	struct mempool_test_environment *env;
	void *input_portion;
	void *output_portion;
	void *buf;
	unsigned int start_index;
	unsigned int end_index;
	struct mempool_thread_queue left_queue;
	struct mempool_thread_queue right_queue;
	struct mempool_thread_state *pool;
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
unsigned long long mempool_get_key(struct mempool_thread_state *state,
				   int record_index)
{
	unsigned int record_size;
	unsigned long long mask = state->env->key.mask;
	unsigned long long key = 0;
	unsigned char *input;
	unsigned char *output;
	size_t written_bytes = 0;
	int i;

	if (state->env->portion.count > state->env->portion.capacity) {
		MEMPOOL_ERR("invalid portion descriptor: "
			    "thread %d, count %d, capacity %d\n",
			    state->id,
			    state->env->portion.count,
			    state->env->portion.capacity);
		return 0;
	}

	if (record_index >= state->env->portion.count) {
		MEMPOOL_ERR("out of range: "
			    "thread %d, record_index %d, count %d\n",
			    state->id,
			    record_index,
			    state->env->portion.count);
		return 0;
	}

	record_size = (unsigned int)state->env->record.capacity *
					state->env->item.granularity;

	for (i = 0; i < state->env->record.capacity; i++) {
		input = (unsigned char *)state->output_portion;
		input += (unsigned int)record_index * record_size;
		input += (unsigned int)i * state->env->item.granularity;

		if (written_bytes >= sizeof(unsigned long long))
			return key;

		output = (unsigned char *)&key;
		output += written_bytes;

		if (is_bit_set(mask, i, state->env->record.capacity)) {
			memcpy(output, input, state->env->item.granularity);
			written_bytes += state->env->item.granularity;
		}
	}

	return key;
}

static
void mempool_swap_records(struct mempool_thread_state *state,
			  int record_index1, int record_index2)
{
	unsigned int record_size;
	unsigned char *record1;
	unsigned char *record2;

	if (state->env->portion.count > state->env->portion.capacity) {
		MEMPOOL_ERR("invalid portion descriptor: "
			    "thread %d, count %d, capacity %d\n",
			    state->id,
			    state->env->portion.count,
			    state->env->portion.capacity);
		return;
	}

	if (record_index1 >= state->env->portion.count) {
		MEMPOOL_ERR("out of range: "
			    "thread %d, record_index %d, count %d\n",
			    state->id,
			    record_index1,
			    state->env->portion.count);
		return;
	}

	if (record_index2 >= state->env->portion.count) {
		MEMPOOL_ERR("out of range: "
			    "thread %d, record_index %d, count %d\n",
			    state->id,
			    record_index2,
			    state->env->portion.count);
		return;
	}

	record_size = (unsigned int)state->env->record.capacity *
					state->env->item.granularity;

	record1 = (unsigned char *)state->output_portion;
	record1 += (unsigned int)record_index1 * record_size;

	record2 = (unsigned char *)state->output_portion;
	record2 += (unsigned int)record_index2 * record_size;

	memcpy(state->buf, record1, record_size);
	memcpy(record1, record2, record_size);
	memcpy(record2, state->buf, record_size);
}

static
int mempool_partition(struct mempool_thread_state *state,
		      int low, int high)
{
	int i;
	int p;
	int first_high;

	p = high;
	first_high = low;

	for (i = low; i < high; i++) {
		if (mempool_get_key(state, i) < mempool_get_key(state, p)) {
			mempool_swap_records(state, i, first_high);
			first_high++;
		}
	}

	mempool_swap_records(state, p, first_high);
	return first_high;
}

static
void mempool_quicksort(struct mempool_thread_state *state,
		       int low, int high)
{
	int p;

	if ((high - low) > 0) {
		p = mempool_partition(state, low, high);
		mempool_quicksort(state, low, p - 1);
		mempool_quicksort(state, p + 1, high);
	}
}

static
int mempool_send_record_to_left_thread(struct mempool_thread_state *state,
					struct mempool_thread_state *left)
{
	unsigned long long lower_bound;
	unsigned long long upper_bound;
	unsigned char *record;
	unsigned int record_size;
	int err = 0;

	if (!left)
		return -ENOSPC;

	record_size = (unsigned int)state->env->record.capacity *
					state->env->item.granularity;

	pthread_mutex_lock(&state->left_queue.lock);
	lower_bound = state->left_queue.bound;
	pthread_mutex_unlock(&state->left_queue.lock);

	pthread_mutex_lock(&left->right_queue.lock);

	switch (left->right_queue.state) {
	case MEMPOOL_QUEUE_QUICKSORT_IN_PROGRESS:
	case MEMPOOL_QUEUE_PLEASE_TAKE_RECORD:
		err = -EAGAIN;
		goto finish_send_record_to_left_thread;

	case MEMPOOL_QUEUE_READY_FOR_EXCHANGE:
		/* continue logic */
		break;

	case MEMPOOL_QUEUE_NO_FREE_SPACE:
		err = -ENOSPC;
		goto finish_send_record_to_left_thread;

	default:
		err = -EFAULT;
		goto finish_send_record_to_left_thread;
	}

	upper_bound = left->right_queue.bound;

	MEMPOOL_DBG(state->env->show_debug,
		    "thread %d, "
		    "lower_bound %#llx, upper_bound %#llx\n",
		    state->id,
		    lower_bound,
		    upper_bound);

	if (upper_bound <= lower_bound) {
		err = -ENOSPC;
		goto finish_send_record_to_left_thread;
	}

	record = (unsigned char *)state->output_portion;
	record += record_size;

	memcpy(left->right_queue.record, record, record_size);

	left->right_queue.state = MEMPOOL_QUEUE_PLEASE_TAKE_RECORD;

	state->start_index++;

finish_send_record_to_left_thread:
	pthread_mutex_unlock(&left->right_queue.lock);

	return err;
}

static
int mempool_send_record_to_right_thread(struct mempool_thread_state *state,
					struct mempool_thread_state *right)
{
	unsigned long long lower_bound;
	unsigned long long upper_bound;
	unsigned char *record;
	unsigned int record_size;
	int err = 0;

	if (!right)
		return -ENOSPC;

	if (state->env->portion.count > state->env->portion.capacity) {
		MEMPOOL_ERR("invalid portion descriptor: "
			    "thread %d, count %d, capacity %d\n",
			    state->id,
			    state->env->portion.count,
			    state->env->portion.capacity);
		return -EFAULT;
	}

	record_size = (unsigned int)state->env->record.capacity *
					state->env->item.granularity;

	pthread_mutex_lock(&state->right_queue.lock);
	upper_bound = state->right_queue.bound;
	pthread_mutex_unlock(&state->right_queue.lock);

	pthread_mutex_lock(&right->left_queue.lock);

	switch (right->left_queue.state) {
	case MEMPOOL_QUEUE_QUICKSORT_IN_PROGRESS:
	case MEMPOOL_QUEUE_PLEASE_TAKE_RECORD:
		err = -EAGAIN;
		goto finish_send_record_to_right_thread;

	case MEMPOOL_QUEUE_READY_FOR_EXCHANGE:
		/* continue logic */
		break;

	case MEMPOOL_QUEUE_NO_FREE_SPACE:
		err = -ENOSPC;
		goto finish_send_record_to_right_thread;

	default:
		err = -EFAULT;
		goto finish_send_record_to_right_thread;
	}

	lower_bound = right->left_queue.bound;

	MEMPOOL_DBG(state->env->show_debug,
		    "thread %d, "
		    "lower_bound %#llx, upper_bound %#llx\n",
		    state->id,
		    lower_bound,
		    upper_bound);

	if (upper_bound <= lower_bound) {
		err = -ENOSPC;
		goto finish_send_record_to_right_thread;
	}

	record = (unsigned char *)state->output_portion;
	record += (state->env->portion.count - 1) * record_size;

	memcpy(right->left_queue.record, record, record_size);

	right->left_queue.state = MEMPOOL_QUEUE_PLEASE_TAKE_RECORD;

	state->end_index--;

finish_send_record_to_right_thread:
	pthread_mutex_unlock(&right->left_queue.lock);

	return err;
}

static
unsigned long long mempool_get_buffer_key(struct mempool_thread_state *state)
{
	unsigned long long mask = state->env->key.mask;
	unsigned long long key;
	unsigned char *input;
	unsigned char *output;
	size_t written_bytes = 0;
	int i;

	for (i = 0; i < state->env->record.capacity; i++) {
		input = (unsigned char *)state->buf;
		input += (unsigned int)i * state->env->item.granularity;

		if (written_bytes >= sizeof(unsigned long long))
			return key;

		output = (unsigned char *)&key;
		output += written_bytes;

		if (is_bit_set(mask, i, state->env->record.capacity)) {
			memcpy(output, input, state->env->item.granularity);
			written_bytes += state->env->item.granularity;
		}
	}

	return key;
}

static
int mempool_binary_search(struct mempool_thread_state *state, int low, int high)
{
	int middle;
	unsigned long long key1, key2;

	MEMPOOL_DBG(state->env->show_debug,
		    "thread %d, low %d, high %d\n",
		    state->id, low, high);

	if (low > high) {
		MEMPOOL_DBG(state->env->show_debug,
			    "thread %d, found %d, "
			    "key1 %#llx, key2 %#llx\n",
			    state->id, low + 1,
			    mempool_get_key(state, low),
			    mempool_get_buffer_key(state));
		low++;

		if (low >= state->env->portion.count)
			low = state->env->portion.count - 1;

		return low;
	}

	middle = (low + high) / 2;

	key1 = mempool_get_key(state, middle);
	key2 = mempool_get_buffer_key(state);

	if (key1 == key2) {
		MEMPOOL_DBG(state->env->show_debug,
			    "thread %d, found %d, "
			    "key1 %#llx, key2 %#llx\n",
			    state->id, middle,
			    mempool_get_key(state, middle),
			    mempool_get_buffer_key(state));
		return middle;
	}

	if (key1 > key2) {
		return mempool_binary_search(state, low, middle - 1);
	} else {
		return mempool_binary_search(state, middle + 1, high);
	}
}

static
int mempool_take_record_from_left_thread(struct mempool_thread_state *state)
{
	unsigned char *record;
	unsigned int record_size;
	int position;
	int err = 0;

	if (state->env->portion.count > state->env->portion.capacity) {
		MEMPOOL_ERR("invalid portion descriptor: "
			    "thread %d, count %d, capacity %d\n",
			    state->id,
			    state->env->portion.count,
			    state->env->portion.capacity);
		return -EFAULT;
	}

	record_size = (unsigned int)state->env->record.capacity *
					state->env->item.granularity;

	pthread_mutex_lock(&state->left_queue.lock);

	switch (state->left_queue.state) {
	case MEMPOOL_QUEUE_READY_FOR_EXCHANGE:
		err = -EAGAIN;
		goto finish_take_record_from_left_thread;

	case MEMPOOL_QUEUE_PLEASE_TAKE_RECORD:
		/* continue logic */
		break;

	case MEMPOOL_QUEUE_NO_FREE_SPACE:
		err = -ENOSPC;
		goto finish_take_record_from_left_thread;

	default:
		err = -EFAULT;
		goto finish_take_record_from_left_thread;
	}

	memcpy(state->buf, state->left_queue.record, record_size);

	position = mempool_binary_search(state,
					 state->start_index,
					 state->end_index);

	if (position < 0 || position >= state->env->portion.count) {
		err = -EFAULT;
		MEMPOOL_ERR("invalid position: "
			    "thread %d, position %d\n",
			    state->id,
			    position);
		goto finish_take_record_from_left_thread;
	}

	record = (unsigned char *)state->output_portion;
	record += record_size;

	memmove(state->output_portion, record, position * record_size);

	record = (unsigned char *)state->output_portion;
	record += position * record_size;

	memcpy(record, state->buf, record_size);

	state->start_index = 0;

	state->left_queue.bound = mempool_get_key(state, 0);

	state->left_queue.state = MEMPOOL_QUEUE_READY_FOR_EXCHANGE;

finish_take_record_from_left_thread:
	pthread_mutex_unlock(&state->left_queue.lock);

	return err;
}

static
int mempool_take_record_from_right_thread(struct mempool_thread_state *state)
{
	unsigned char *record;
	unsigned int record_size;
	int position;
	int err = 0;

	if (state->env->portion.count > state->env->portion.capacity) {
		MEMPOOL_ERR("invalid portion descriptor: "
			    "thread %d, count %d, capacity %d\n",
			    state->id,
			    state->env->portion.count,
			    state->env->portion.capacity);
		return -EFAULT;
	}

	record_size = (unsigned int)state->env->record.capacity *
					state->env->item.granularity;

	pthread_mutex_lock(&state->right_queue.lock);

	switch (state->right_queue.state) {
	case MEMPOOL_QUEUE_READY_FOR_EXCHANGE:
		err = -EAGAIN;
		goto finish_take_record_from_right_thread;

	case MEMPOOL_QUEUE_PLEASE_TAKE_RECORD:
		/* continue logic */
		break;

	case MEMPOOL_QUEUE_NO_FREE_SPACE:
		err = -ENOSPC;
		goto finish_take_record_from_right_thread;

	default:
		err = -EFAULT;
		goto finish_take_record_from_right_thread;
	}

	memcpy(state->buf, state->right_queue.record, record_size);

	position = mempool_binary_search(state,
					 state->start_index,
					 state->end_index);

	if (position < 0 || position >= state->env->portion.count) {
		err = -EFAULT;
		MEMPOOL_ERR("invalid position: "
			    "thread %d, position %d\n",
			    state->id,
			    position);
		goto finish_take_record_from_right_thread;
	}

	record = (unsigned char *)state->output_portion;
	record += position * record_size;

	memmove(record + record_size, record,
		((state->env->portion.count - 1) - position) * record_size);

	record = (unsigned char *)state->output_portion;
	record += position * record_size;

	memcpy(record, state->buf, record_size);

	state->end_index = state->env->portion.count - 1;

	state->right_queue.bound =
		mempool_get_key(state, state->env->portion.count - 1);

	state->right_queue.state = MEMPOOL_QUEUE_READY_FOR_EXCHANGE;

finish_take_record_from_right_thread:
	pthread_mutex_unlock(&state->right_queue.lock);

	return err;
}

static
void mempool_exchange_sort(struct mempool_thread_state *state)
{
	struct mempool_thread_state *left_thread = NULL;
	struct mempool_thread_state *right_thread = NULL;
	unsigned long long lower_bound;
	unsigned long long upper_bound;
	int queue_state = MEMPOOL_QUEUE_READY_FOR_EXCHANGE;
	int err1, err2;
	int err = 0;

	if (state->id != 0)
		left_thread = &state->pool[state->id - 1];
	else {
		pthread_mutex_lock(&state->left_queue.lock);
		state->left_queue.state = MEMPOOL_QUEUE_NO_FREE_SPACE;
		pthread_mutex_unlock(&state->left_queue.lock);
	}

	if ((state->id + 1) < state->env->threads.count)
		right_thread = &state->pool[state->id + 1];
	else {
		pthread_mutex_lock(&state->right_queue.lock);
		state->right_queue.state = MEMPOOL_QUEUE_NO_FREE_SPACE;
		pthread_mutex_unlock(&state->right_queue.lock);
	}

	lower_bound = mempool_get_key(state, 0);
	upper_bound = mempool_get_key(state, state->env->portion.count - 1);

	pthread_mutex_lock(&state->left_queue.lock);
	state->left_queue.bound = lower_bound;
	state->left_queue.state = queue_state;
	pthread_mutex_unlock(&state->left_queue.lock);

	pthread_mutex_lock(&state->right_queue.lock);
	state->right_queue.bound = upper_bound;
	state->right_queue.state = queue_state;
	pthread_mutex_unlock(&state->right_queue.lock);

	while (queue_state == MEMPOOL_QUEUE_READY_FOR_EXCHANGE) {
		err = mempool_send_record_to_left_thread(state, left_thread);
		if (err == -EFAULT) {
			MEMPOOL_ERR("fail to send record to left thread: "
				    "thread %d\n", state->id);
			goto finish_exchange_sort;
		} else if (err == -ENOSPC) {
			err = 0;
			pthread_mutex_lock(&state->left_queue.lock);
			state->left_queue.state = MEMPOOL_QUEUE_NO_FREE_SPACE;
			pthread_mutex_unlock(&state->left_queue.lock);
		}

		err = mempool_send_record_to_right_thread(state, right_thread);
		if (err == -EFAULT) {
			MEMPOOL_ERR("fail to send record to right thread: "
				    "thread %d\n", state->id);
			goto finish_exchange_sort;
		} else if (err == -ENOSPC) {
			err = 0;
			pthread_mutex_lock(&state->right_queue.lock);
			state->right_queue.state = MEMPOOL_QUEUE_NO_FREE_SPACE;
			pthread_mutex_unlock(&state->right_queue.lock);
		} else if (err == -EAGAIN)
			continue;

		err1 = mempool_take_record_from_left_thread(state);
		if (err1 == -EFAULT) {
			err = err1;
			MEMPOOL_ERR("fail to process record from left thread: "
				    "thread %d\n", state->id);
			goto finish_exchange_sort;
		} else if (err1 == -ENOSPC) {
			MEMPOOL_DBG(state->env->show_debug,
				    "no free space: thread %d\n",
				    state->id);
			pthread_mutex_lock(&state->left_queue.lock);
			state->left_queue.state = MEMPOOL_QUEUE_NO_FREE_SPACE;
			pthread_mutex_unlock(&state->left_queue.lock);
		}

		err2 = mempool_take_record_from_right_thread(state);
		if (err2 == -EFAULT) {
			err = err2;
			MEMPOOL_ERR("fail to process record from right thread: "
				    "thread %d\n", state->id);
			goto finish_exchange_sort;
		} else if (err2 == -ENOSPC) {
			MEMPOOL_DBG(state->env->show_debug,
				    "no free space: thread %d\n",
				    state->id);
			pthread_mutex_lock(&state->right_queue.lock);
			state->right_queue.state = MEMPOOL_QUEUE_NO_FREE_SPACE;
			pthread_mutex_unlock(&state->right_queue.lock);
		}

		if (err1 == -ENOSPC && err2 == -ENOSPC)
			queue_state = MEMPOOL_QUEUE_NO_FREE_SPACE;
	};

finish_exchange_sort:
	if (err) {
		pthread_mutex_lock(&state->left_queue.lock);
		state->left_queue.state = MEMPOOL_QUEUE_FAILED;
		pthread_mutex_unlock(&state->left_queue.lock);

		pthread_mutex_lock(&state->right_queue.lock);
		state->right_queue.state = MEMPOOL_QUEUE_FAILED;
		pthread_mutex_unlock(&state->right_queue.lock);
	}

	return;
}

static
int mempool_sort_algorithm(struct mempool_thread_state *state)
{
	unsigned int record_size;
	unsigned int portion_bytes;
	int err = 0;

	MEMPOOL_DBG(state->env->show_debug,
		    "thread %d, input %p, output %p\n",
		    state->id,
		    state->input_portion,
		    state->output_portion);

	record_size = (unsigned int)state->env->record.capacity *
					state->env->item.granularity;
	portion_bytes = record_size * state->env->portion.capacity;

	memcpy(state->output_portion, state->input_portion,
		portion_bytes);

	state->buf = calloc(1, record_size);
	if (!state->buf) {
		err = -ENOMEM;
		MEMPOOL_ERR("fail to allocate buffer: "
			    "thread %d, %s\n",
			    state->id,
			    strerror(errno));
		goto finish_algorithm;
	}

	pthread_mutex_lock(&state->left_queue.lock);
	state->left_queue.record = calloc(1, record_size);
	if (!state->left_queue.record) {
		err = -ENOMEM;
		state->left_queue.state = MEMPOOL_QUEUE_FAILED;
		MEMPOOL_ERR("fail to allocate buffer: "
			    "thread %d, %s\n",
			    state->id,
			    strerror(errno));
		goto finish_algorithm;
	}
	state->left_queue.state = MEMPOOL_QUEUE_QUICKSORT_IN_PROGRESS;
	pthread_mutex_unlock(&state->left_queue.lock);

	pthread_mutex_lock(&state->right_queue.lock);
	state->right_queue.record = calloc(1, record_size);
	if (!state->right_queue.record) {
		err = -ENOMEM;
		state->right_queue.state = MEMPOOL_QUEUE_FAILED;
		MEMPOOL_ERR("fail to allocate buffer: "
			    "thread %d, %s\n",
			    state->id,
			    strerror(errno));
		goto finish_algorithm;
	}
	state->right_queue.state = MEMPOOL_QUEUE_QUICKSORT_IN_PROGRESS;
	pthread_mutex_unlock(&state->right_queue.lock);

	mempool_quicksort(state, 0, state->env->portion.count - 1);

	mempool_exchange_sort(state);

finish_algorithm:
	if (state->buf)
		free(state->buf);

	if (state->left_queue.record)
		free(state->left_queue.record);

	if (state->right_queue.record)
		free(state->right_queue.record);

	return err;
}

static
unsigned long long mempool_get_input_key(struct mempool_thread_state *state,
					 int record_index)
{
	unsigned int record_size;
	unsigned long long mask = state->env->key.mask;
	unsigned long long key = 0;
	unsigned char *input;
	unsigned char *output;
	size_t written_bytes = 0;
	int i;

	if (state->env->portion.count > state->env->portion.capacity) {
		MEMPOOL_ERR("invalid portion descriptor: "
			    "thread %d, count %d, capacity %d\n",
			    state->id,
			    state->env->portion.count,
			    state->env->portion.capacity);
		return 0;
	}

	if (record_index >= state->env->portion.count) {
		MEMPOOL_ERR("out of range: "
			    "thread %d, record_index %d, count %d\n",
			    state->id,
			    record_index,
			    state->env->portion.count);
		return 0;
	}

	record_size = (unsigned int)state->env->record.capacity *
					state->env->item.granularity;

	for (i = 0; i < state->env->record.capacity; i++) {
		input = (unsigned char *)state->input_portion;
		input += (unsigned int)record_index * record_size;
		input += (unsigned int)i * state->env->item.granularity;

		if (written_bytes >= sizeof(unsigned long long))
			return key;

		output = (unsigned char *)&key;
		output += written_bytes;

		if (is_bit_set(mask, i, state->env->record.capacity)) {
			memcpy(output, input, state->env->item.granularity);
			written_bytes += state->env->item.granularity;
		}
	}

	return key;
}


static
int mempool_select_algorithm(struct mempool_thread_state *state)
{
	unsigned int record_size;
	unsigned int portion_bytes;
	size_t written_bytes = 0;
	unsigned long long min;
	unsigned long long max;
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
	min = state->env->condition.min;
	max = state->env->condition.max;

	memset(state->output_portion, 0, portion_bytes);

	for (i = 0; i < state->env->portion.count; i++) {
		unsigned long long key = 0;

		if ((written_bytes + record_size) > portion_bytes) {
			MEMPOOL_ERR("out of space: "
				    "thread %d, written_bytes %zu, "
				    "portion_bytes %u\n",
				    state->id,
				    written_bytes,
				    portion_bytes);
			return -E2BIG;
		}

		key = mempool_get_input_key(state, i);

		MEMPOOL_DBG(state->env->show_debug,
			    "thread %d, key %llu, min %llu, max %llu\n",
			    state->id,
			    key, min, max);

		if (min <= key && key < max) {
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
	}

	return 0;
}

static
int mempool_add_value(struct mempool_thread_state *state,
		      unsigned long long mask,
		      int record_index, size_t *written_bytes)
{
	unsigned int record_size;
	unsigned char *input;
	unsigned long long *output;
	int value_items = 0;
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
		if (is_bit_set(mask, i, state->env->record.capacity))
			value_items++;
	}

	*written_bytes = 0;

	if (value_items == 0)
		return 0;

	for (i = 0; i < state->env->record.capacity; i++) {
		input = (unsigned char *)state->input_portion;
		input += (unsigned int)record_index * record_size;
		input += (unsigned int)i * state->env->item.granularity;

		output = (unsigned long long *)state->output_portion;
		output += i;

		if (is_bit_set(mask, i, state->env->record.capacity)) {
			*output += *input;
		}
	}

	*written_bytes = state->env->item.granularity * value_items;

	return 0;
}

static
int mempool_total_algorithm(struct mempool_thread_state *state)
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

		err = mempool_add_value(state, state->env->value.mask,
					i, &written_bytes);
		if (err) {
			MEMPOOL_ERR("fail to add value: "
				    "thread %d, record_index %d, "
				    "written_bytes %zu, err %d\n",
				    state->id, i, written_bytes, err);
			return err;
		}
	}

	return 0;
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

	case MEMPOOL_SELECT_ALGORITHM:
		state->err = mempool_select_algorithm(state);
		if (state->err) {
			MEMPOOL_ERR("select algorithm failed: "
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
	struct mempool_test_environment environment;
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
	environment.condition.min = 0;
	environment.condition.max = ULLONG_MAX;
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
		cur->buf = NULL;

		cur->start_index = 0;
		cur->end_index = environment.portion.count - 1;

		pthread_mutex_init(&cur->left_queue.lock, NULL);
		cur->left_queue.state = MEMPOOL_QUEUE_UNKNOWN_STATE;
		cur->left_queue.bound = ULLONG_MAX;
		cur->left_queue.record = NULL;

		pthread_mutex_init(&cur->right_queue.lock, NULL);
		cur->right_queue.state = MEMPOOL_QUEUE_UNKNOWN_STATE;
		cur->right_queue.bound = ULLONG_MAX;
		cur->right_queue.record = NULL;

		cur->pool = pool;

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
