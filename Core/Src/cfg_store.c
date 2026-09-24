#include "cfg_store.h"

#include "config.h"
#include <stdio.h>
#include <string.h>

#define CFG_MAGIC          (0x43464731UL) /* CFG1 */
#define CFG_VERSION_LEGACY (0x00010002UL)
#define CFG_VERSION        (0x00010003UL)

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t version;
    ph_ctrl_cfg_t cfg;
    char serial_number[DEVICE_SERIAL_MAX_LEN];
    uint32_t crc32;
} cfg_blob_t;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t version;
    ph_ctrl_cfg_t cfg;
    uint32_t crc32;
} cfg_blob_legacy_t;

static char g_device_serial[DEVICE_SERIAL_MAX_LEN] = SERIAL_NUMBER;

static uint32_t crc32_sw(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    for(uint32_t i = 0U; i < len; i++) {
        crc ^= data[i];
        for(int b = 0; b < 8; b++) {
            uint32_t mask = -(crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

static void cfg_set_default(void)
{
    g_cfg.ph_high = 8.00f;
    g_cfg.ph_low = 7.00f;
    g_cfg.time_off = 50;
    g_cfg.time_on = 2;
    g_cfg.pump_power = 25;
    g_cfg.control_mode = 0;
}

static void serial_set_default(void)
{
    snprintf(g_device_serial, sizeof(g_device_serial), "%s", SERIAL_NUMBER);
}

const char *device_serial_get(void)
{
    return g_device_serial;
}

bool device_serial_is_valid(const char *serial_number)
{
    size_t length;

    if(serial_number == NULL) {
        return false;
    }
    length = strlen(serial_number);
    if(length < 3U || length >= DEVICE_SERIAL_MAX_LEN) {
        return false;
    }

    for(size_t i = 0U; i < length; i++) {
        char c = serial_number[i];
        if(!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
             c == '-' || c == '_')) {
            return false;
        }
    }
    return true;
}

void cfg_print(const ph_ctrl_cfg_t *cfg, const char *tag)
{
    printf("[%s] ph_high=%.2f ph_low=%.2f time_off=%lu time_on=%lu "
           "pump_power=%lu\r\n",
           tag ? tag : "CFG", cfg->ph_high, cfg->ph_low,
           (unsigned long)cfg->time_off, (unsigned long)cfg->time_on,
           (unsigned long)cfg->pump_power);
}

static bool flash_erase_page(uint32_t page_addr)
{
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t page_error = 0;

    HAL_FLASH_Unlock();
    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = page_addr;
    erase.NbPages = 1;
    if(HAL_FLASHEx_Erase(&erase, &page_error) != HAL_OK) {
        HAL_FLASH_Lock();
        return false;
    }
    HAL_FLASH_Lock();
    return true;
}

static bool flash_program_halfwords(uint32_t addr, const uint8_t *data,
                                    uint32_t len)
{
    if((len % 2U) != 0U) {
        return false;
    }

    HAL_FLASH_Unlock();
    for(uint32_t i = 0U; i < len; i += 2U) {
        uint16_t halfword;
        memcpy(&halfword, data + i, sizeof(halfword));
        if(HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, addr + i,
                             halfword) != HAL_OK) {
            HAL_FLASH_Lock();
            return false;
        }
    }
    HAL_FLASH_Lock();
    return true;
}

static bool cfg_write_blob(const char *serial_number)
{
    cfg_blob_t blob;

    memset(&blob, 0, sizeof(blob));
    blob.magic = CFG_MAGIC;
    blob.version = CFG_VERSION;
    blob.cfg = g_cfg;
    snprintf(blob.serial_number, sizeof(blob.serial_number), "%s",
             serial_number);
    blob.crc32 = crc32_sw((const uint8_t *)&blob,
                          sizeof(blob) - sizeof(blob.crc32));

    if(!flash_erase_page(OTA_CONFIG_ADDR)) {
        printf("[CFG] erase failed\r\n");
        return false;
    }
    if(!flash_program_halfwords(OTA_CONFIG_ADDR, (const uint8_t *)&blob,
                                sizeof(blob))) {
        printf("[CFG] program failed\r\n");
        return false;
    }

    if(memcmp((const void *)OTA_CONFIG_ADDR, &blob, sizeof(blob)) != 0) {
        printf("[CFG] verify failed\r\n");
        return false;
    }
    printf("[CFG] saved\r\n");
    return true;
}

bool cfg_load_from_flash(void)
{
    const cfg_blob_t *blob = (const cfg_blob_t *)OTA_CONFIG_ADDR;

    serial_set_default();

    if(blob->magic == CFG_MAGIC && blob->version == CFG_VERSION_LEGACY) {
        cfg_blob_legacy_t legacy;
        memcpy(&legacy, (const void *)OTA_CONFIG_ADDR, sizeof(legacy));
        uint32_t crc = crc32_sw((const uint8_t *)&legacy,
                                sizeof(legacy) - sizeof(legacy.crc32));
        if(crc == legacy.crc32) {
            g_cfg = legacy.cfg;
            cfg_print(&g_cfg, "LEGACY FLASH LOADED");
            printf("[CFG] serial default=%s\r\n", g_device_serial);
            return true;
        }
    }

    if(blob->magic != CFG_MAGIC || blob->version != CFG_VERSION) {
        cfg_set_default();
        cfg_print(&g_cfg, "FLASH INVALID -> DEFAULT");
        printf("[CFG] serial default=%s\r\n", g_device_serial);
        return false;
    }

    cfg_blob_t tmp;
    memcpy(&tmp, blob, sizeof(tmp));
    uint32_t crc = crc32_sw((const uint8_t *)&tmp,
                            sizeof(tmp) - sizeof(tmp.crc32));
    if(crc != tmp.crc32 || !device_serial_is_valid(tmp.serial_number)) {
        cfg_set_default();
        serial_set_default();
        printf("[CFG] CRC/serial invalid -> default\r\n");
        return false;
    }

    g_cfg = tmp.cfg;
    snprintf(g_device_serial, sizeof(g_device_serial), "%s",
             tmp.serial_number);
    cfg_print(&g_cfg, "FLASH LOADED");
    printf("[CFG] serial=%s\r\n", g_device_serial);
    return true;
}

bool cfg_save_to_flash(void)
{
    return cfg_write_blob(g_device_serial);
}

bool device_serial_save(const char *serial_number)
{
    if(!device_serial_is_valid(serial_number)) {
        return false;
    }
    return cfg_write_blob(serial_number);
}
