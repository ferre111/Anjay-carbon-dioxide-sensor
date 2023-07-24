#include "pasco2.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "i2c_wrapper.h"
#include <anjay/anjay.h>

#if CONFIG_ANJAY_CLIENT_BOARD_PASCO2

#    define I2C_ADDRESS_PASCO2 0x28
#    define I2C_SDA_PASCO2 21
#    define I2C_SCL_PASCO2 22
#    define I2C_FREQ_PASCO2 400000

#    define REG_ADDR_SENS_STS 0x01
#    define REG_ADDR_MEAS_RATE_H 0x02
#    define REG_ADDR_MEAS_RATE_L 0x03
#    define REG_ADDR_MEAS_CFG 0x04
#    define REG_ADDR_CO2PPM_H 0x05
#    define REG_ADDR_CO2PPM_L 0x06
#    define REG_ADDR_MEAS_STS 0x07
#    define REG_ADDR_INT_CFG 0x08
#    define REG_ADDR_PRES_REF_H 0x0B
#    define REG_ADDR_PRES_REF_L 0x0C
#    define REG_ADDR_SENS_RST 0x10

#    define SENS_STS_CORRECT_VAL 0xC0
#    define MEAS_STS_MEASUR_RDY_VAL 0x10

#    define RESET_VAL 0xA3

static const char *TAG = "pasco2";

static i2c_device_t pasco2_device = {
    .config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA_PASCO2,
        .scl_io_num = I2C_SCL_PASCO2,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_FREQ_PASCO2
    },
    .port = I2C_NUM_1,
    .address = I2C_ADDRESS_PASCO2
};

static int pasco2_check_sts_reg(void) {
    uint8_t reg_val = 0;
    if (i2c_master_read_slave_reg(&pasco2_device, REG_ADDR_SENS_STS, &reg_val,
                                  1U)
            || SENS_STS_CORRECT_VAL != reg_val) {
        ESP_LOGW(TAG,
                 "Cannot read PASCO2 sensor status register or wrong status "
                 "register value: %d",
                 reg_val);
        return -1;
    }
    return 0;
}

int pasco2_init(void) {
    // check if status register have corrected value
    uint8_t aux = 0;
    if (pasco2_reset()) {
        return -1;
    }

    if (pasco2_check_sts_reg()) {
        return -1;
    }

    /* without changing pressure compensation registers */
    // if (i2c_master_write_slave_reg(&pasco2_device, REG_ADDR_PRES_REF_H, xx,
    // 1) || i2c_master_write_slave_reg(&pasco2_device, REG_ADDR_PRES_REF_L, xx,
    // 1)){
    //  return -1;
    // }

    // Idle mode
    if (i2c_master_write_slave_reg(&pasco2_device, REG_ADDR_MEAS_CFG, &aux,
                                   1)) {
        return -1;
    }

    vTaskDelay(pdMS_TO_TICKS(1000UL));

    // Set measurment period
    aux = (uint8_t) (PASCO2_MEASURMENTS_PERIOD >> 8);
    if (i2c_master_write_slave_reg(&pasco2_device, REG_ADDR_MEAS_RATE_H, &aux,
                                   1)) {
        return -1;
    }
    // uint8_t aux = 0x3C;
    aux = (uint8_t) (PASCO2_MEASURMENTS_PERIOD & 0x00FF);
    if (i2c_master_write_slave_reg(&pasco2_device, REG_ADDR_MEAS_RATE_L, &aux,
                                   1)) {
        return -1;
    }

    // low active, data ready notification
    aux = (1 << 2);
    if (i2c_master_write_slave_reg(&pasco2_device, REG_ADDR_INT_CFG, &aux, 1)) {
        return -1;
    }

    // Continuous mode enable, ABOC enabled, PWM sf disabled
    aux = (((1 << 1) & ~(1 << 0)) | ((1 << 2) & ~(1 << 3))) & ~(1 << 5);

    if (i2c_master_write_slave_reg(&pasco2_device, REG_ADDR_MEAS_CFG, &aux,
                                   1)) {
        return -1;
    }

    return 0;
}

int pasco2_is_measur_rdy(void) {
    uint8_t reg_val = 0;
    if (i2c_master_read_slave_reg(&pasco2_device, REG_ADDR_MEAS_STS, &reg_val,
                                  1U)
            || !(reg_val & (1 << 4))) {
        return -1;
    }
    return 0;
}

int pasco2_get_measur_val(uint16_t *val) {
    assert(val);

    uint8_t aux[2];
    if (i2c_master_read_slave_reg(&pasco2_device, REG_ADDR_CO2PPM_H, aux, 2U)) {
        return -1;
    }

    *val = ((uint32_t) aux[0] << 8) + aux[1];
    return 0;
}

int pasco2_reset_int_status_clear(void) {
    uint8_t aux;

    aux = (1 << 1);
    if (i2c_master_write_slave_reg(&pasco2_device, REG_ADDR_MEAS_STS, &aux,
                                   1)) {
        return -1;
    }

    return 0;
}

int pasco2_reset(void) {
    uint8_t aux = RESET_VAL;

    if (i2c_master_write_slave_reg(&pasco2_device, REG_ADDR_SENS_RST, &aux,
                                   1)) {
        return -1;
    }
    return 0;
}

#endif // CONFIG_ANJAY_CLIENT_BOARD_PASCO2
