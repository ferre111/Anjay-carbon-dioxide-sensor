#ifndef _PASCO2_H_
#define _PASCO2_H_

#include <stdint.h>

#define PASCO2_MEASURMENTS_PERIOD 10 // in seconds
#define CO2_NUMBER_OF_MEASURMENTS_PER_HOUR 3600U / (PASCO2_MEASURMENTS_PERIOD)

int pasco2_init(void);
int pasco2_is_measur_rdy(void);
int pasco2_get_measur_val(uint16_t *val);
int pasco2_reset_int_status_clear(void);
int pasco2_reset(void);

#endif // _PASCO2_H_
