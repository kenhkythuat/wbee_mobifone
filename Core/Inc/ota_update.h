/*
 * ota_update.h
 *
 * App-side OTA downloader for SIMCOM A7680 + GitLab raw files.
 */
#ifndef INC_OTA_UPDATE_H_
#define INC_OTA_UPDATE_H_

#include "stdbool.h"
#include "stdint.h"

typedef enum {
  OTA_RESULT_OK = 0,
  OTA_RESULT_NO_UPDATE,
  OTA_RESULT_ERROR
} ota_result_t;

typedef struct {
  char version[16];
  char hex_url[256];
  uint32_t app_addr;
  uint32_t size;
  uint32_t crc32;
} ota_manifest_t;

void ota_init(void);
ota_result_t ota_check_and_download(void);
bool ota_load_manifest_from_gitlab(ota_manifest_t *manifest);
bool ota_download_hex_to_staging(const ota_manifest_t *manifest);
bool ota_mark_pending(const ota_manifest_t *manifest);
bool ota_metadata_is_pending(void);

#endif /* INC_OTA_UPDATE_H_ */
