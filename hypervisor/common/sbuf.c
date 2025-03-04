/*
 * SHARED BUFFER
 *
 * Copyright (C) 2017-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Li Fei <fei1.li@intel.com>
 *
 */

#include <types.h>
#include <rtl.h>
#include <errno.h>
#include <asm/cpu.h>
#include <asm/per_cpu.h>
#include <vm_event.h>

uint32_t sbuf_next_ptr(uint32_t pos_arg,
		uint32_t span, uint32_t scope)
{
	uint32_t pos = pos_arg;
	pos += span;
	pos = (pos >= scope) ? (pos - scope) : pos;
	return pos;
}

/**
 * The high caller should guarantee each time there must have
 * sbuf->ele_size data can be write form data and this function
 * should guarantee execution atomically.
 *
 * flag:
 * If OVERWRITE_EN set, buf can store (ele_num - 1) elements at most.
 * Should use lock to guarantee that only one read or write at
 * the same time.
 * if OVERWRITE_EN not set, buf can store (ele_num - 1) elements
 * at most. Shouldn't modify the sbuf->head.
 *
 * return:
 * ele_size:	write succeeded.
 * 0:		no write, buf is full
 * negative:	failed.
 */

uint32_t sbuf_put(struct shared_buf *sbuf, uint8_t *data)
{
	void *to;
	uint32_t next_tail;
	uint32_t ele_size;
	bool trigger_overwrite = false;

	stac();
	next_tail = sbuf_next_ptr(sbuf->tail, sbuf->ele_size, sbuf->size);

	if ((next_tail == sbuf->head) && ((sbuf->flags & OVERWRITE_EN) == 0U)) {
		/* if overrun is not enabled, return 0 directly */
		ele_size = 0U;
	} else {
		if (next_tail == sbuf->head) {
			/* accumulate overrun count if necessary */
			sbuf->overrun_cnt += sbuf->flags & OVERRUN_CNT_EN;
			trigger_overwrite = true;
		}
		to = (void *)sbuf + SBUF_HEAD_SIZE + sbuf->tail;

		(void)memcpy_s(to, sbuf->ele_size, data, sbuf->ele_size);
		/* make sure write data before update head */
		cpu_write_memory_barrier();

		if (trigger_overwrite) {
			sbuf->head = sbuf_next_ptr(sbuf->head,
					sbuf->ele_size, sbuf->size);
		}
		sbuf->tail = next_tail;
		ele_size = sbuf->ele_size;
	}
	clac();

	return ele_size;
}

int32_t sbuf_setup_common(struct acrn_vm *vm, uint16_t cpu_id, uint32_t sbuf_id, uint64_t *hva)
{
	int32_t ret = 0;

	switch (sbuf_id) {
		case ACRN_TRACE:
		case ACRN_HVLOG:
		case ACRN_SEP:
		case ACRN_SOCWATCH:
			ret = sbuf_share_setup(cpu_id, sbuf_id, hva);
			break;
		case ACRN_ASYNCIO:
			ret = init_asyncio(vm, hva);
			break;
		case ACRN_VM_EVENT:
			ret = init_vm_event(vm, hva);
			break;
		default:
			pr_err("%s not support sbuf_id %d", __func__, sbuf_id);
			ret = -1;
	}

	return ret;
}
