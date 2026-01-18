#include "ph_json_uart5_rx.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <main.h>

static UART_HandleTypeDef *s_huart = NULL;

/* RX state */
static uint8_t  s_rx_byte = 0;
static char     s_line[PH_JSON_UART_RX_LINE_MAX];
static volatile uint16_t s_len = 0;
static volatile uint8_t  s_line_ready = 0;

static char s_last_line[PH_JSON_UART_RX_LINE_MAX];

/* ====== Helpers: extract value by key ====== */
static int json_get_float(const char *json, const char *key, float *out)
{
    char pattern[32];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);

    const char *p = strstr(json, pattern);
    if(!p) return 0;

    p += strlen(pattern);
    while(*p == ' ' || *p == '\t') p++;

    char *end = NULL;
    float v = strtof(p, &end);
    if(end == p) return 0;

    *out = v;
    return 1;
}

static int json_get_u32(const char *json, const char *key, uint32_t *out)
{
    char pattern[32];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);

    const char *p = strstr(json, pattern);
    if(!p) return 0;

    p += strlen(pattern);
    while(*p == ' ' || *p == '\t') p++;

    char *end = NULL;
    unsigned long v = strtoul(p, &end, 10);
    if(end == p) return 0;

    *out = (uint32_t)v;
    return 1;
}

int parse_control_json(const char *json, ph_ctrl_cfg_t *out)
{
    ph_ctrl_cfg_t tmp = *out;   // ✅ giữ giá trị cũ (nhất là control_mode)

    int ok = 1;
    ok &= json_get_float(json, "ph_high", &tmp.ph_high);
    ok &= json_get_float(json, "ph_low",  &tmp.ph_low);
    ok &= json_get_u32 (json, "time_off", &tmp.time_off);
    ok &= json_get_u32 (json, "time_on",  &tmp.time_on);
    ok &= json_get_u32 (json, "pump_power", &tmp.pump_power);

    // control_mode: optional (nếu có thì update, không có thì giữ)
    uint32_t m;
    if (json_get_u32(json, "control_mode", &m)) {
        tmp.control_mode = (m ? 1U : 0U);
    }

    if(!ok) return 0;
    *out = tmp;
    return 1;
}


 int parse_mode_json(const char *json, uint32_t *mode_out)
 {
     uint32_t m;
     if(json_get_u32(json, "control_mode", &m)) {
         *mode_out = (m ? 1U : 0U);
         return 1;
     }
     return 0;
 }


/* ====== Public APIs ====== */
void ph_json_uart5_rx_init(UART_HandleTypeDef *huart)
{
    s_huart = huart;
    s_len = 0;
    s_line_ready = 0;
    memset(s_line, 0, sizeof(s_line));
    memset(s_last_line, 0, sizeof(s_last_line));

    /* Start receive 1 byte interrupt */
    HAL_UART_Receive_IT(s_huart, &s_rx_byte, 1);
}

void ph_json_uart5_rx_irq(UART_HandleTypeDef *huart)
{
    if(s_huart == NULL) return;
    if(huart != s_huart) return;   // chỉ xử lý đúng UART đã init

    uint8_t b = s_rx_byte;

    if(b == '\r') {
        // ignore CR
    }
    else if(b == '\n') {
        // end of line
        if(s_len >= PH_JSON_UART_RX_LINE_MAX) s_len = PH_JSON_UART_RX_LINE_MAX - 1;
        s_line[s_len] = '\0';

        // lưu lại line cho debug
        strncpy(s_last_line, s_line, PH_JSON_UART_RX_LINE_MAX - 1);
        s_last_line[PH_JSON_UART_RX_LINE_MAX - 1] = '\0';

        s_line_ready = 1;
        s_len = 0;
    }
    else {
        if(s_len < (PH_JSON_UART_RX_LINE_MAX - 1)) {
            s_line[s_len++] = (char)b;
        } else {
            // overflow -> reset để tránh rác
            s_len = 0;
        }
    }

    // nhận byte tiếp theo
    HAL_UART_Receive_IT(s_huart, &s_rx_byte, 1);
}

bool ph_json_uart5_rx_poll(ph_ctrl_cfg_t *out)
{
    if(!out) return false;
    if(!s_line_ready) return false;

    s_line_ready = 0;

    // parse từ last_line (đã chốt chuỗi)
    if(parse_control_json(s_last_line, out)) {
        return true;
    }

    return false;
}

const char *ph_json_uart5_rx_get_last_line(void)
{
    return s_last_line;
}
