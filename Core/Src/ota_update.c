/*
 * ota_update.c
 *
 * Downloads Intel HEX firmware from GitHub via SIMCOM A7680 HTTP(S), writes it
 * to the OTA staging flash area, verifies size/CRC, and marks it pending for a
 * bootloader to install after reset.
 */
#include "ota_update.h"

#include "cJSON.h"
#include "config.h"
#include "main.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

#if OTA_ENABLE

#define OTA_METADATA_MAGIC 0x4F544131UL /* 'OTA1' */
#define OTA_METADATA_VERSION 0x00010000UL
#define OTA_STATE_PENDING 1U
#define OTA_HTTP_READ_CHUNK 256U
#define OTA_HEX_LINE_MAX 96U

typedef struct __attribute__((packed)) {
  uint32_t magic;
  uint32_t metadata_version;
  uint32_t state;
  uint32_t app_addr;
  uint32_t staging_addr;
  uint32_t size;
  uint32_t crc32;
  char version[16];
  uint32_t header_crc32;
} ota_metadata_t;

extern char rx_data_sim[700];
extern char array_at_command[400];
extern void send_to_simcom_a76xx(char *cmd);

static uint32_t g_download_crc;
static uint32_t g_download_size;

static void simcom_clear_rx(void) {
  memset(rx_data_sim, '\0', sizeof(rx_data_sim));
}

static bool simcom_response_has(const char *text) {
  return strstr(rx_data_sim, text) != NULL;
}

static bool simcom_wait_for(const char *text, uint32_t timeout_ms) {
  uint32_t start_tick = HAL_GetTick();
  while ((HAL_GetTick() - start_tick) < timeout_ms) {
    if (simcom_response_has(text)) {
      return true;
    }
    HAL_Delay(50);
  }
  return false;
}

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, uint32_t len) {
  crc = ~crc;
  for (uint32_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t b = 0; b < 8U; b++) {
      uint32_t mask = (uint32_t)(-(int32_t)(crc & 1U));
      crc = (crc >> 1) ^ (0xEDB88320UL & mask);
    }
  }
  return ~crc;
}

static bool flash_erase_pages(uint32_t start_addr, uint32_t size) {
  FLASH_EraseInitTypeDef erase = {0};
  uint32_t page_error = 0;
  uint32_t page_count = (size + OTA_FLASH_PAGE_SIZE - 1U) / OTA_FLASH_PAGE_SIZE;

  HAL_FLASH_Unlock();
  erase.TypeErase = FLASH_TYPEERASE_PAGES;
  erase.PageAddress = start_addr;
  erase.NbPages = page_count;
  if (HAL_FLASHEx_Erase(&erase, &page_error) != HAL_OK) {
    HAL_FLASH_Lock();
    printf("[OTA] erase fail page=0x%08lX\r\n", (unsigned long)page_error);
    return false;
  }
  HAL_FLASH_Lock();
  return true;
}

static bool flash_program_halfwords(uint32_t addr, const uint8_t *data,
                                    uint32_t len) {
  if (((addr & 1U) != 0U) || ((len & 1U) != 0U)) {
    printf("[OTA] unaligned flash write addr=0x%08lX len=%lu\r\n",
           (unsigned long)addr, (unsigned long)len);
    return false;
  }

  HAL_FLASH_Unlock();
  for (uint32_t i = 0; i < len; i += 2U) {
    uint16_t halfword = (uint16_t)data[i] | ((uint16_t)data[i + 1U] << 8);
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, addr + i, halfword) !=
        HAL_OK) {
      HAL_FLASH_Lock();
      printf("[OTA] program fail addr=0x%08lX\r\n",
             (unsigned long)(addr + i));
      return false;
    }
  }
  HAL_FLASH_Lock();
  return true;
}

static bool ota_save_metadata(const ota_metadata_t *metadata) {
  ota_metadata_t blob = *metadata;
  blob.header_crc32 = 0;
  blob.header_crc32 =
      crc32_update(0, (const uint8_t *)&blob, sizeof(blob) - sizeof(uint32_t));

  if (!flash_erase_pages(OTA_METADATA_ADDR, OTA_FLASH_PAGE_SIZE)) {
    return false;
  }
  return flash_program_halfwords(OTA_METADATA_ADDR, (const uint8_t *)&blob,
                                 sizeof(blob));
}

bool ota_metadata_is_pending(void) {
  const ota_metadata_t *metadata = (const ota_metadata_t *)OTA_METADATA_ADDR;
  ota_metadata_t copy;
  uint32_t saved_crc;

  if (metadata->magic != OTA_METADATA_MAGIC ||
      metadata->metadata_version != OTA_METADATA_VERSION ||
      metadata->state != OTA_STATE_PENDING) {
    return false;
  }

  memcpy(&copy, metadata, sizeof(copy));
  saved_crc = copy.header_crc32;
  copy.header_crc32 = 0;
  return saved_crc ==
         crc32_update(0, (const uint8_t *)&copy,
                      sizeof(copy) - sizeof(copy.header_crc32));
}

static int version_part(const char **cursor) {
  int value = 0;
  while (**cursor >= '0' && **cursor <= '9') {
    value = (value * 10) + (**cursor - '0');
    (*cursor)++;
  }
  if (**cursor == '.') {
    (*cursor)++;
  }
  return value;
}

static int version_compare(const char *left, const char *right) {
  const char *l = left;
  const char *r = right;
  for (uint8_t i = 0; i < 4U; i++) {
    int lv = version_part(&l);
    int rv = version_part(&r);
    if (lv != rv) {
      return lv > rv ? 1 : -1;
    }
  }
  return 0;
}

static uint32_t parse_u32_field(cJSON *json, const char *name,
                                uint32_t default_value) {
  cJSON *item = cJSON_GetObjectItem(json, name);
  if (item == NULL) {
    return default_value;
  }
  if (item->valuestring != NULL) {
    return (uint32_t)strtoul(item->valuestring, NULL, 0);
  }
  return (uint32_t)item->valuedouble;
}

static bool simcom_http_open(const char *url, uint32_t *content_len) {
  int method = -1;
  int status = 0;
  int length = 0;
  char *action;
  bool is_https = strncmp(url, "https://", 8) == 0;

  send_to_simcom_a76xx("AT+HTTPTERM\r\n");
  HAL_Delay(300);

  if (is_https) {
    simcom_clear_rx();
    send_to_simcom_a76xx("AT+CSSLCFG=\"sslversion\",0,4\r\n");
    simcom_wait_for("OK", 2000);

    simcom_clear_rx();
    send_to_simcom_a76xx("AT+CSSLCFG=\"authmode\",0,0\r\n");
    simcom_wait_for("OK", 2000);

    simcom_clear_rx();
    send_to_simcom_a76xx("AT+CSSLCFG=\"enableSNI\",0,1\r\n");
    simcom_wait_for("OK", 2000);
  }

  simcom_clear_rx();
  send_to_simcom_a76xx("AT+HTTPINIT\r\n");
  if (!simcom_wait_for("OK", 3000)) {
    printf("[OTA] HTTPINIT fail\r\n");
    return false;
  }

  simcom_clear_rx();
  send_to_simcom_a76xx("AT+HTTPPARA=\"CID\",1\r\n");
  simcom_wait_for("OK", 1000);

  if (is_https) {
    simcom_clear_rx();
    send_to_simcom_a76xx("AT+HTTPPARA=\"SSLCFG\",0\r\n");
    simcom_wait_for("OK", 1000);
  }

  simcom_clear_rx();
  send_to_simcom_a76xx("AT+HTTPPARA=\"CONNECTTO\",120\r\n");
  simcom_wait_for("OK", 1000);

  simcom_clear_rx();
  send_to_simcom_a76xx("AT+HTTPPARA=\"RECVTO\",120\r\n");
  simcom_wait_for("OK", 1000);

  simcom_clear_rx();
  snprintf(array_at_command, sizeof(array_at_command),
           "AT+HTTPPARA=\"URL\",\"%s\"\r\n", url);
  send_to_simcom_a76xx(array_at_command);
  if (!simcom_wait_for("OK", 3000)) {
    printf("[OTA] HTTP URL fail\r\n");
    return false;
  }

  simcom_clear_rx();
  send_to_simcom_a76xx("AT+HTTPACTION=0\r\n");
  if (!simcom_wait_for("+HTTPACTION:", 60000)) {
    printf("[OTA] HTTPACTION timeout\r\n");
    return false;
  }

  action = strstr(rx_data_sim, "+HTTPACTION:");
  if (action == NULL ||
      sscanf(action, "+HTTPACTION: %d,%d,%d", &method, &status, &length) != 3) {
    printf("[OTA] HTTPACTION parse fail: %s\r\n", rx_data_sim);
    return false;
  }
  if (status < 200 || status >= 300 || length <= 0) {
    printf("[OTA] HTTP status=%d len=%d\r\n", status, length);
    return false;
  }

  *content_len = (uint32_t)length;
  return true;
}

static void simcom_http_close(void) {
  send_to_simcom_a76xx("AT+HTTPTERM\r\n");
  HAL_Delay(300);
}

static bool simcom_http_read(uint32_t offset, uint32_t len, char *out,
                             uint32_t out_cap, uint32_t *out_len) {
  char *header;
  char *body;
  int actual = 0;

  if (len >= out_cap) {
    len = out_cap - 1U;
  }

  simcom_clear_rx();
  snprintf(array_at_command, sizeof(array_at_command), "AT+HTTPREAD=%lu,%lu\r\n",
           (unsigned long)offset, (unsigned long)len);
  send_to_simcom_a76xx(array_at_command);
  if (!simcom_wait_for("+HTTPREAD:", 10000)) {
    printf("[OTA] HTTPREAD timeout\r\n");
    return false;
  }

  header = strstr(rx_data_sim, "+HTTPREAD:");
  if (header == NULL || sscanf(header, "+HTTPREAD: %d", &actual) != 1 ||
      actual < 0) {
    printf("[OTA] HTTPREAD parse fail\r\n");
    return false;
  }

  body = strstr(header, "\r\n");
  if (body == NULL) {
    return false;
  }
  body += 2;
  if ((uint32_t)actual >= out_cap) {
    actual = (int)out_cap - 1;
  }

  memcpy(out, body, (uint32_t)actual);
  out[actual] = '\0';
  *out_len = (uint32_t)actual;
  return true;
}

static bool http_get_small(const char *url, char *out, uint32_t out_cap) {
  uint32_t content_len = 0;
  uint32_t read_len = 0;

  if (!simcom_http_open(url, &content_len)) {
    return false;
  }
  if (content_len >= out_cap) {
    simcom_http_close();
    printf("[OTA] small GET too large=%lu\r\n", (unsigned long)content_len);
    return false;
  }
  if (!simcom_http_read(0, content_len, out, out_cap, &read_len)) {
    simcom_http_close();
    return false;
  }
  simcom_http_close();
  out[read_len] = '\0';
  return true;
}

static int hex_nibble(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  return -1;
}

static bool hex_byte(const char *text, uint8_t *out) {
  int hi = hex_nibble(text[0]);
  int lo = hex_nibble(text[1]);
  if (hi < 0 || lo < 0) {
    return false;
  }
  *out = (uint8_t)((hi << 4) | lo);
  return true;
}

static bool parse_hex_line(const char *line, uint32_t manifest_app_addr,
                           uint32_t *upper_addr, bool *eof_seen) {
  uint8_t count;
  uint8_t type;
  uint8_t checksum = 0;
  uint8_t addr_hi;
  uint8_t addr_lo;
  uint16_t offset;
  uint8_t data[32];

  if (line[0] != ':' || !hex_byte(&line[1], &count) || count > sizeof(data)) {
    return false;
  }
  if (!hex_byte(&line[3], &addr_hi) || !hex_byte(&line[5], &addr_lo) ||
      !hex_byte(&line[7], &type)) {
    return false;
  }
  offset = (uint16_t)(((uint16_t)addr_hi << 8) | addr_lo);

  checksum = count + (uint8_t)(offset >> 8) + (uint8_t)offset + type;
  for (uint8_t i = 0; i < count; i++) {
    if (!hex_byte(&line[9 + (i * 2U)], &data[i])) {
      return false;
    }
    checksum = (uint8_t)(checksum + data[i]);
  }
  {
    uint8_t file_checksum;
    if (!hex_byte(&line[9 + (count * 2U)], &file_checksum)) {
      return false;
    }
    checksum = (uint8_t)(checksum + file_checksum);
  }
  if (checksum != 0U) {
    printf("[OTA] HEX checksum fail\r\n");
    return false;
  }

  if (type == 0x00U) {
    uint32_t absolute_addr = *upper_addr + offset;
    uint32_t staging_addr;

    if (absolute_addr < manifest_app_addr ||
        absolute_addr + count > manifest_app_addr + OTA_APP_MAX_SIZE) {
      printf("[OTA] HEX addr out of app range 0x%08lX\r\n",
             (unsigned long)absolute_addr);
      return false;
    }
    staging_addr = OTA_STAGING_START_ADDR + (absolute_addr - manifest_app_addr);
    if (staging_addr + count >
        OTA_STAGING_START_ADDR + OTA_STAGING_MAX_SIZE) {
      printf("[OTA] staging overflow\r\n");
      return false;
    }
    if (!flash_program_halfwords(staging_addr, data, count)) {
      return false;
    }
    g_download_crc = crc32_update(g_download_crc, data, count);
    g_download_size += count;
  } else if (type == 0x01U) {
    *eof_seen = true;
  } else if (type == 0x04U && count == 2U) {
    *upper_addr = (uint32_t)(((uint16_t)data[0] << 8) | data[1]) << 16;
  }

  return true;
}

static bool process_hex_text(char *text, char *partial, uint32_t partial_cap,
                             uint32_t *partial_len, uint32_t app_addr,
                             uint32_t *upper_addr, bool *eof_seen) {
  char *cursor = text;

  while (*cursor != '\0') {
    char c = *cursor++;
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      partial[*partial_len] = '\0';
      if (*partial_len > 0U) {
        if (!parse_hex_line(partial, app_addr, upper_addr, eof_seen)) {
          return false;
        }
      }
      *partial_len = 0;
      continue;
    }
    if (*partial_len + 1U >= partial_cap) {
      printf("[OTA] HEX line too long\r\n");
      return false;
    }
    partial[*partial_len] = c;
    (*partial_len)++;
  }
  return true;
}

void ota_init(void) {
  if (ota_metadata_is_pending()) {
    printf("[OTA] pending image exists\r\n");
  }
}

bool ota_load_manifest_from_github(ota_manifest_t *manifest) {
  char manifest_json[512];
  const char *manifest_urls[] = {
      OTA_MANIFEST_URL,
#ifdef OTA_MANIFEST_URL_FALLBACK_1
      OTA_MANIFEST_URL_FALLBACK_1,
#endif
#ifdef OTA_MANIFEST_URL_FALLBACK_2
      OTA_MANIFEST_URL_FALLBACK_2,
#endif
  };
  cJSON *json;
  cJSON *version;
  cJSON *hex_url;

  if (manifest == NULL) {
    return false;
  }
  memset(manifest, 0, sizeof(*manifest));

  bool downloaded = false;
  for (uint8_t i = 0; i < (sizeof(manifest_urls) / sizeof(manifest_urls[0]));
       i++) {
    printf("[OTA] manifest url %u\r\n", i);
    if (http_get_small(manifest_urls[i], manifest_json, sizeof(manifest_json))) {
      downloaded = true;
      break;
    }
    printf("[OTA] manifest url %u fail\r\n", i);
  }

  if (!downloaded) {
    printf("[OTA] manifest download fail all urls\r\n");
    return false;
  }

  json = cJSON_Parse(manifest_json);
  if (json == NULL) {
    printf("[OTA] manifest JSON parse fail\r\n");
    return false;
  }

  version = cJSON_GetObjectItem(json, "version");
  hex_url = cJSON_GetObjectItem(json, "hex_url");
  if (version == NULL || version->valuestring == NULL || hex_url == NULL ||
      hex_url->valuestring == NULL) {
    cJSON_Delete(json);
    printf("[OTA] manifest missing version/hex_url\r\n");
    return false;
  }

  snprintf(manifest->version, sizeof(manifest->version), "%s",
           version->valuestring);
  snprintf(manifest->hex_url, sizeof(manifest->hex_url), "%s",
           hex_url->valuestring);
  manifest->app_addr = parse_u32_field(json, "app_addr", OTA_APP_START_ADDR);
  manifest->size = parse_u32_field(json, "size", 0);
  manifest->crc32 = parse_u32_field(json, "crc32", 0);
  cJSON_Delete(json);

  return manifest->size > 0U && manifest->crc32 != 0U &&
         manifest->app_addr == OTA_APP_START_ADDR;
}

bool ota_download_hex_to_staging(const ota_manifest_t *manifest) {
  uint32_t content_len = 0;
  uint32_t offset = 0;
  char chunk[OTA_HTTP_READ_CHUNK + 1U];
  char partial[OTA_HEX_LINE_MAX];
  uint32_t partial_len = 0;
  uint32_t upper_addr = 0;
  bool eof_seen = false;

  if (manifest == NULL || manifest->size > OTA_APP_MAX_SIZE ||
      manifest->size > OTA_STAGING_MAX_SIZE) {
    return false;
  }

  printf("[OTA] erase staging\r\n");
  if (!flash_erase_pages(OTA_STAGING_START_ADDR, OTA_STAGING_MAX_SIZE)) {
    return false;
  }

  g_download_crc = 0;
  g_download_size = 0;
  memset(partial, 0, sizeof(partial));

  if (!simcom_http_open(manifest->hex_url, &content_len)) {
    return false;
  }

  while (offset < content_len && !eof_seen) {
    uint32_t read_len = 0;
    uint32_t request_len = content_len - offset;
    if (request_len > OTA_HTTP_READ_CHUNK) {
      request_len = OTA_HTTP_READ_CHUNK;
    }
    if (!simcom_http_read(offset, request_len, chunk, sizeof(chunk),
                          &read_len)) {
      simcom_http_close();
      return false;
    }
    if (!process_hex_text(chunk, partial, sizeof(partial), &partial_len,
                          manifest->app_addr, &upper_addr, &eof_seen)) {
      simcom_http_close();
      return false;
    }
    offset += read_len;
    IWDG->KR = 0xAAAA;
  }
  simcom_http_close();

  if (!eof_seen && partial_len > 0U) {
    partial[partial_len] = '\0';
    if (!parse_hex_line(partial, manifest->app_addr, &upper_addr, &eof_seen)) {
      return false;
    }
  }

  printf("[OTA] downloaded size=%lu crc=0x%08lX\r\n",
         (unsigned long)g_download_size, (unsigned long)g_download_crc);
  return eof_seen && g_download_size == manifest->size &&
         g_download_crc == manifest->crc32;
}

bool ota_mark_pending(const ota_manifest_t *manifest) {
  ota_metadata_t metadata;

  if (manifest == NULL) {
    return false;
  }
  memset(&metadata, 0, sizeof(metadata));
  metadata.magic = OTA_METADATA_MAGIC;
  metadata.metadata_version = OTA_METADATA_VERSION;
  metadata.state = OTA_STATE_PENDING;
  metadata.app_addr = manifest->app_addr;
  metadata.staging_addr = OTA_STAGING_START_ADDR;
  metadata.size = manifest->size;
  metadata.crc32 = manifest->crc32;
  snprintf(metadata.version, sizeof(metadata.version), "%s",
           manifest->version);

  return ota_save_metadata(&metadata);
}

ota_result_t ota_check_and_download(void) {
  ota_manifest_t manifest;

  printf("[OTA] check GitHub manifest\r\n");
  if (!ota_load_manifest_from_github(&manifest)) {
    return OTA_RESULT_ERROR;
  }

  printf("[OTA] remote=%s current=%s\r\n", manifest.version, VERSION_WBEE);
  if (version_compare(manifest.version, VERSION_WBEE) <= 0) {
    return OTA_RESULT_NO_UPDATE;
  }

  if (!ota_download_hex_to_staging(&manifest)) {
    printf("[OTA] download/verify fail\r\n");
    return OTA_RESULT_ERROR;
  }
  if (!ota_mark_pending(&manifest)) {
    printf("[OTA] mark pending fail\r\n");
    return OTA_RESULT_ERROR;
  }

  printf("[OTA] pending update ready, reset for bootloader install\r\n");
  return OTA_RESULT_OK;
}

#else

void ota_init(void) {}
ota_result_t ota_check_and_download(void) { return OTA_RESULT_NO_UPDATE; }
bool ota_load_manifest_from_github(ota_manifest_t *manifest) {
  (void)manifest;
  return false;
}
bool ota_download_hex_to_staging(const ota_manifest_t *manifest) {
  (void)manifest;
  return false;
}
bool ota_mark_pending(const ota_manifest_t *manifest) {
  (void)manifest;
  return false;
}
bool ota_metadata_is_pending(void) { return false; }

#endif
