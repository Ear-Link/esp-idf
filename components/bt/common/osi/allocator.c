/******************************************************************************
 *
 *  Copyright (C) 2014 Google, Inc.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/
#include <stdlib.h>
#include <string.h>

#include "bt_common.h"
#include "btc_a2dp_source.h"
#include "esp_log.h"
#include "osi/allocator.h"
#include "osi/mutex.h"

extern void *pvPortZalloc(size_t size);
extern void vPortFree(void *pv);


#if HEAP_MEMORY_DEBUG

#define OSI_MEM_DBG_INFO_MAX    1024*3
typedef struct {
    void *p;
    int size;
    const char *func;
    int line;
} osi_mem_dbg_info_t;

static uint32_t mem_dbg_count = 0;
static osi_mem_dbg_info_t mem_dbg_info[OSI_MEM_DBG_INFO_MAX];
static uint32_t mem_dbg_current_size = 0;
static uint32_t mem_dbg_max_size = 0;

#define OSI_MEM_DBG_MAX_SECTION_NUM 5
typedef struct {
    bool used;
    uint32_t max_size;
} osi_mem_dbg_max_size_section_t;
static osi_mem_dbg_max_size_section_t mem_dbg_max_size_section[OSI_MEM_DBG_MAX_SECTION_NUM];

void osi_mem_dbg_init(void)
{
    int i;

    for (i = 0; i < OSI_MEM_DBG_INFO_MAX; i++) {
        mem_dbg_info[i].p = NULL;
        mem_dbg_info[i].size = 0;
        mem_dbg_info[i].func = NULL;
        mem_dbg_info[i].line = 0;
    }
    mem_dbg_count = 0;
    mem_dbg_current_size = 0;
    mem_dbg_max_size = 0;

    for (i = 0; i < OSI_MEM_DBG_MAX_SECTION_NUM; i++){
        mem_dbg_max_size_section[i].used = false;
        mem_dbg_max_size_section[i].max_size = 0;
    }
}

void osi_mem_dbg_record(void *p, int size, const char *func, int line)
{
    int i;

    if (!p || size == 0) {
        OSI_TRACE_ERROR("%s invalid !!\n", __func__);
        return;
    }

    for (i = 0; i < OSI_MEM_DBG_INFO_MAX; i++) {
        if (mem_dbg_info[i].p == NULL) {
            mem_dbg_info[i].p = p;
            mem_dbg_info[i].size = size;
            mem_dbg_info[i].func = func;
            mem_dbg_info[i].line = line;
            mem_dbg_count++;
            break;
        }
    }

    if (i >= OSI_MEM_DBG_INFO_MAX) {
        OSI_TRACE_ERROR("%s full %s %d !!\n", __func__, func, line);
    }

    mem_dbg_current_size += size;
    if(mem_dbg_max_size < mem_dbg_current_size) {
        mem_dbg_max_size = mem_dbg_current_size;
    }

    for (i = 0; i < OSI_MEM_DBG_MAX_SECTION_NUM; i++){
        if (mem_dbg_max_size_section[i].used) {
            if(mem_dbg_max_size_section[i].max_size < mem_dbg_current_size) {
                mem_dbg_max_size_section[i].max_size = mem_dbg_current_size;
            }
        }
    }
}

void osi_mem_dbg_clean(void *p, const char *func, int line)
{
    int i;

    if (!p) {
        OSI_TRACE_ERROR("%s invalid\n", __func__);
        return;
    }

    for (i = 0; i < OSI_MEM_DBG_INFO_MAX; i++) {
        if (mem_dbg_info[i].p == p) {
            mem_dbg_current_size -= mem_dbg_info[i].size;
            mem_dbg_info[i].p = NULL;
            mem_dbg_info[i].size = 0;
            mem_dbg_info[i].func = NULL;
            mem_dbg_info[i].line = 0;
            mem_dbg_count--;
            break;
        }
    }

    if (i >= OSI_MEM_DBG_INFO_MAX) {
        OSI_TRACE_ERROR("%s full %s %d !!\n", __func__, func, line);
    }
}

void osi_mem_dbg_show(void)
{
    int i;

    for (i = 0; i < OSI_MEM_DBG_INFO_MAX; i++) {
        if (mem_dbg_info[i].p || mem_dbg_info[i].size != 0 ) {
            OSI_TRACE_ERROR("--> p %p, s %d, f %s, l %d\n", mem_dbg_info[i].p, mem_dbg_info[i].size, mem_dbg_info[i].func, mem_dbg_info[i].line);
        }
    }
    OSI_TRACE_ERROR("--> count %d\n", mem_dbg_count);
    OSI_TRACE_ERROR("--> size %dB\n--> max size %dB\n", mem_dbg_current_size, mem_dbg_max_size);
}

uint32_t osi_mem_dbg_get_max_size(void)
{
    return mem_dbg_max_size;
}

uint32_t osi_mem_dbg_get_current_size(void)
{
    return mem_dbg_current_size;
}

void osi_men_dbg_set_section_start(uint8_t index)
{
    if (index >= OSI_MEM_DBG_MAX_SECTION_NUM) {
        OSI_TRACE_ERROR("Then range of index should be between 0 and %d, current index is %d.\n",
                            OSI_MEM_DBG_MAX_SECTION_NUM - 1, index);
        return;
    }

    if (mem_dbg_max_size_section[index].used) {
        OSI_TRACE_WARNING("This index(%d) has been started, restart it.\n", index);
    }

    mem_dbg_max_size_section[index].used = true;
    mem_dbg_max_size_section[index].max_size = mem_dbg_current_size;
}

void osi_men_dbg_set_section_end(uint8_t index)
{
    if (index >= OSI_MEM_DBG_MAX_SECTION_NUM) {
        OSI_TRACE_ERROR("Then range of index should be between 0 and %d, current index is %d.\n",
                            OSI_MEM_DBG_MAX_SECTION_NUM - 1, index);
        return;
    }

    if (!mem_dbg_max_size_section[index].used) {
        OSI_TRACE_ERROR("This index(%d) has not been started.\n", index);
        return;
    }

    mem_dbg_max_size_section[index].used = false;
}

uint32_t osi_mem_dbg_get_max_size_section(uint8_t index)
{
    if (index >= OSI_MEM_DBG_MAX_SECTION_NUM){
        OSI_TRACE_ERROR("Then range of index should be between 0 and %d, current index is %d.\n",
                            OSI_MEM_DBG_MAX_SECTION_NUM - 1, index);
        return 0;
    }

    return mem_dbg_max_size_section[index].max_size;
}
#endif

char *osi_strdup(const char *str)
{
    size_t size = strlen(str) + 1;  // + 1 for the null terminator
    char *new_string = (char *)osi_calloc(size);

    if (!new_string) {
        return NULL;
    }

    memcpy(new_string, str, size);
    return new_string;
}

void *osi_malloc_func(size_t size)
{
    void *p = osi_malloc_base(size);

    if (size != 0 && p == NULL) {
        OSI_TRACE_ERROR("malloc failed (caller=%p size=%u)\n", __builtin_return_address(0), size);
#if HEAP_ALLOCATION_FAILS_ABORT
        assert(0);
#endif
    }

    return p;
}

void *osi_calloc_func(size_t size)
{
    void *p = osi_calloc_base(size);

    if (size != 0 && p == NULL) {
        OSI_TRACE_ERROR("calloc failed (caller=%p size=%u)\n", __builtin_return_address(0), size);
#if HEAP_ALLOCATION_FAILS_ABORT
        assert(0);
#endif
    }

    return p;
}

void osi_free_func(void *ptr)
{
#if HEAP_MEMORY_DEBUG
    osi_mem_dbg_clean(ptr, __func__, __LINE__);
#endif
    free(ptr);
}

#define TAG "SBC_POOL"
/**
 * Preallocated SBC buffer pool
 *
 * (MAX_PCM_FRAME_NUM_PER_TICK + MAX_OUTPUT_A2DP_SRC_FRAME_QUEUE_SZ) - that's
 * just too much
 *
 * 1 buffer is too low - sometimes we get "SBC buffer pool exhausted"
 * 2 buffer is quite ok
 * 7 or more buffers - too much memory is wasted, other tasks may starve
 */
#define SBC_BUFFER_POOL_SIZE 2

// Each buffer should be BTC_MEDIA_AA_BUF_SIZE bytes total
// We need to allocate the header + data area together
typedef struct {
    BT_HDR hdr; // Header comes first
    uint8_t data[BTC_MEDIA_AA_BUF_SIZE -
                 sizeof(BT_HDR)]; // Remaining space for data
} sbc_complete_buffer_t;

static sbc_complete_buffer_t sbc_buffer_pool[SBC_BUFFER_POOL_SIZE];
static bool sbc_buffer_used[SBC_BUFFER_POOL_SIZE];
static osi_mutex_t sbc_buffer_lock;
static bool sbc_buffer_initialized = false;

esp_err_t sbc_buffer_pool_init(void) {
    if (sbc_buffer_initialized)
        return ESP_OK;

    esp_err_t ret = osi_mutex_new(&sbc_buffer_lock);
    if (ret != ESP_OK)
        return ret;

    memset(sbc_buffer_used, 0, sizeof(sbc_buffer_used));
    sbc_buffer_initialized = true;

    ESP_EARLY_LOGD(
        TAG, "SBC buffer pool initialized, range: %p - %p (size: %d bytes)",
        &sbc_buffer_pool[0],
        (uint8_t*)&sbc_buffer_pool[0] + sizeof(sbc_buffer_pool) - 1,
        sizeof(sbc_buffer_pool));
    return ESP_OK;
}

BT_HDR* sbc_buffer_alloc(void) {
    if (!sbc_buffer_initialized) {
        ESP_EARLY_LOGD(TAG, "Attempt to free SBC buffer before initialization");
        return NULL;
    }

    osi_mutex_lock(&sbc_buffer_lock, OSI_MUTEX_MAX_TIMEOUT);

    for (int i = 0; i < SBC_BUFFER_POOL_SIZE; i++) {
        if (!sbc_buffer_used[i]) {
            sbc_buffer_used[i] = true;
            osi_mutex_unlock(&sbc_buffer_lock);
            ESP_EARLY_LOGD(TAG, "Allocated buffer %d at %p", i,
                           &sbc_buffer_pool[i]);
            return (BT_HDR*)&sbc_buffer_pool[i];
        }
    }

    osi_mutex_unlock(&sbc_buffer_lock);
    ESP_EARLY_LOGD(TAG, "SBC buffer pool exhausted");
    return NULL;
}

esp_err_t sbc_buffer_free(BT_HDR* buf) {
    if (!sbc_buffer_initialized) {
        ESP_EARLY_LOGD(TAG, "Attempt to free SBC buffer before initialization");
        return ESP_ERR_INVALID_STATE;
    }
    if (buf == NULL) {
        ESP_EARLY_LOGD(TAG, "Attempt to free NULL SBC buffer");
        return ESP_ERR_INVALID_ARG;
    }

    // Quick check: is the pointer within the general pool area?
    if ((uint8_t*)buf < (uint8_t*)sbc_buffer_pool ||
        (uint8_t*)buf >= (uint8_t*)sbc_buffer_pool + sizeof(sbc_buffer_pool)) {
        ESP_EARLY_LOGD(
            TAG, "Attempt to free pointer %p outside of SBC buffer pool range",
            buf);
        return ESP_ERR_INVALID_ARG;
    }

    osi_mutex_lock(&sbc_buffer_lock, OSI_MUTEX_MAX_TIMEOUT);

    // Check exact alignment
    size_t offset = (uint8_t*)buf - (uint8_t*)sbc_buffer_pool;
    if (offset % BTC_MEDIA_AA_BUF_SIZE != 0) {
        osi_mutex_unlock(&sbc_buffer_lock);
        ESP_EARLY_LOGD(TAG, "Attempt to free unaligned SBC buffer pointer %p",
                       buf);
        return ESP_ERR_INVALID_ARG;
    }

    int idx = offset / BTC_MEDIA_AA_BUF_SIZE;
    if (idx >= 0 && idx < SBC_BUFFER_POOL_SIZE) {
        sbc_buffer_used[idx] = false;
        osi_mutex_unlock(&sbc_buffer_lock);
        ESP_EARLY_LOGD(TAG, "Freed buffer %d at %p", idx, buf);
        return ESP_OK;
    }

    osi_mutex_unlock(&sbc_buffer_lock);
    ESP_EARLY_LOGD(TAG, "Attempt to free invalid SBC buffer pointer %p", buf);

    return ESP_ERR_INVALID_ARG;
}