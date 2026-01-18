#include "cfg_store.h"
#include <string.h>
#include <stdio.h>

#define CFG_FLASH_ADDR        (0x0807F800UL)   // last page of 512KB
#define CFG_MAGIC             (0x43464731UL)   // 'CFG1'
#define CFG_VERSION (0x00010002UL)   // tăng lên 1 nấc


typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t version;
    ph_ctrl_cfg_t cfg;
    uint32_t crc32;
} cfg_blob_t;

//ph_ctrl_cfg_t g_cfg;

/* CRC32 software */
static uint32_t crc32_sw(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    for(uint32_t i=0; i<len; i++) {
        crc ^= data[i];
        for(int b=0; b<8; b++) {
            uint32_t mask = -(crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

static void cfg_set_default(void)
{
    g_cfg.ph_high    = 8.00f;
    g_cfg.ph_low     = 7.00f;
    g_cfg.time_off   = 50;
    g_cfg.time_on    = 2;
    g_cfg.pump_power = 25;
    g_cfg.control_mode = 0;   // mặc định Offline
}

void cfg_print(const ph_ctrl_cfg_t *cfg, const char *tag)
{
    printf("[%s] ph_high=%.2f ph_low=%.2f time_off=%lu time_on=%lu pump_power=%lu\r\n",
           tag ? tag : "CFG",
           cfg->ph_high, cfg->ph_low,
           (unsigned long)cfg->time_off,
           (unsigned long)cfg->time_on,
           (unsigned long)cfg->pump_power);
}

static bool flash_erase_page(uint32_t page_addr)
{
    HAL_FLASH_Unlock();

    FLASH_EraseInitTypeDef erase = {0};
    uint32_t page_error = 0;
    erase.TypeErase   = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = page_addr;
    erase.NbPages     = 1;

    if (HAL_FLASHEx_Erase(&erase, &page_error) != HAL_OK) {
        HAL_FLASH_Lock();
        return false;
    }

    HAL_FLASH_Lock();
    return true;
}

/* Program theo HALFWORD cho F1 (an toàn nhất) */
static bool flash_program_halfwords(uint32_t addr, const uint8_t *data, uint32_t len)
{
    if(len % 2 != 0) return false;

    HAL_FLASH_Unlock();

    for(uint32_t i=0; i<len; i+=2) {
        uint16_t hw;
        memcpy(&hw, data + i, 2);
        if(HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, addr + i, hw) != HAL_OK) {
            HAL_FLASH_Lock();
            return false;
        }
    }

    HAL_FLASH_Lock();
    return true;
}

bool cfg_load_from_flash(void)
{
    const cfg_blob_t *blob = (const cfg_blob_t *)CFG_FLASH_ADDR;

    if(blob->magic != CFG_MAGIC || blob->version != CFG_VERSION) {
        cfg_set_default();
        cfg_print(&g_cfg, "FLASH INVALID -> DEFAULT");
        return false;
    }

    cfg_blob_t tmp;
    memcpy(&tmp, blob, sizeof(tmp));

    uint32_t crc_calc = crc32_sw((const uint8_t *)&tmp, sizeof(tmp) - sizeof(tmp.crc32));
    if(crc_calc != tmp.crc32) {
        cfg_set_default();
        cfg_print(&g_cfg, "CRC FAIL -> DEFAULT");
        return false;
    }

    g_cfg = tmp.cfg;
    cfg_print(&g_cfg, "FLASH LOADED");
    return true;
}

bool cfg_save_to_flash(void)
{
    cfg_blob_t blob;
    blob.magic   = CFG_MAGIC;
    blob.version = CFG_VERSION;
    blob.cfg     = g_cfg;
    blob.crc32   = crc32_sw((const uint8_t *)&blob, sizeof(blob) - sizeof(blob.crc32));

    if(!flash_erase_page(CFG_FLASH_ADDR)) {
        printf("[CFG] erase failed\r\n");
        return false;
    }

    if(!flash_program_halfwords(CFG_FLASH_ADDR, (const uint8_t *)&blob, sizeof(blob))) {
        printf("[CFG] program failed\r\n");
        return false;
    }

    printf("[CFG] saved\r\n");
    return true;
}
