/*
 * Copyright (c) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/arch/xtensa/cache.h>
#include <zephyr/logging/log_backend.h>
#include <zephyr/logging/log_core.h>
#include <zephyr/logging/log_output.h>
#include <zephyr/logging/log_backend_std.h>
#include <zephyr/logging/log_backend_cavs_hda.h>
#include <zephyr/drivers/dma.h>
#include <zephyr/kernel.h>

static uint32_t log_format_current = CONFIG_LOG_BACKEND_CAVS_HDA_OUTPUT_DEFAULT;
static const struct device *hda_log_dev;
static uint32_t hda_log_chan;

/*
 * HDA requires 128 byte aligned data and 128 byte aligned transfers.
 */
static __aligned(128) uint8_t hda_log_buf[CONFIG_LOG_BACKEND_CAVS_HDA_SIZE];
static volatile uint32_t hda_log_buffered;
static struct k_spinlock hda_log_lock;
static struct k_timer hda_log_timer;
static cavs_hda_log_hook_t hook;

/* atomic bit flags for state */
#define HDA_LOG_DMA_READY 0
#define HDA_LOG_PANIC_MODE 1
static atomic_t hda_log_flags;

static uint32_t hda_log_flush(void)
{
	uint32_t nearest128 = hda_log_buffered & ~((128) - 1);

	if (nearest128 > 0) {
		hda_log_buffered = hda_log_buffered - nearest128;
#if !(IS_ENABLED(CONFIG_KERNEL_COHERENCE))
		z_xtensa_cache_flush(hda_log_buf, CONFIG_LOG_BACKEND_CAVS_HDA_SIZE);
#endif
		dma_reload(hda_log_dev, hda_log_chan, 0, 0, nearest128);
	}

	return nearest128;
}


static int hda_log_out(uint8_t *data, size_t length, void *ctx)
{
	int ret;
	bool do_log_flush;
	struct dma_status status;

	k_spinlock_key_t key = k_spin_lock(&hda_log_lock);

	/* Defaults when DMA not yet initialized */
	uint32_t dma_free = sizeof(hda_log_buf);
	uint32_t write_pos = 0;

	if (atomic_test_bit(&hda_log_flags, HDA_LOG_DMA_READY)) {
		ret = dma_get_status(hda_log_dev, hda_log_chan, &status);
		if (ret != 0) {
			ret = length;
			goto out;
		}

		/* The hardware tells us what space we have available, and where to
		 * start writing. If the buffer is full we have no space.
		 */
		if (status.free <= 0) {
			ret = length;
			goto out;
		}

		dma_free = status.free;
		write_pos = status.write_position;
	}

	/* Account for buffered writes since last dma_reload
	 *
	 * No underflow should be possible here, status.free is the apparent
	 * free space in the buffer from the DMA's read/write positions.
	 * When dma_reload is called status.free may be reduced by
	 * the nearest 128 divisible value of hda_log_buffered,
	 * where hda_log_buffered is then subtracted by the same amount.
	 * After which status.free should only increase in value.
	 *
	 * Assert this trueth though, just in case.
	 */
	__ASSERT_NO_MSG(dma_free > hda_log_buffered);
	uint32_t available = dma_free - hda_log_buffered;

	/* If there isn't enough space for the message there's an overflow */
	if (available < length) {
		ret = length;
		goto out;
	}

	/* Copy over the message to the buffer */
	uint32_t idx = write_pos + hda_log_buffered;

	if (idx > sizeof(hda_log_buf)) {
		idx -= sizeof(hda_log_buf);
	}

	size_t copy_len = (idx + length) < sizeof(hda_log_buf) ? length : sizeof(hda_log_buf) - idx;

	memcpy(&hda_log_buf[idx], data, copy_len);

	/* There may be a wrapped copy */
	size_t wrap_copy_len = length - copy_len;

	if (wrap_copy_len != 0) {
		memcpy(&hda_log_buf[0], &data[copy_len], wrap_copy_len);
	}

	ret = length;
	hda_log_buffered += length;

	uint32_t written = 0;

out:
	/* If DMA_READY changes from unset to set during this call, that is
	 * perfectly acceptable. The default values for write_pos and dma_free
	 * are the correct values if that occurs.
	 */
	do_log_flush = ((hda_log_buffered > sizeof(hda_log_buf)/2) ||
			  atomic_test_bit(&hda_log_flags, HDA_LOG_PANIC_MODE))
			  && atomic_test_bit(&hda_log_flags, HDA_LOG_DMA_READY);

	if (do_log_flush) {
		written = hda_log_flush();
	}

	k_spin_unlock(&hda_log_lock, key);

	/* The hook may have log calls and needs to be done outside of the spin
	 * lock to avoid recursion on the spin lock (deadlocks) in cases of
	 * direct logging.
	 */
	if (hook != NULL && written  > 0) {
		hook(written);
	}

	return ret;
}
/**
 * 128 bytes is the smallest transferrable size for HDA so use that
 * and encompass almost all log lines in the formatter before flushing
 * and memcpy'ing to the HDA buffer.
 */
#define LOG_BUF_SIZE 128
static uint8_t log_buf[LOG_BUF_SIZE];
LOG_OUTPUT_DEFINE(log_output_cavs_hda, hda_log_out, log_buf, LOG_BUF_SIZE);

static void hda_log_periodic(struct k_timer *tm)
{
	ARG_UNUSED(tm);

	k_spinlock_key_t key = k_spin_lock(&hda_log_lock);

	uint32_t written = hda_log_flush();

	k_spin_unlock(&hda_log_lock, key);

	/* The hook may have log calls and needs to be done outside of the spin
	 * lock to avoid recursion on the spin lock (deadlocks) in cases of
	 * direct logging.
	 */
	if (hook != NULL && written  > 0) {
		hook(written);
	}
}

static inline void dropped(const struct log_backend *const backend,
			   uint32_t cnt)
{
	ARG_UNUSED(backend);

	log_output_dropped_process(&log_output_cavs_hda, cnt);
}

static void panic(struct log_backend const *const backend)
{
	ARG_UNUSED(backend);

	/* will immediately flush all future writes once set */
	atomic_set_bit(&hda_log_flags, HDA_LOG_PANIC_MODE);

	/* flushes the log queue */
	log_backend_std_panic(&log_output_cavs_hda);
}

static int format_set(const struct log_backend *const backend, uint32_t log_type)
{
	ARG_UNUSED(backend);

	log_format_current = log_type;

	return 0;
}

static uint32_t format_flags(void)
{
	uint32_t flags = LOG_OUTPUT_FLAG_LEVEL | LOG_OUTPUT_FLAG_TIMESTAMP;

	if (IS_ENABLED(CONFIG_LOG_BACKEND_FORMAT_TIMESTAMP)) {
		flags |= LOG_OUTPUT_FLAG_FORMAT_TIMESTAMP;
	}

	return flags;
}

static volatile uint32_t counter;

static void process(const struct log_backend *const backend,
		union log_msg_generic *msg)
{
	ARG_UNUSED(backend);

	log_format_func_t log_output_func = log_format_func_t_get(log_format_current);

	log_output_func(&log_output_cavs_hda, &msg->log, format_flags());
}

/**
 * Lazily initialized, while the DMA may not be setup we continue
 * to buffer log messages untilt he buffer is full.
 */
static void init(const struct log_backend *const backend)
{
	ARG_UNUSED(backend);

	hda_log_buffered = 0;
}

const struct log_backend_api log_backend_cavs_hda_api = {
	.process = process,
	.dropped = IS_ENABLED(CONFIG_LOG_MODE_IMMEDIATE) ? NULL : dropped,
	.panic = panic,
	.format_set = format_set,
	.init = init,
};

LOG_BACKEND_DEFINE(log_backend_cavs_hda, log_backend_cavs_hda_api, true);

void cavs_hda_log_init(cavs_hda_log_hook_t fn, uint32_t channel)
{
	hook = fn;

	int res;

	hda_log_dev = device_get_binding("HDA_HOST_IN");
	__ASSERT(hda_log_dev, "No valid DMA device found");

	hda_log_chan = dma_request_channel(hda_log_dev, &channel);
	__ASSERT(hda_log_chan >= 0, "No valid DMA channel");
	__ASSERT(hda_log_chan == channel, "Not requested channel");

	hda_log_buffered = 0;

	/* configure channel */
	struct dma_block_config hda_log_dma_blk_cfg = {
		.block_size = CONFIG_LOG_BACKEND_CAVS_HDA_SIZE,
		.source_address = (uint32_t)(uintptr_t)&hda_log_buf,
	};

	struct dma_config hda_log_dma_cfg = {
		.channel_direction = MEMORY_TO_HOST,
		.block_count = 1,
		.head_block = &hda_log_dma_blk_cfg,
		.source_data_size = 4,
	};

	res = dma_config(hda_log_dev, hda_log_chan, &hda_log_dma_cfg);
	__ASSERT(res == 0, "DMA config failed");

	res = dma_start(hda_log_dev, hda_log_chan);
	__ASSERT(res == 0, "DMA start failed");

	atomic_set_bit(&hda_log_flags, HDA_LOG_DMA_READY);

	k_timer_init(&hda_log_timer, hda_log_periodic, NULL);
	k_timer_start(&hda_log_timer,
		      K_MSEC(CONFIG_LOG_BACKEND_CAVS_HDA_FLUSH_TIME),
		      K_MSEC(CONFIG_LOG_BACKEND_CAVS_HDA_FLUSH_TIME));

	printk("hda log initialized\n");
}
