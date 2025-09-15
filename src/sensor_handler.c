#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/rtio/rtio.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>

#include "sensor_handler.h"
#include "m90e26_reg.h"

LOG_MODULE_REGISTER(sensor_handler, LOG_LEVEL_INF);

// --- 內部定義 ---
static const uint8_t regs_to_poll[] = {
	APENG, ANENG, ATENG, RPENG, RNENG, RTENG, ENSTAT,
	IRMS, URMS, PMEAN, QMEAN, FREQ, POWERF, PANG, SMEAN,
	IRMS2, PMEAN2, QMEAN2, POWERF2, PANG2, SMEAN2
};

#define NUM_REGS_TO_POLL (sizeof(regs_to_poll) / sizeof(regs_to_poll[0]))
#define TOTAL_REQUESTS (SENSOR_HANDLER_NUM_INSTANCES * NUM_REGS_TO_POLL)

struct m90e26_instance {
	const struct device *dev;
	const struct rtio_iodev *iodev; // <<< 新增：儲存 iodev 指標
	char *name;
	struct k_mutex lock;
	// 資料快取區
	uint16_t apeng, aneng, ateng, rpeng, rneng, rteng, enstat;
	uint16_t irms, urms, pmean, qmean, freq, powerf, pang, smean;
	uint16_t irms2, pmean2, qmean2, powerf2, pang2, smean2;
};

struct cqe_user_context {
	struct m90e26_instance *instance;
	uint8_t reg_addr;
	uint16_t result_buf; // <<< 修改：讓每個上下文自帶一個緩衝區
};

// --- 模組內部靜態變數 ---
static struct m90e26_instance instances[SENSOR_HANDLER_NUM_INSTANCES];
static struct cqe_user_context context_pool[TOTAL_REQUESTS];
RTIO_DEFINE(m90e26_rtio_ctx, TOTAL_REQUESTS, TOTAL_REQUESTS);
atomic_t pending_responses;
static struct k_timer periodic_read_timer;

// --- 背景更新邏輯 ---
static void completion_cb(const struct rtio *r, struct rtio_cqe *cqe)
{
	struct cqe_user_context *ctx = cqe->userdata;
	uint16_t received_val = ctx->result_buf; // <<< 修改：從上下文的緩衝區讀取

	if (cqe->result == 0) {
		switch (ctx->reg_addr) {
		case APENG:  ctx->instance->apeng = received_val; break;
		/* ... (其他 case 與先前相同) ... */
		case URMS:   ctx->instance->urms = received_val; break;
		case IRMS:   ctx->instance->irms = received_val; break;
		case PMEAN:  ctx->instance->pmean = received_val; break;
		case SMEAN2:  ctx->instance->smean2 = received_val; break;
		}
	} else {
		LOG_ERR("Failed to read reg 0x%02x from %s (err: %d)",
			ctx->reg_addr, ctx->instance->name, cqe->result);
	}

	if (atomic_dec(&pending_responses) == 1) {
		LOG_INF("--- Background poll cycle finished ---");
		for (int i = 0; i < SENSOR_HANDLER_NUM_INSTANCES; i++) {
			k_mutex_unlock(&instances[i].lock);
		}
	}
}

static void periodic_read_timer_handler(struct k_timer *timer)
{
	LOG_INF("--- Background poll cycle starting ---");
	for (int i = 0; i < SENSOR_HANDLER_NUM_INSTANCES; i++) {
		if (k_mutex_lock(&instances[i].lock, K_MSEC(50)) != 0) {
			LOG_WRN("Could not lock instance %s, skipping poll.", instances[i].name);
			return;
		}
	}
	atomic_set(&pending_responses, TOTAL_REQUESTS);
	int ctx_idx = 0;
	for (int i = 0; i < SENSOR_HANDLER_NUM_INSTANCES; i++) {
		for (int j = 0; j < NUM_REGS_TO_POLL; j++) {
			struct m90e26_instance *inst = &instances[i];
			uint8_t reg = regs_to_poll[j];

			// <<< 修改：API 更新 >>>
			struct rtio_sqe *sqe = rtio_sqe_acquire(&m90e26_rtio_ctx);
			if (!sqe) {
				LOG_ERR("Failed to acquire SQE!");
				atomic_sub(&pending_responses, TOTAL_REQUESTS - ctx_idx);
				for(int k=0; k<SENSOR_HANDLER_NUM_INSTANCES; k++) k_mutex_unlock(&instances[k].lock);
				return;
			}
			context_pool[ctx_idx] = (struct cqe_user_context){ .instance = inst, .reg_addr = reg };
			sqe->userdata = &context_pool[ctx_idx];

			// <<< 修改：API 更新 >>>
			rtio_sqe_prep_read(sqe, inst->iodev, RTIO_PRIO_NORM,
					   (uint8_t*)&context_pool[ctx_idx].result_buf, // 寫入到上下文的緩衝區
					   sizeof(uint16_t),
					   (void *)(uintptr_t)reg);
			ctx_idx++;
		}
	}
	// <<< 修改：API 更新 >>>
	rtio_submit(&m90e26_rtio_ctx, TOTAL_REQUESTS);
}

// --- 公開 API 函式實作 ---
int sensor_handler_init(void)
{
	// <<< 修改：在 init 中初始化 instances 陣列 >>>
	const struct device *devs[] = {
		DEVICE_DT_GET(DT_NODELABEL(m90e26_A)),
		DEVICE_DT_GET(DT_NODELABEL(m90e26_B)),
		DEVICE_DT_GET(DT_NODELABEL(m90e26_C)),
	};
	const char *names[] = {"M90E26_A", "M90E26_B", "M90E26_C"};

	for (int i = 0; i < SENSOR_HANDLER_NUM_INSTANCES; i++) {
		instances[i].dev = devs[i];
		instances[i].name = (char*)names[i];
		if (!device_is_ready(instances[i].dev)) {
			LOG_ERR("Device %s is not ready!", instances[i].name);
			return -ENODEV;
    }
}