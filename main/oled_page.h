#ifndef OLED_PAGE_H_
#define OLED_PAGE_H_

#include <stdint.h>

int oled_page_init(void);
int oled_page_update_co2(uint16_t measurement);
int oled_update_temp(double measurement);
int oled_update_humi(double measurement);
int oled_avs_icon(bool enable);
int oled_wifi_icon(bool enable);
int oled_page_deinit(void);

#endif // OLED_PAGE_H_
