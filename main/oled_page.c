#include <inttypes.h>
#include <stdio.h>

#include "oled.h"

#if CONFIG_ANJAY_CLIENT_BOARD_PASCO2

/* +1 for null terminator */
static char co2_meas_txt[OLED_MAX_CHAR_PER_LINE + 1];
static char temp_meas_txt[OLED_MAX_CHAR_PER_LINE + 1];
static char humi_meas_txt[OLED_MAX_CHAR_PER_LINE + 1];

static uint8_t co2_heading_id;
static uint8_t temp_heading_id;
static uint8_t humi_heading_id;
static uint8_t ppm_id;
static uint8_t co2_meas_text_id;
static uint8_t temp_meas_text_id;
static uint8_t humi_meas_text_id;

static uint8_t avs_icon_id;
static uint8_t wifi_icon_id;

static const unsigned char avs_icon[] = { 0x10, 0x10, 0x00, 0xE0, 0xF8, 0x3C,
                                          0x9C, 0x1E, 0x76, 0xF6, 0xC6, 0x0E,
                                          0x7E, 0xFC, 0x7C, 0x98, 0xC0, 0x00,
                                          0x00, 0x0B, 0x19, 0x3E, 0x3F, 0x7E,
                                          0x70, 0x63, 0x6F, 0x6E, 0x78, 0x3B,
                                          0x3C, 0x1F, 0x07, 0x00 };

static const unsigned char wifi_icon[] = { 0x10, 0x10, 0x00, 0x60, 0x30, 0xB0,
                                           0x98, 0xD8, 0xD8, 0xC8, 0xC8, 0xD8,
                                           0xD8, 0x98, 0xB0, 0x30, 0x60, 0x00,
                                           0x00, 0x00, 0x00, 0x01, 0x01, 0x04,
                                           0x06, 0x16, 0x16, 0x06, 0x04, 0x01,
                                           0x01, 0x00, 0x00, 0x00 };

int oled_page_init(void) {
    OLED_createTextField(&co2_heading_id, 42U, 20U, "CO2:", 2U, false);
    OLED_createTextField(&ppm_id, 107U, 55U, "ppm", 1U, false);
    snprintf(co2_meas_txt, OLED_MAX_CHAR_PER_LINE + 1, "---");
    OLED_createTextField(&co2_meas_text_id, 0U, 39U, co2_meas_txt, 3U, false);

    OLED_createTextField(&temp_heading_id, 0U, 0U, "Temp:", 1U, false);
    snprintf(temp_meas_txt, OLED_MAX_CHAR_PER_LINE + 1, "---C");
    OLED_createTextField(&temp_meas_text_id, 0U, 10U, temp_meas_txt, 1U, false);

    OLED_createTextField(&humi_heading_id, 99U, 0U, "RH:", 1U, false);
    snprintf(humi_meas_txt, OLED_MAX_CHAR_PER_LINE + 1, " ---%%");
    OLED_createTextField(&humi_meas_text_id, 99U, 10U, humi_meas_txt, 1U,
                         false);

    OLED_update();

    return 0;
}

int oled_page_update_co2(uint16_t measurement) {
    snprintf(co2_meas_txt, OLED_MAX_CHAR_PER_LINE + 1, "%" PRIu16, measurement);

    OLED_update();

    return 0;
}

int oled_update_temp(double measurement) {
    snprintf(temp_meas_txt, OLED_MAX_CHAR_PER_LINE + 1, "%.1fC", measurement);

    OLED_update();

    return 0;
}

int oled_update_humi(double measurement) {
    snprintf(humi_meas_txt, OLED_MAX_CHAR_PER_LINE + 1, "%.1f%%", measurement);

    OLED_update();

    return 0;
}

int oled_avs_icon(bool enable) {
    static bool enabled = false;
    int ret = 0;

    if (enable && !enabled
            && !(ret = OLED_createImage(&avs_icon_id, 70, 0, avs_icon))) {
        enabled = true;
    } else if (!enable && enabled) {
        OLED_deleteObject(avs_icon_id);
        enabled = false;
    }

    return ret;
}

int oled_wifi_icon(bool enable) {
    static bool enabled = false;
    int ret = 0;

    if (enable && !enabled
            && !(ret = OLED_createImage(&wifi_icon_id, 40, 0, wifi_icon))) {
        enabled = true;
    } else if (!enable && enabled) {
        OLED_deleteObject(wifi_icon_id);
        enabled = false;
    }

    return ret;
}

int oled_page_deinit(void) {
    OLED_deleteObject(co2_heading_id);
    OLED_deleteObject(co2_meas_text_id);
    OLED_deleteObject(ppm_id);
    OLED_deleteObject(humi_heading_id);
    OLED_deleteObject(humi_meas_text_id);
    OLED_deleteObject(temp_heading_id);
    OLED_deleteObject(temp_meas_text_id);

    OLED_deleteObject(avs_icon_id);
    OLED_deleteObject(wifi_icon_id);

    return 0;
}

#endif // CONFIG_ANJAY_CLIENT_BOARD_PASCO2
