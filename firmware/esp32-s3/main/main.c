#include <inttypes.h>
#include <assert.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "usb_stream.h"
#include "font_ascii_16x24.h"

#define AP_PASSWORD "12345678"
#define AP_CHANNEL 6
#define AP_MAX_CONN 4
#define HTTP_PORT 8080
#define CONTROL_HTTP_PORT 8081

#define UVC_WIDTH 640
#define UVC_HEIGHT 480
#define UVC_FPS 15
#define UVC_BUFFER_SIZE (256 * 1024)
#define FRAME_QUEUE_LEN 1

#define LCD_HOST SPI2_HOST
#define LCD_PIXEL_CLOCK_HZ (20 * 1000 * 1000)
#define LCD_H_RES 320
#define LCD_V_RES 240
#define LCD_CMD_BITS 8
#define LCD_PARAM_BITS 8
#define LCD_TRANSFER_TIMEOUT_MS 120
#define LCD_FORCED_REFRESH_MS 1500
#define LCD_MISO_GPIO 14
#define LCD_BACKLIGHT_GPIO 13
#define LCD_SCLK_GPIO 12
#define LCD_MOSI_GPIO 11
#define LCD_DC_GPIO 10
#define LCD_RST_GPIO 9
#define LCD_CS_GPIO 8

#define TFT_LED_GPIO 13
#define KEY1_GPIO 21
#define KEY2_GPIO 38
#define KEY3_GPIO 39
#define LED1_GPIO 5
#define R_VCC_GPIO 4
#define LASER_1_GPIO 40
#define LASER_2_GPIO 41
#define BEEP_GPIO 42

static const char *TAG = "uvc_ap_stream";
static QueueHandle_t s_frame_queue;
static httpd_handle_t s_httpd;
static httpd_handle_t s_control_httpd;
static esp_lcd_panel_handle_t s_lcd_panel;
static SemaphoreHandle_t s_lcd_transfer_done;
static atomic_int s_stream_clients;
static atomic_int s_laser_enabled;
static atomic_int s_result_state;
static atomic_int s_key1_level;
static atomic_int s_key2_level;
static atomic_int s_key3_level;
static atomic_uint s_capture_requests;
static atomic_uint s_capture_pending;
static atomic_int s_realtime_enabled;
static atomic_int s_lcd_enabled;
static atomic_int s_camera_connected;
static atomic_int s_lcd_ready;
static atomic_int s_lcd_dirty;
static atomic_int s_lcd_transfer_timeout_warned;
static atomic_int s_beep_ticks_remaining;
static char s_result_text[48] = "";
static char s_result_confidence[16] = "";
static char s_result_reason[48] = "";

enum {
    RESULT_IDLE = 0,
    RESULT_PROCESSING = 1,
    RESULT_OK = 2,
    RESULT_NG = 3,
};

static void apply_result_state(int state);
static void lcd_render_status_page(void);
static void mark_lcd_dirty(void);

typedef struct {
    uint8_t *data;
    size_t len;
    uint32_t sequence;
    uint32_t width;
    uint32_t height;
} frame_msg_t;

static void free_frame_msg(frame_msg_t *msg)
{
    if (msg && msg->data) {
        free(msg->data);
        msg->data = NULL;
    }
}

static void drop_oldest_frame(void)
{
    frame_msg_t old = {0};
    if (xQueueReceive(s_frame_queue, &old, 0) == pdTRUE) {
        free_frame_msg(&old);
    }
}

static void mark_lcd_dirty(void)
{
    atomic_store(&s_lcd_dirty, 1);
}

static void drain_stream_frames(void)
{
    if (!s_frame_queue) {
        return;
    }

    frame_msg_t old = {0};
    while (xQueueReceive(s_frame_queue, &old, 0) == pdTRUE) {
        free_frame_msg(&old);
    }
}

static uint8_t *alloc_dma_or_psram(size_t size)
{
    uint8_t *buf = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) {
        buf = heap_caps_malloc(size, MALLOC_CAP_8BIT);
    }
    return buf;
}

static const char *result_state_name(int state)
{
    switch (state) {
    case RESULT_PROCESSING:
        return "processing";
    case RESULT_OK:
        return "ok";
    case RESULT_NG:
        return "ng";
    case RESULT_IDLE:
    default:
        return "idle";
    }
}

static void apply_laser_state(bool enabled)
{
    atomic_store(&s_laser_enabled, enabled ? 1 : 0);
    gpio_set_level(LED1_GPIO, enabled ? 1 : 0);
    gpio_set_level(R_VCC_GPIO, enabled ? 1 : 0);
    gpio_set_level(LASER_1_GPIO, enabled ? 1 : 0);
    gpio_set_level(LASER_2_GPIO, enabled ? 1 : 0);
}

static void set_realtime_enabled(bool enabled)
{
    atomic_store(&s_realtime_enabled, enabled ? 1 : 0);
    if (enabled) {
        atomic_store(&s_capture_pending, 0);
    } else {
        drain_stream_frames();
    }
}

static bool request_single_capture(void)
{
    atomic_fetch_add(&s_capture_requests, 1);
    if (atomic_load(&s_realtime_enabled) == 0) {
        atomic_fetch_add(&s_capture_pending, 1);
    }
    apply_result_state(RESULT_PROCESSING);
    return true;
}

static void set_lcd_enabled(bool enabled)
{
    atomic_store(&s_lcd_enabled, enabled ? 1 : 0);
    gpio_set_level(TFT_LED_GPIO,
                   (enabled && atomic_load(&s_lcd_ready) != 0) ? 1 : 0);
    mark_lcd_dirty();
}

static void apply_result_state(int state)
{
    atomic_store(&s_result_state, state);

    switch (state) {
    case RESULT_OK:
        gpio_set_level(BEEP_GPIO, 0);
        break;
    case RESULT_NG:
        atomic_store(&s_beep_ticks_remaining, 3);
        break;
    case RESULT_PROCESSING:
        gpio_set_level(BEEP_GPIO, 0);
        break;
    case RESULT_IDLE:
    default:
        gpio_set_level(BEEP_GPIO, 0);
        break;
    }
    mark_lcd_dirty();
}

static int parse_result_state(const char *text)
{
    if (!text) {
        return RESULT_IDLE;
    }
    if (strstr(text, "ng") || strstr(text, "NG") ||
        strstr(text, "fail") || strstr(text, "bad")) {
        return RESULT_NG;
    }
    if (strstr(text, "processing") || strstr(text, "busy")) {
        return RESULT_PROCESSING;
    }
    if (strstr(text, "ok") || strstr(text, "OK") ||
        strstr(text, "pass") || strstr(text, "good")) {
        return RESULT_OK;
    }
    if (strstr(text, "idle") || strstr(text, "clear")) {
        return RESULT_IDLE;
    }
    return RESULT_IDLE;
}

static void copy_ascii_field(char *dst, size_t dst_size, const char *src)
{
    if (!dst || dst_size == 0) {
        return;
    }
    size_t out = 0;
    if (src) {
        for (size_t i = 0; src[i] != 0 && out + 1 < dst_size; ++i) {
            unsigned char ch = (unsigned char)src[i];
            dst[out++] = (ch >= 32 && ch <= 126) ? (char)ch : '?';
        }
    }
    dst[out] = 0;
}

static void url_decode_inplace(char *text)
{
    char *src = text;
    char *dst = text;
    while (*src) {
        if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else if (*src == '%' &&
                   ((src[1] >= '0' && src[1] <= '9') || (src[1] >= 'A' && src[1] <= 'F') || (src[1] >= 'a' && src[1] <= 'f')) &&
                   ((src[2] >= '0' && src[2] <= '9') || (src[2] >= 'A' && src[2] <= 'F') || (src[2] >= 'a' && src[2] <= 'f'))) {
            char hex[3] = {src[1], src[2], 0};
            *dst++ = (char)strtol(hex, NULL, 16);
            src += 3;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = 0;
}

static uint16_t *s_lcd_page_buffer;
static uint16_t *s_lcd_transfer_buffer;

static bool lcd_color_transfer_done_cb(esp_lcd_panel_io_handle_t panel_io,
                                       esp_lcd_panel_io_event_data_t *edata,
                                       void *user_ctx)
{
    (void)panel_io;
    (void)edata;
    (void)user_ctx;
    if (!s_lcd_transfer_done) {
        return false;
    }
    BaseType_t high_task_wakeup = pdFALSE;
    xSemaphoreGiveFromISR(s_lcd_transfer_done, &high_task_wakeup);
    return high_task_wakeup == pdTRUE;
}

static void lcd_buffer_fill(uint16_t color)
{
    if (!s_lcd_page_buffer) {
        return;
    }
    for (int i = 0; i < LCD_H_RES * LCD_V_RES; ++i) {
        s_lcd_page_buffer[i] = color;
    }
}

static void lcd_buffer_draw_char(int x, int y, char ch, uint16_t color)
{
    if (ch >= 'a' && ch <= 'z') {
        ch = (char)(ch - 'a' + 'A');
    }
    if (ch < 32 || ch > 126) {
        ch = '?';
    }
    const uint16_t *glyph = s_font16x24[ch - 32];
    for (int row = 0; row < LCD_FONT_H; ++row) {
        for (int col = 0; col < LCD_FONT_W; ++col) {
            int px = x + col;
            int py = y + row;
            if (px < 0 || px >= LCD_H_RES || py < 0 || py >= LCD_V_RES) {
                continue;
            }
            if (glyph[row] & (1 << (LCD_FONT_W - 1 - col))) {
                s_lcd_page_buffer[py * LCD_H_RES + px] = color;
            }
        }
    }
}

static void lcd_buffer_draw_text(int x, int y, const char *text, uint16_t color)
{
    if (!text) {
        return;
    }
    for (size_t i = 0; text[i] != 0; ++i) {
        lcd_buffer_draw_char(x + (int)i * LCD_FONT_W, y, text[i], color);
    }
}

static void lcd_buffer_draw_char_small(int x, int y, char ch, uint16_t color)
{
    if (ch >= 'a' && ch <= 'z') {
        ch = (char)(ch - 'a' + 'A');
    }
    if (ch < 32 || ch > 126) {
        ch = '?';
    }

    const uint16_t *glyph = s_font16x24[ch - 32];
    for (int small_row = 0; small_row < LCD_FONT_H / 2; ++small_row) {
        for (int small_col = 0; small_col < LCD_FONT_W / 2; ++small_col) {
            bool set = false;
            for (int row_offset = 0; row_offset < 2 && !set; ++row_offset) {
                int glyph_row = small_row * 2 + row_offset;
                for (int col_offset = 0; col_offset < 2; ++col_offset) {
                    int glyph_col = small_col * 2 + col_offset;
                    if (glyph[glyph_row] & (1 << (LCD_FONT_W - 1 - glyph_col))) {
                        set = true;
                        break;
                    }
                }
            }
            if (set) {
                int px = x + small_col;
                int py = y + small_row;
                if (px >= 0 && px < LCD_H_RES && py >= 0 && py < LCD_V_RES) {
                    s_lcd_page_buffer[py * LCD_H_RES + px] = color;
                }
            }
        }
    }
}

static void lcd_buffer_draw_text_small(int x, int y, const char *text, uint16_t color)
{
    if (!text) {
        return;
    }
    for (size_t i = 0; text[i] != 0; ++i) {
        lcd_buffer_draw_char_small(x + (int)i * (LCD_FONT_W / 2), y, text[i], color);
    }
}

// Dedicated compact 5x7 footer glyphs. Strokes are two pixels thick while the
// character advance stays narrow, matching the weight of the main status font
// without making the footer span most of the screen.
static const uint8_t *lcd_footer_glyph(char ch)
{
    static const uint8_t blank[7] = {0, 0, 0, 0, 0, 0, 0};
    static const uint8_t colon[7] = {0, 0x04, 0x04, 0, 0x04, 0x04, 0};
    static const uint8_t one[7]   = {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E};
    static const uint8_t two[7]   = {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F};
    static const uint8_t three[7] = {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E};
    static const uint8_t c[7] = {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E};
    static const uint8_t e[7] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F};
    static const uint8_t g[7] = {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0E};
    static const uint8_t i[7] = {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1F};
    static const uint8_t k[7] = {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11};
    static const uint8_t l[7] = {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F};
    static const uint8_t o[7] = {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
    static const uint8_t p[7] = {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10};
    static const uint8_t r[7] = {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11};
    static const uint8_t t[7] = {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
    static const uint8_t v[7] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04};
    static const uint8_t w[7] = {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A};

    switch (ch) {
    case ' ': return blank;
    case ':': return colon;
    case '1': return one;
    case '2': return two;
    case '3': return three;
    case 'C': return c;
    case 'E': return e;
    case 'G': return g;
    case 'I': return i;
    case 'K': return k;
    case 'L': return l;
    case 'O': return o;
    case 'P': return p;
    case 'R': return r;
    case 'T': return t;
    case 'V': return v;
    case 'W': return w;
    default: return blank;
    }
}

static void lcd_buffer_draw_char_footer(int x, int y, char ch, uint16_t color)
{
    if (ch >= 'a' && ch <= 'z') {
        ch = (char)(ch - 'a' + 'A');
    }
    const uint8_t *glyph = lcd_footer_glyph(ch);
    for (int row = 0; row < 7; ++row) {
        for (int col = 0; col < 5; ++col) {
            if (glyph[row] & (1 << (4 - col))) {
                for (int dy = 0; dy < 2; ++dy) {
                    for (int dx = 0; dx < 2; ++dx) {
                        int px = x + col + dx;
                        int py = y + row * 2 + dy;
                        if (px >= 0 && px < LCD_H_RES && py >= 0 && py < LCD_V_RES) {
                            s_lcd_page_buffer[py * LCD_H_RES + px] = color;
                        }
                    }
                }
            }
        }
    }
}

static void lcd_buffer_draw_text_footer(int x, int y, const char *text, uint16_t color)
{
    if (!text) {
        return;
    }
    for (size_t i = 0; text[i] != 0; ++i) {
        lcd_buffer_draw_char_footer(x + (int)i * 7, y, text[i], color);
    }
}

static void lcd_buffer_draw_hline(int y, uint16_t color)
{
    if (!s_lcd_page_buffer || y < 0 || y >= LCD_V_RES) {
        return;
    }
    for (int x = 0; x < LCD_H_RES; ++x) {
        s_lcd_page_buffer[y * LCD_H_RES + x] = color;
    }
}

static void lcd_flush_page(void)
{
    if (!s_lcd_page_buffer || !s_lcd_transfer_buffer || !s_lcd_panel ||
        !s_lcd_transfer_done) {
        return;
    }
    // Keep page composition atomic in RAM, but transfer in stripes so each SPI
    // transaction stays within the configured max_transfer_sz budget.
    const int stripe_h = 20;
    for (int y = 0; y < LCD_V_RES; y += stripe_h) {
        int h = (y + stripe_h <= LCD_V_RES) ? stripe_h : (LCD_V_RES - y);
        memcpy(s_lcd_transfer_buffer,
               s_lcd_page_buffer + y * LCD_H_RES,
               LCD_H_RES * h * sizeof(uint16_t));
        xSemaphoreTake(s_lcd_transfer_done, 0);
        esp_err_t err = esp_lcd_panel_draw_bitmap(s_lcd_panel, 0, y,
                                                  LCD_H_RES, y + h,
                                                  s_lcd_transfer_buffer);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "LCD draw failed at y=%d: %s", y, esp_err_to_name(err));
            break;
        }
        if (xSemaphoreTake(s_lcd_transfer_done,
                           pdMS_TO_TICKS(LCD_TRANSFER_TIMEOUT_MS)) != pdTRUE) {
            if (atomic_exchange(&s_lcd_transfer_timeout_warned, 1) == 0) {
                ESP_LOGW(TAG, "LCD transfer timeout at y=%d", y);
            }
            break;
        }
        atomic_store(&s_lcd_transfer_timeout_warned, 0);
    }
}

static void lcd_render_status_page(void)
{
    if (atomic_load(&s_lcd_enabled) == 0 || !s_lcd_panel ||
        atomic_load(&s_lcd_ready) == 0) {
        return;
    }

    lcd_buffer_fill(0x0000);
    char header[48];
    snprintf(header, sizeof(header), "OCR SCANNER WIFI:OK PC:%s BAT:--",
             atomic_load(&s_stream_clients) > 0 ? "ON" : "OFF");
    lcd_buffer_draw_text_small(8, 6, header, 0xFFFF);
    lcd_buffer_draw_hline(24, 0x7BEF);

    if (atomic_load(&s_camera_connected) == 0) {
        lcd_buffer_draw_text(18, 52, "CAMERA ERROR", 0xFFFF);
        lcd_buffer_draw_text(18, 96, "CHECK USB CAM", 0xFFFF);
    } else {
        int state = atomic_load(&s_result_state);
        switch (state) {
        case RESULT_PROCESSING:
            lcd_buffer_draw_text(18, 52, "PROCESSING", 0xFFFF);
            lcd_buffer_draw_text(18, 96, "PLEASE WAIT", 0xFFFF);
            break;
        case RESULT_OK:
            lcd_buffer_draw_text(18, 40, "SUCCESS", 0xFFFF);
            lcd_buffer_draw_text(18, 78, s_result_text[0] ? s_result_text : "-", 0xFFFF);
            lcd_buffer_draw_text(18, 122, "CONF:", 0xFFFF);
            lcd_buffer_draw_text(108, 122, s_result_confidence[0] ? s_result_confidence : "-", 0xFFFF);
            break;
        case RESULT_NG:
            lcd_buffer_draw_text(18, 52, "FAILED", 0xFFFF);
            lcd_buffer_draw_text(18, 96, s_result_reason[0] ? s_result_reason : "NO VALID TEXT", 0xFFFF);
            break;
        case RESULT_IDLE:
        default:
            lcd_buffer_draw_text(18, 52, "READY", 0xFFFF);
            lcd_buffer_draw_text(18, 96, "WAITING OCR", 0xFFFF);
            break;
        }
    }

    // Keep power separate on the left. The K3/K2/K1 group is right-aligned to
    // match the physical buttons, with K1 at the far right.
    lcd_buffer_draw_hline(209, 0x7BEF);
    lcd_buffer_draw_text_footer(26, 216, "PWR", 0xFFFF);
    lcd_buffer_draw_text_footer(159, 216, "K3:LGT", 0xFFFF);
    lcd_buffer_draw_text_footer(211, 216, "K2:OCR", 0xFFFF);
    lcd_buffer_draw_text_footer(263, 216, "K1:VIEW", 0xFFFF);
    lcd_flush_page();
}

static void camera_frame_cb(uvc_frame_t *frame, void *ptr)
{
    (void)ptr;

    if (frame->frame_format != UVC_FRAME_FORMAT_MJPEG || frame->data_bytes == 0) {
        ESP_LOGW(TAG, "unsupported frame format=%d bytes=%u",
                 frame->frame_format, (unsigned)frame->data_bytes);
        return;
    }

    bool should_send_to_pc = false;
    if (atomic_load(&s_stream_clients) > 0) {
        if (atomic_load(&s_realtime_enabled) != 0) {
            should_send_to_pc = true;
        } else {
            unsigned int pending = atomic_load(&s_capture_pending);
            while (pending > 0) {
                if (atomic_compare_exchange_weak(&s_capture_pending, &pending, pending - 1)) {
                    should_send_to_pc = true;
                    break;
                }
            }
        }
    }

    if (should_send_to_pc) {
        frame_msg_t msg = {
            .data = heap_caps_malloc(frame->data_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT),
            .len = frame->data_bytes,
            .sequence = frame->sequence,
            .width = frame->width,
            .height = frame->height,
        };
        if (!msg.data) {
            msg.data = heap_caps_malloc(frame->data_bytes, MALLOC_CAP_8BIT);
        }
        if (!msg.data) {
            ESP_LOGW(TAG, "drop stream frame %" PRIu32 ": no memory", frame->sequence);
        } else {
            memcpy(msg.data, frame->data, frame->data_bytes);
            if (xQueueSend(s_frame_queue, &msg, 0) != pdTRUE) {
                drop_oldest_frame();
                if (xQueueSend(s_frame_queue, &msg, 0) != pdTRUE) {
                    free_frame_msg(&msg);
                }
            }
        }
    }

}

static void stream_state_changed_cb(usb_stream_state_t event, void *arg)
{
    (void)arg;

    switch (event) {
    case STREAM_CONNECTED: {
        atomic_store(&s_camera_connected, 1);
        mark_lcd_dirty();
        ESP_LOGI(TAG, "USB camera connected");
        size_t frame_size = 0;
        size_t frame_index = 0;
        uvc_frame_size_list_get(NULL, &frame_size, &frame_index);
        if (frame_size) {
            uvc_frame_size_t *frames = calloc(frame_size, sizeof(uvc_frame_size_t));
            if (frames) {
                uvc_frame_size_list_get(frames, NULL, NULL);
                for (size_t i = 0; i < frame_size; ++i) {
                    ESP_LOGI(TAG, "camera frame[%u]=%ux%u", (unsigned)i,
                             frames[i].width, frames[i].height);
                }
                free(frames);
            }
        }
        break;
    }
    case STREAM_DISCONNECTED:
        atomic_store(&s_camera_connected, 0);
        mark_lcd_dirty();
        ESP_LOGW(TAG, "USB camera disconnected");
        break;
    default:
        ESP_LOGW(TAG, "USB stream event=%d", event);
        break;
    }
}

static esp_err_t stream_handler(httpd_req_t *req)
{
    static const char *boundary = "esp32s3frame";
    char part_header[160];
    frame_msg_t msg = {0};

    atomic_fetch_add(&s_stream_clients, 1);
    mark_lcd_dirty();

    httpd_resp_set_type(req, "multipart/x-mixed-replace; boundary=esp32s3frame");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");

    ESP_LOGI(TAG, "stream client connected");
    while (true) {
        if (xQueueReceive(s_frame_queue, &msg, pdMS_TO_TICKS(5000)) != pdTRUE) {
            if (httpd_resp_send_chunk(req, "", 0) != ESP_OK) {
                break;
            }
            continue;
        }

        int header_len = snprintf(part_header, sizeof(part_header),
                                  "\r\n--%s\r\n"
                                  "Content-Type: image/jpeg\r\n"
                                  "Content-Length: %u\r\n"
                                  "X-Width: %" PRIu32 "\r\n"
                                  "X-Height: %" PRIu32 "\r\n"
                                  "X-Sequence: %" PRIu32 "\r\n\r\n",
                                  boundary, (unsigned)msg.len,
                                  msg.width, msg.height, msg.sequence);

        esp_err_t err = httpd_resp_send_chunk(req, part_header, header_len);
        if (err == ESP_OK) {
            err = httpd_resp_send_chunk(req, (const char *)msg.data, msg.len);
        }
        free_frame_msg(&msg);

        if (err != ESP_OK) {
            break;
        }
    }

    atomic_fetch_sub(&s_stream_clients, 1);
    mark_lcd_dirty();
    ESP_LOGI(TAG, "stream client disconnected");
    return ESP_OK;
}

static esp_err_t status_handler(httpd_req_t *req)
{
    char body[512];
    int len = snprintf(body, sizeof(body),
                       "{\"status\":\"ok\",\"stream\":\"http://192.168.4.1:%d/stream\","
                       "\"clients\":%d,\"laser\":%d,\"result\":\"%s\","
                       "\"key1\":%d,\"key2\":%d,\"key3\":%d,"
                       "\"realtime\":%d,\"capture_requests\":%u,"
                       "\"capture_pending\":%u}\n",
                       HTTP_PORT, atomic_load(&s_stream_clients),
                       atomic_load(&s_laser_enabled),
                       result_state_name(atomic_load(&s_result_state)),
                       atomic_load(&s_key1_level),
                       atomic_load(&s_key2_level),
                       atomic_load(&s_key3_level),
                       atomic_load(&s_realtime_enabled),
                       atomic_load(&s_capture_requests),
                       atomic_load(&s_capture_pending));
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, body, len);
}

static esp_err_t result_handler(httpd_req_t *req)
{
    char body[256] = {0};
    int to_read = req->content_len;
    if (to_read >= (int)sizeof(body)) {
        to_read = sizeof(body) - 1;
    }

    int received = 0;
    while (received < to_read) {
        int ret = httpd_req_recv(req, body + received, to_read - received);
        if (ret <= 0) {
            return ESP_FAIL;
        }
        received += ret;
    }
    body[received] = 0;

    char state_value[24] = {0};
    char text_value[48] = {0};
    char confidence_value[16] = {0};
    char reason_value[48] = {0};

    if (httpd_query_key_value(body, "state", state_value, sizeof(state_value)) == ESP_OK) {
        url_decode_inplace(state_value);
    } else {
        copy_ascii_field(state_value, sizeof(state_value), body);
    }
    if (httpd_query_key_value(body, "text", text_value, sizeof(text_value)) == ESP_OK) {
        url_decode_inplace(text_value);
    }
    if (httpd_query_key_value(body, "confidence", confidence_value, sizeof(confidence_value)) == ESP_OK) {
        url_decode_inplace(confidence_value);
    }
    if (httpd_query_key_value(body, "reason", reason_value, sizeof(reason_value)) == ESP_OK) {
        url_decode_inplace(reason_value);
    }

    int state = parse_result_state(state_value);
    copy_ascii_field(s_result_text, sizeof(s_result_text), text_value);
    copy_ascii_field(s_result_confidence, sizeof(s_result_confidence), confidence_value);
    copy_ascii_field(s_result_reason, sizeof(s_result_reason), reason_value);
    apply_result_state(state);
    ESP_LOGI(TAG, "PC result: %s text=%s confidence=%s reason=%s",
             result_state_name(state), s_result_text, s_result_confidence, s_result_reason);

    char resp[80];
    int len = snprintf(resp, sizeof(resp), "{\"result\":\"%s\"}\n", result_state_name(state));
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, resp, len);
}

static esp_err_t control_handler(httpd_req_t *req)
{
    char query[128] = {0};
    char value[32] = {0};

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        if (httpd_query_key_value(query, "laser", value, sizeof(value)) == ESP_OK) {
            apply_laser_state(atoi(value) != 0);
        }
        if (httpd_query_key_value(query, "realtime", value, sizeof(value)) == ESP_OK) {
            int enabled = atoi(value) != 0;
            set_realtime_enabled(enabled);
            ESP_LOGI(TAG, "PC control realtime=%d", enabled);
        }
        if (httpd_query_key_value(query, "capture", value, sizeof(value)) == ESP_OK && atoi(value) != 0) {
            request_single_capture();
            ESP_LOGI(TAG, "PC OCR trigger request=%u pending=%u realtime=%d",
                     atomic_load(&s_capture_requests),
                     atomic_load(&s_capture_pending),
                     atomic_load(&s_realtime_enabled));
        }
        if (httpd_query_key_value(query, "result", value, sizeof(value)) == ESP_OK) {
            apply_result_state(parse_result_state(value));
        }
        if (httpd_query_key_value(query, "beep", value, sizeof(value)) == ESP_OK && atoi(value) != 0) {
            atomic_store(&s_beep_ticks_remaining, 5);
        }
    }

    char resp[120];
    int len = snprintf(resp, sizeof(resp),
                       "{\"laser\":%d,\"realtime\":%d,\"capture_pending\":%u,\"result\":\"%s\"}\n",
                       atomic_load(&s_laser_enabled),
                       atomic_load(&s_realtime_enabled),
                       atomic_load(&s_capture_pending),
                       result_state_name(atomic_load(&s_result_state)));
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, resp, len);
}

static void start_http_server(void)
{
    httpd_config_t stream_config = HTTPD_DEFAULT_CONFIG();
    stream_config.server_port = HTTP_PORT;
    stream_config.ctrl_port = HTTP_PORT + 10;
    stream_config.stack_size = 8192;

    httpd_config_t control_config = HTTPD_DEFAULT_CONFIG();
    control_config.server_port = CONTROL_HTTP_PORT;
    control_config.ctrl_port = CONTROL_HTTP_PORT + 10;
    control_config.stack_size = 8192;

    ESP_ERROR_CHECK(httpd_start(&s_httpd, &stream_config));
    ESP_ERROR_CHECK(httpd_start(&s_control_httpd, &control_config));

    httpd_uri_t stream_uri = {
        .uri = "/stream",
        .method = HTTP_GET,
        .handler = stream_handler,
    };
    httpd_uri_t status_uri = {
        .uri = "/status",
        .method = HTTP_GET,
        .handler = status_handler,
    };
    httpd_uri_t result_uri = {
        .uri = "/result",
        .method = HTTP_POST,
        .handler = result_handler,
    };
    httpd_uri_t control_uri = {
        .uri = "/control",
        .method = HTTP_GET,
        .handler = control_handler,
    };

    ESP_ERROR_CHECK(httpd_register_uri_handler(s_httpd, &stream_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_control_httpd, &status_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_control_httpd, &result_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_control_httpd, &control_uri));
    ESP_LOGI(TAG, "HTTP stream ready: http://192.168.4.1:%d/stream", HTTP_PORT);
    ESP_LOGI(TAG, "HTTP control ready: http://192.168.4.1:%d", CONTROL_HTTP_PORT);
}

static void start_lcd_panel(void)
{
    s_lcd_transfer_done = xSemaphoreCreateBinary();
    assert(s_lcd_transfer_done);

    spi_bus_config_t buscfg = {
        .sclk_io_num = LCD_SCLK_GPIO,
        .mosi_io_num = LCD_MOSI_GPIO,
        .miso_io_num = LCD_MISO_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * 40 * sizeof(uint16_t) + 8,
    };
    esp_err_t ret = spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(ret);
    }

    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = LCD_DC_GPIO,
        .cs_gpio_num = LCD_CS_GPIO,
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = LCD_CMD_BITS,
        .lcd_param_bits = LCD_PARAM_BITS,
        .spi_mode = 0,
        .trans_queue_depth = 1,
        .on_color_trans_done = lcd_color_transfer_done_cb,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = -1,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &s_lcd_panel));
    // Keep the panel in reset from early boot, then release it only after the
    // rail has had time to settle. This is more reliable than asking the panel
    // driver to pulse reset after a fast power cycle.
    vTaskDelay(pdMS_TO_TICKS(500));
    gpio_set_level(LCD_RST_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(120));
    gpio_set_level(LCD_RST_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(180));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_lcd_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(s_lcd_panel, true));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(s_lcd_panel, true));
    // The board is mounted 180 degrees relative to the LCD controller's default
    // orientation. Camera preview used to compensate in software; status pages
    // should instead correct the panel orientation once here.
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(s_lcd_panel, true, true));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_lcd_panel, true));

    s_lcd_page_buffer = heap_caps_malloc(LCD_H_RES * LCD_V_RES * sizeof(uint16_t),
                                         MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_lcd_page_buffer) {
        s_lcd_page_buffer = heap_caps_malloc(LCD_H_RES * LCD_V_RES * sizeof(uint16_t), MALLOC_CAP_8BIT);
    }
    assert(s_lcd_page_buffer);
    s_lcd_transfer_buffer = heap_caps_malloc(LCD_H_RES * 20 * sizeof(uint16_t),
                                             MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    assert(s_lcd_transfer_buffer);

    atomic_store(&s_lcd_ready, 1);
    ESP_LOGI(TAG, "LCD status display ready on ST7789 %dx%d", LCD_H_RES, LCD_V_RES);
    lcd_render_status_page();
    mark_lcd_dirty();
    gpio_set_level(TFT_LED_GPIO, atomic_load(&s_lcd_enabled) != 0 ? 1 : 0);
}

static void lcd_service_task(void *arg)
{
    (void)arg;
    // Do not let LCD bring-up block the rest of the device. This is especially
    // important after a fast power cycle, when the panel may need longer to settle.
    vTaskDelay(pdMS_TO_TICKS(700));
    start_lcd_panel();

    TickType_t last_forced_refresh = 0;
    while (true) {
        TickType_t now = xTaskGetTickCount();
        bool force_refresh = (now - last_forced_refresh) >= pdMS_TO_TICKS(LCD_FORCED_REFRESH_MS);
        bool dirty = atomic_exchange(&s_lcd_dirty, 0) != 0;
        if (atomic_load(&s_lcd_enabled) != 0 && (dirty || force_refresh)) {
            lcd_render_status_page();
            last_forced_refresh = xTaskGetTickCount();
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void start_wifi_ap(void)
{
    uint8_t mac[6];
    char ssid[32];

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP));
    snprintf(ssid, sizeof(ssid), "PipeCam-%02X%02X", mac[4], mac[5]);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {0};
    strlcpy((char *)wifi_config.ap.ssid, ssid, sizeof(wifi_config.ap.ssid));
    wifi_config.ap.ssid_len = strlen(ssid);
    strlcpy((char *)wifi_config.ap.password, AP_PASSWORD, sizeof(wifi_config.ap.password));
    wifi_config.ap.channel = AP_CHANNEL;
    wifi_config.ap.max_connection = AP_MAX_CONN;
    wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    wifi_config.ap.pmf_cfg.required = false;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi AP started");
    ESP_LOGI(TAG, "SSID: %s", ssid);
    ESP_LOGI(TAG, "Password: %s", AP_PASSWORD);
    ESP_LOGI(TAG, "Board IP: 192.168.4.1");
}

static void start_uvc_camera(void)
{
    uint8_t *xfer_buffer_a = alloc_dma_or_psram(UVC_BUFFER_SIZE);
    uint8_t *xfer_buffer_b = alloc_dma_or_psram(UVC_BUFFER_SIZE);
    uint8_t *frame_buffer = alloc_dma_or_psram(UVC_BUFFER_SIZE);
    assert(xfer_buffer_a && xfer_buffer_b && frame_buffer);

    uvc_config_t uvc_config = {
        .frame_width = UVC_WIDTH,
        .frame_height = UVC_HEIGHT,
        .frame_interval = FPS2INTERVAL(UVC_FPS),
        .xfer_buffer_size = UVC_BUFFER_SIZE,
        .xfer_buffer_a = xfer_buffer_a,
        .xfer_buffer_b = xfer_buffer_b,
        .frame_buffer_size = UVC_BUFFER_SIZE,
        .frame_buffer = frame_buffer,
        .frame_cb = camera_frame_cb,
        .frame_cb_arg = NULL,
    };

    ESP_ERROR_CHECK(uvc_streaming_config(&uvc_config));
    ESP_ERROR_CHECK(usb_streaming_state_register(stream_state_changed_cb, NULL));
    ESP_ERROR_CHECK(usb_streaming_start());
    ESP_LOGI(TAG, "waiting for USB camera...");
    ESP_ERROR_CHECK(usb_streaming_connect_wait(portMAX_DELAY));
}

static void hardware_task(void *arg)
{
    (void)arg;
    int last_key1 = 1;
    int last_key2 = 1;
    int last_key3 = 1;
    uint32_t processing_tick = 0;

    while (true) {
        int key1 = gpio_get_level(KEY1_GPIO);
        int key2 = gpio_get_level(KEY2_GPIO);
        int key3 = gpio_get_level(KEY3_GPIO);

        atomic_store(&s_key1_level, key1);
        atomic_store(&s_key2_level, key2);
        atomic_store(&s_key3_level, key3);

        if (last_key1 == 1 && key1 == 0) {
            bool enabled = atomic_load(&s_lcd_enabled) == 0;
            set_lcd_enabled(enabled);
            ESP_LOGI(TAG, "KEY1 toggle view=%d", enabled ? 1 : 0);
        }
        if (last_key2 == 1 && key2 == 0) {
            request_single_capture();
            ESP_LOGI(TAG, "KEY2 OCR trigger request=%u pending=%u realtime=%d",
                     atomic_load(&s_capture_requests),
                     atomic_load(&s_capture_pending),
                     atomic_load(&s_realtime_enabled));
        }
        if (last_key3 == 1 && key3 == 0) {
            apply_laser_state(atomic_load(&s_laser_enabled) == 0);
            ESP_LOGI(TAG, "KEY3 toggle light=%d", atomic_load(&s_laser_enabled));
        }

        last_key1 = key1;
        last_key2 = key2;
        last_key3 = key3;

        if (atomic_load(&s_result_state) == RESULT_PROCESSING) {
            processing_tick++;
        }

        int beep_ticks = atomic_load(&s_beep_ticks_remaining);
        if (beep_ticks > 0) {
            gpio_set_level(BEEP_GPIO, 1);
            atomic_store(&s_beep_ticks_remaining, beep_ticks - 1);
        } else {
            gpio_set_level(BEEP_GPIO, 0);
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

static void init_hardware_io(void)
{
    uint64_t output_mask = (1ULL << TFT_LED_GPIO) | (1ULL << LCD_RST_GPIO) |
                           (1ULL << LED1_GPIO) |
                           (1ULL << R_VCC_GPIO) | (1ULL << LASER_1_GPIO) |
                           (1ULL << LASER_2_GPIO) | (1ULL << BEEP_GPIO);
    gpio_config_t out_conf = {
        .pin_bit_mask = output_mask,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&out_conf);

    uint64_t input_mask = (1ULL << KEY1_GPIO) | (1ULL << KEY2_GPIO) | (1ULL << KEY3_GPIO);
    gpio_config_t in_conf = {
        .pin_bit_mask = input_mask,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&in_conf);

    set_realtime_enabled(true);
    set_lcd_enabled(true);
    gpio_set_level(LCD_RST_GPIO, 0);
    apply_laser_state(false);
    apply_result_state(RESULT_IDLE);
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    init_hardware_io();
    xTaskCreate(hardware_task, "hardware_task", 3072, NULL, 5, NULL);

    s_frame_queue = xQueueCreate(FRAME_QUEUE_LEN, sizeof(frame_msg_t));
    assert(s_frame_queue);

    start_wifi_ap();
    start_http_server();
    xTaskCreate(lcd_service_task, "lcd_service_task", 4096, NULL, 4, NULL);
    start_uvc_camera();
}
