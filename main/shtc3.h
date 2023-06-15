#ifndef _SHTC3_H_
#define _SHTC3_H_

#include <stdint.h>

int temperature_read_data(void);
int humidity_read_data(void);
int temperature_get_data(double *sensor_data);
int humidity_get_data(double *sensor_data);

int shtc3_get_temp_and_humi(double *temp, double *humi);
int shtc3_get_temp_and_humi_polling(double *temp, double *humi);
int shtc3_wakeup(void);
int shtc3_sleep(void);

#endif // _SHTC3_H_
