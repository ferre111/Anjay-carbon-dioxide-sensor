//==============================================================================
//    S E N S I R I O N   AG,  Laubisruetistr. 50, CH-8712 Staefa, Switzerland
//==============================================================================
// Project   :  SHTC3 Sample Code (V1.0)
// File      :  shtc3.c (V1.0)
// Author    :  RFU
// Date      :  24-Nov-2017
// Controller:  STM32F100RB
// IDE       :  �Vision V5.17.0.0
// Compiler  :  Armcc
// Brief     :  Sensor Layer: Implementation of functions for sensor access.
//==============================================================================

#include "shtc3.h"

#include "driver/i2c.h"
#include "esp_log.h"
#include "i2c_wrapper.h"
#include "esp_log.h"
#include "oled_page.h"

#define CRC_POLYNOMIAL  0x131 // P(x) = x^8 + x^5 + x^4 + 1 = 100110001

#define READ_ID             0xEFC8  // command: read ID register
#define SOFT_RESET          0x805D  // soft reset
#define SLEEP               0xB098  // sleep
#define WAKEUP              0x3517  // wakeup
#define MEAS_T_RH_POLLING   0x7866  // meas. read T first, clock stretching disabled
#define MEAS_T_RH_CLOCKSTR  0x7CA2  // meas. read T first, clock stretching enabled
#define MEAS_RH_T_POLLING   0x58E0  // meas. read RH first, clock stretching disabled
#define MEAS_RH_T_CLOCKSTR  0x5C24  // meas. read RH first, clock stretching enabled

static const char *TAG = "shtc3";

static int shtc3_check_crc(uint8_t data[], uint8_t nbrOfBytes,
                              uint8_t checksum);
static double shtc3_calc_temperature(uint16_t rawValue);
static double shtc3_calc_humidity(uint16_t rawValue);

#define I2C_ADDRESS_SHTC3      0x70
#define I2C_SDA_SHTC3          21
#define I2C_SCL_SHTC3          22
#define I2C_FREQ_SHTC3         400000

static i2c_device_t shtc3_device = {
    .config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA_SHTC3,
        .scl_io_num = I2C_SCL_SHTC3,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_FREQ_SHTC3
    },
    .port = I2C_NUM_1,
    .address = I2C_ADDRESS_SHTC3
};

static int shtc3_write_command(const i2c_device_t *const device,
                               const uint16_t command) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (cmd != NULL) {
        if (i2c_master_start(cmd)) {
            return -1;
        }
        if (i2c_master_write_byte(cmd,
                                  (device->address << 1) | I2C_MASTER_WRITE,
                                  I2C_ACK_CHECK_EN)) {
            return -1;
        }
        if (i2c_master_write_byte(cmd, (uint8_t)(command >> 8), I2C_ACK_CHECK_EN)) {
            return -1;
        }
        if (i2c_master_write_byte(cmd, (uint8_t)(command & 0xFF), I2C_ACK_CHECK_EN)) {
            return -1;
        }
        if (i2c_master_stop(cmd)) {
            return -1;
        }
        esp_err_t ret =
                i2c_master_cmd_begin(device->port, cmd, I2C_TIMEOUT_TICKS);
        i2c_cmd_link_delete(cmd);
        return (int) ret;
    } else {
        return -1;
    }
}

static int shtc3_read_hum_temp(const i2c_device_t *const device, uint8_t *data) {
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (cmd != NULL) {
        if (i2c_master_start(cmd)) {
            return -1;
        }
        if (i2c_master_write_byte(cmd,
                                  (device->address << 1) | I2C_MASTER_READ,
                                  I2C_ACK_CHECK_EN)) {
            return -1;
        }
        if (i2c_master_read(cmd, data, 6, I2C_MASTER_ACK)) {
            return -1;
        }
        if (i2c_master_stop(cmd)) {
            return -1;
        }
        esp_err_t ret =
                i2c_master_cmd_begin(device->port, cmd, I2C_TIMEOUT_TICKS);
        i2c_cmd_link_delete(cmd);
        return (int) ret;
    } else {
        return -1;
    }
}

int shtc3_get_temp_and_humi(double *temp, double *humi){
  uint8_t data[6];

  if (shtc3_write_command(&shtc3_device, MEAS_T_RH_CLOCKSTR) || shtc3_read_hum_temp(&shtc3_device, data)){
    return -1;
  }

  if (shtc3_check_crc(data, 2, data[2]) || shtc3_check_crc(&data[3], 2, data[5])) {
    return -1;
  }

  *temp = shtc3_calc_temperature(((uint16_t)data[0] << 8) + data[1]);
  *humi = shtc3_calc_humidity(((uint16_t)data[3] << 8) + data[4]);

  return 0;
}

int shtc3_get_temp_and_humi_polling(double *temp, double *humi){
  int error;
  uint8_t  maxPolling = 20;
  uint8_t data[6];

  // measure, read temperature first, clock streching disabled (polling)
  if(shtc3_write_command(&shtc3_device, MEAS_T_RH_POLLING)) {
    return -1;
  }
  // poll every 1ms for measurement ready
  while(maxPolling--) {
    // check if the measurement has finished
    error = shtc3_read_hum_temp(&shtc3_device, data);

    if(!error) {
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }

  if(shtc3_check_crc(data, 2, data[2]) || shtc3_check_crc(&data[3], 2, data[5])) {
    return -1;
  }

  *temp = shtc3_calc_temperature(((uint16_t)data[0] << 8) + data[1]);
  *humi = shtc3_calc_humidity(((uint16_t)data[3] << 8) + data[4]);

  return 0;
}

static double current_temp, current_humi;

int temperature_read_data(void) {
    if (shtc3_wakeup()) {
        ESP_LOGW(TAG, "shtc3_wakeup has failed");
        return -1;
    }

    if (shtc3_get_temp_and_humi_polling(&current_temp, &current_humi)) {
        ESP_LOGW(TAG, "shtc3_get_temp_and_humi has failed");
        return -1;
    }

    if (shtc3_sleep()) {
        ESP_LOGW(TAG, "shtc3_sleep has failed");
        return -1;
    }
    return 0;
}

int humidity_read_data(void) {
    return temperature_read_data();
}

int temperature_get_data(double *sensor_data) {
  oled_update_temp(current_temp);
  *sensor_data = current_temp;
  return 0;
}

int humidity_get_data(double *sensor_data) {
  oled_update_humi(current_humi);
  *sensor_data = current_humi;
  return 0;
}

int shtc3_sleep(void) {
  return shtc3_write_command(&shtc3_device, SLEEP);
}

int shtc3_wakeup(void) {
  int error = shtc3_write_command(&shtc3_device, WAKEUP);

  vTaskDelay(pdMS_TO_TICKS(50));

  return error;
}

int shtc3_soft_reset(void){
  return shtc3_write_command(&shtc3_device, SOFT_RESET);
}

static int shtc3_check_crc(uint8_t data[], uint8_t nbrOfBytes,
                              uint8_t checksum){
  uint8_t bit;
  uint8_t crc = 0xFF; // calculated checksum
  uint8_t byteCtr;    // byte counter

  // calculates 8-Bit checksum with given polynomial
  for(byteCtr = 0; byteCtr < nbrOfBytes; byteCtr++) {
    crc ^= (data[byteCtr]);
    for(bit = 8; bit > 0; --bit) {
      if(crc & 0x80) {
        crc = (crc << 1) ^ CRC_POLYNOMIAL;
      } else {
        crc = (crc << 1);
      }
    }
  }

  if(crc != checksum) {
    return -1;
  }

  return 0;
}

static double shtc3_calc_temperature(uint16_t rawValue){
  // calculate temperature
  // T = -45 + 175 * rawValue / 2^16
  return 175 * (double)rawValue / 65536.0f - 45.0f;
}

static double shtc3_calc_humidity(uint16_t rawValue){
  // calculate relative humidity
  // RH = rawValue / 2^16 * 100
  return 100 * (double)rawValue / 65536.0f;
}
