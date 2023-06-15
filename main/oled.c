/*
 * OLED.c
 *
 *  Created on: Nov 19, 2020
 *      Author: Wiktor Lechowicz
 */
#include "oled.h"
#include "driver/i2c.h"
#include "i2c_wrapper.h"

#include "ascii_font.h"

//---------------------------------------------------------------------------------------
// DEFINES

// Control byte options
#define OLED_CONTROL_BYTE_ 0b00000000
#define _OLED_SINGLE_BYTE 0b10000000
#define _OLED_MULTIPLE_BYTES 0b00000000
#define _OLED_COMMAND 0b00000000
#define _OLED_DATA 0b01000000

// basic command set
#define OLED_CMD_EntireDisplayOnPreserve 0xA4
#define OLED_CMD_EntireDisplayOn 0xA5
#define OLED_CMD_SetNotInversedDisplay 0xA6
#define OLED_CMD_SetInversedDisplay 0xA7
#define OLED_CMD_SetDisplayON 0xAF
#define OLED_CMD_SetDisplayOFF 0xAE
#define OLED_CMD_EnableChargePumpDuringDisplay 0x8D

#define TIMEOUT 100

#define FIRST_BUFFER 0
#define SECOND_BUFFER 1

#define OLED_ADDRESS 0x3C

#define I2C_SDA_OLED 21
#define I2C_SCL_OLED 22
#define I2C_FREQ_OLED 400000

static i2c_device_t oled_device = {
    .config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA_OLED,
        .scl_io_num = I2C_SCL_OLED,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_FREQ_OLED
    },
    .port = I2C_NUM_1,
    .address = OLED_ADDRESS
};

//---------------------------------------------------------------------------------------
// INIT BYTE STREAM

const uint8_t initSequence[] = {
    OLED_CMD_SetDisplayOFF,

    0x20, // set memory addressing mode
    0x00, // horizontal addressing mode

    0x21,            // set column address
    0x00,            // start
    OLED_X_SIZE - 1, // stop

    0x22, // set page address
    0x00, // start
    0x07, // stop

    0xD5, // set display clock frequency and prescaler
    0x80,

    0xD9, // set pre-charge period
    0x22,

    0xA8, // set MUX ratio
    0x3F,

    0xD3, // set display offset
    0x00,

    0x40, // set display start line

    0xA1, // mirror horizontally
    // 0xA0

    0xC0, // set COM scan direction

    0xDA, // set com pins hw configuration
    0x12,

    0x81, // set contrast
    0x9F,

    0xC8, // mirror vertically
    // 0xC0

    OLED_CMD_EntireDisplayOnPreserve,

    OLED_CMD_SetNotInversedDisplay,

    OLED_CMD_EnableChargePumpDuringDisplay, 0x14,
    // OLED_CMD_SetDisplayON,
};

typedef enum drawable_enum_T {
    TEXT_FIELD = 0,
    LINE,
    RECTANGLE,
    IMAGE
} drawable_enum;

// common part of all drawable objects
typedef struct drawable_base_T {
    drawable_enum type;
    uint8_t x0;
    uint8_t y0;
    uint8_t isUsed : 1;
} drawable_base;

// specific parts of drawable objects
// === TEXT FIELD ===
typedef struct textField_T {
    char *text; // pointer to character sequence
                // If it is different from zero it will be
                // updated in buffer and decremented so both firstBuffer
                // and secondBuffer becomes updated.
    uint8_t size;
    bool reverse;
} textField;

// === LINE ===
typedef struct line_T {
    uint8_t x1; // end point of line x coordinate
    uint8_t y1; // end point of line y coordinate
} line;

// === RECTANGLE ===
typedef struct rectangle_T {
    uint8_t width;  // rectangle width
    uint8_t height; // rectangle height
} rectangle;

// === IMAGE ===
typedef struct image_T {
    uint8_t *imageArray; // pointer to an array with image representation
} image;

union drawable_specific {
    textField textField;
    line line;
    rectangle rectangle;
    image image;
};

// drawable object type
typedef struct drawable_T {
    drawable_base common;
    union drawable_specific spec;
} drawable;

// struct for managing OLED. This is not a part of API.
typedef struct oled_T {
    uint8_t buffer[OLED_NUM_OF_PAGES * OLED_X_SIZE];
    drawable drawables[OLED_MAX_DRAWABLES_COUNT]; // drawable objects
} oled;

static oled oled_ctx;

//---------------------------------------------------------------------------------------
/* STATIC FUNCTIONS */

/*send single command to driver*/
static void sendCommand(uint8_t command) {
    i2c_master_write_slave_reg(&oled_device, 0x00, &command, 1);
}

/* send stream of commands to driver */
static void sendCommandStream(const uint8_t *stream, uint16_t streamLength) {
    assert(streamLength);

    i2c_master_write_slave_reg(&oled_device, 0x01, stream, streamLength);
}

/* set next column pointer in display driver */
static void setAddress(uint8_t page, uint8_t column) {
    uint8_t addressArray[3];

    addressArray[0] = 0xB0 | (page & 0x07);
    addressArray[1] = column & 0x0F;
    addressArray[2] = 0x10 | ((0xF0 & column) >> 4);

    i2c_master_write_slave_reg(&oled_device,
                               OLED_CONTROL_BYTE_ | _OLED_COMMAND
                                       | _OLED_MULTIPLE_BYTES,
                               addressArray, 3);
}

static void setPixel(uint8_t x, uint8_t y) {
    *(oled_ctx.buffer + x + OLED_X_SIZE * ((uint8_t) (y / 8))) |=
            (0x01 << y % 8);
}

/* clear display buffer content */
void clearScreen() {
    for (uint8_t v = 0; v < OLED_NUM_OF_PAGES; v++) {
        for (uint8_t c = 0; c < OLED_X_SIZE; c++) {
            *(oled_ctx.buffer + v * OLED_X_SIZE + c) = 0;
        }
    }
}

/* print text to buffer */
static void
printText(uint8_t x0, uint8_t y0, char *text, uint8_t size, bool reverse) {

    if (x0 >= OLED_X_SIZE || (y0 + 8) >= OLED_Y_SIZE)
        return;

    uint8_t i = 0;
    uint8_t v = y0 / 8;
    uint8_t rem = y0 % 8;
    uint8_t y = y0;

    while (text[i] != '\0') {
        if (size == 1) {
            if (rem) {
                for (uint8_t j = 0; j < 6; j++) {
                    if (true == reverse) {
                        *(oled_ctx.buffer + v * OLED_X_SIZE + x0) |=
                                ~((font_ASCII[text[i] - ' '][j] << rem));
                        *(oled_ctx.buffer + (v + 1U) * OLED_X_SIZE + x0++) |=
                                ~((font_ASCII[text[i] - ' '][j] >> (8 - rem)));
                    } else {
                        *(oled_ctx.buffer + v * OLED_X_SIZE + x0) |=
                                ((font_ASCII[text[i] - ' '][j] << rem));
                        *(oled_ctx.buffer + (v + 1U) * OLED_X_SIZE + x0++) |=
                                ((font_ASCII[text[i] - ' '][j] >> (8 - rem)));
                    }
                }
            } else {
                for (uint8_t j = 0; j < 6; j++) {
                    if (true == reverse) {
                        *(oled_ctx.buffer + v * OLED_X_SIZE + x0++) =
                                ~(font_ASCII[text[i] - ' '][j]);
                    } else {
                        *(oled_ctx.buffer + v * OLED_X_SIZE + x0++) =
                                (font_ASCII[text[i] - ' '][j]);
                    }
                }
            }
            i++;
        } else if (size > 1) {
            for (uint8_t j = 0; j < 6 * size; j++) {
                for (uint8_t k = 0; k < 8; k++) {
                    for (uint8_t l = 0; l < size; l++) {
                        if (font_ASCII[text[i] - ' '][j / size] & (0x01 << k)) {
                            setPixel(x0, y);
                        }
                        y++;
                    }
                }
                y = y0;
                x0++;
            }
            i++;
        }
    }
}

/* draw line in buffer */
static void drawLine(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) {
    static float tan = 0.0f;
    static float oneOverTan = 0.0f;
    if (y1 == y0) {
        tan = 0.0f;
        oneOverTan = 99999.0f;
    } else if (x1 == x0) {
        tan = 99999.0f;
        oneOverTan = 0.0f;
    } else {
        float temp = ((float) y1 - y0) / ((float) x1 - x0);
        if (temp > 0)
            tan = temp;
        else
            tan = -temp;
        oneOverTan = 1.0f / tan;
    }

    int8_t xDir = 0, yDir = 0;
    if (x1 - x0 > 0)
        xDir = 1;
    else
        xDir = -1;

    if (y1 - y0 > 0)
        yDir = 1;
    else
        yDir = -1;

    float y = y0;
    float x = x0;

    uint8_t numOfIterations = 0, xLen = 0, yLen = 0;

    if (x1 - x0 > 0)
        xLen = x1 - x0;
    else
        xLen = x0 - x1;

    if (y1 - y0 > 0)
        yLen = y1 - y0;
    else
        yLen = y0 - y1;

    if (xLen > yLen)
        numOfIterations = xLen + 1;
    else
        numOfIterations = yLen + 1;

    float yTemp = 0.5f, xTemp = 0.5f;

    if (1) {
        for (uint8_t i = 0; i < numOfIterations; i++) {
            *(oled_ctx.buffer + ((uint8_t) y / 8) * OLED_X_SIZE
              + (uint8_t) x) |= 1 << ((uint8_t) y % 8);
            yTemp += tan;
            if (yTemp >= 1) {
                if (yDir == 1)
                    y++;
                else
                    y--;
                yTemp = yTemp - (int) yTemp;
            }

            xTemp += oneOverTan;
            if (xTemp >= 1) {
                if (xDir == 1)
                    x++;
                else
                    x--;
                xTemp = xTemp - (int) xTemp;
            }
        }
    }
}

/* draw rectangle in buffer */
static void drawRect(
        uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, enum OLED_Color color) {
    uint8_t rem0 = y0 % 8; // 6
    uint8_t rem1 = y1 % 8; // 1

    uint8_t v = y0 / 8;
    uint8_t c = x0;

    while (c <= x1) {
        if (color == WHITE)
            if (y1 / 8 == y0 / 8)
                *(oled_ctx.buffer + v * OLED_X_SIZE + c++) |=
                        (0xFF << rem0) & (0xFF >> (8 - rem1));
            else
                *(oled_ctx.buffer + v * OLED_X_SIZE + c++) |= 0xFF << rem0;
        else if (y1 / 8 == y0 / 8)
            *(oled_ctx.buffer + v * OLED_X_SIZE + c++) &=
                    ~((0xFF << rem0) & (0xFF >> (8 - rem1)));
        else
            *(oled_ctx.buffer + v * OLED_X_SIZE + c++) &= ~(0xFF << rem0);
    }
    v++;

    while (v < y1 / 8) {
        c = x0;
        while (c <= x1) {
            if (color == WHITE)
                *(oled_ctx.buffer + v * OLED_X_SIZE + c++) = 0xFF;
            else
                *(oled_ctx.buffer + v * OLED_X_SIZE + c++) = 0x00;
        }
        v++;
    }

    if ((y1 % 8) && (y1 / 8 != y0 / 8)) {
        c = x0;
        while (c <= x1) {
            if (color == WHITE)
                *(oled_ctx.buffer + v * OLED_X_SIZE + c++) |=
                        0xFF >> (8 - rem1);
            else
                *(oled_ctx.buffer + v * OLED_X_SIZE + c++) &=
                        ~(0xFF >> (8 - rem1));
        }
    }
}

/* draw image in buffer */
static void drawImage(uint8_t x0, uint8_t y0, const uint8_t image[]) {
    uint8_t width = image[0], height = image[1];
    uint8_t x1 = x0 + width - 1;
    uint8_t y1 = y0 + height - 1;
    uint8_t rem0 = y0 % 8; // 6

    uint8_t v = (y0 / 8) % OLED_NUM_OF_PAGES;
    uint8_t c;

    uint16_t i = 2;

    if (rem0 == 0) {
        while (v <= y1 / 8) {
            c = x0;
            while (c <= x1) {
                *(oled_ctx.buffer + v * OLED_X_SIZE + c++) |= image[i++];
            }
            v++;
        }
    } else {
        c = x0;
        while (c <= x1) {
            *(oled_ctx.buffer + v * OLED_X_SIZE + c++) |= image[i++] << rem0;
        }

        v++;
        while ((v <= y1 / 8) && (v < 8)) {
            c = x0;
            while (c <= x1) {
                *(oled_ctx.buffer + v * OLED_X_SIZE + c) |=
                        image[i - width] >> (8 - rem0);
                if ((v != y1 / 8) && (v < 8))
                    *(oled_ctx.buffer + v * OLED_X_SIZE + c) |= image[i]
                                                                << rem0;
                i++;
                c++;
            }
            v++;
        }
    }
}

/* get next unused ID for new drawable object*/
static void getNextFreeId(uint8_t *id) {
    *id = 0;
    while (oled_ctx.drawables[*id].common.isUsed) {
        (*id)++;
        if (*id == OLED_MAX_DRAWABLES_COUNT) // all drawable slots already used
        {
            id = NULL;
            return;
        }
    }
}
//---------------------------------------------------------------------------------------
/* API functions */
/* These functions are described in header file */

void OLED_Init() {
    uint8_t i = 0;

    i2c_device_init(&oled_device);
    /* free all IDs */
    for (i = 0; i < OLED_MAX_DRAWABLES_COUNT; i++) {
        oled_ctx.drawables[i].common.isUsed = 0;
    }

    /* send sequence of command with initialization data */
    sendCommandStream(initSequence, sizeof(initSequence));

    OLED_update();
}

void OLED_update() {
    /* clear buffer content */
    clearScreen();
    /* updata buffer with drawable objects */
    for (uint8_t i = 0; i < OLED_MAX_DRAWABLES_COUNT; i++) {
        if (oled_ctx.drawables[i].common.isUsed) {
            switch (oled_ctx.drawables[i].common.type) {
            case TEXT_FIELD:
                printText(oled_ctx.drawables[i].common.x0,
                          oled_ctx.drawables[i].common.y0,
                          oled_ctx.drawables[i].spec.textField.text,
                          oled_ctx.drawables[i].spec.textField.size,
                          oled_ctx.drawables[i].spec.textField.reverse);
                break;
            case LINE:
                drawLine(oled_ctx.drawables[i].common.x0,
                         oled_ctx.drawables[i].common.y0,
                         oled_ctx.drawables[i].spec.line.x1,
                         oled_ctx.drawables[i].spec.line.y1);
                break;
            case RECTANGLE:
                drawRect(oled_ctx.drawables[i].common.x0,
                         oled_ctx.drawables[i].common.y0,
                         oled_ctx.drawables[i].common.x0
                                 + oled_ctx.drawables[i].spec.rectangle.width,
                         oled_ctx.drawables[i].common.y0
                                 + oled_ctx.drawables[i].spec.rectangle.height,
                         WHITE);
                break;
            case IMAGE:
                drawImage(oled_ctx.drawables[i].common.x0,
                          oled_ctx.drawables[i].common.y0,
                          oled_ctx.drawables[i].spec.image.imageArray);
                break;
            }
        }
    }

    i2c_master_write_slave_reg(
            &oled_device,
            OLED_CONTROL_BYTE_ | _OLED_DATA | _OLED_MULTIPLE_BYTES,
            oled_ctx.buffer, OLED_X_SIZE * OLED_NUM_OF_PAGES);
}

void OLED_setDisplayOn() {
    sendCommand(OLED_CMD_SetDisplayON);
}

void OLED_setDisplayOff() {
    sendCommand(OLED_CMD_SetDisplayOFF);
}

void OLED_setInversed(uint8_t tf) {
    if (tf)
        sendCommand(OLED_CMD_SetInversedDisplay);
    else
        sendCommand(OLED_CMD_SetNotInversedDisplay);
}

void OLED_moveObject(uint8_t id, uint8_t x0, uint8_t y0) {
    oled_ctx.drawables[id].common.x0 = x0;
    oled_ctx.drawables[id].common.y0 = y0;
}

void OLED_deleteObject(uint8_t id) {
    oled_ctx.drawables[id].common.isUsed = 0;
}

// === TEXT FIELD ===
int OLED_createTextField(uint8_t *id,
                         uint8_t x0,
                         uint8_t y0,
                         char *text,
                         uint8_t fontSize,
                         bool reverse) {
    getNextFreeId(id);
    if (id == NULL) {
        return -1; // all ids used
    }

    oled_ctx.drawables[*id].common.isUsed = 1;
    oled_ctx.drawables[*id].common.type = TEXT_FIELD;
    oled_ctx.drawables[*id].common.x0 = x0;
    oled_ctx.drawables[*id].common.y0 = y0;
    oled_ctx.drawables[*id].spec.textField.text = text;
    oled_ctx.drawables[*id].spec.textField.reverse = reverse;
    if (fontSize == 0)
        fontSize = 1;
    else
        fontSize = fontSize % 5;
    oled_ctx.drawables[*id].spec.textField.size = fontSize;

    return 0;
}

void OLED_textFieldSetText(uint8_t id, char *text) {
    if (TEXT_FIELD == oled_ctx.drawables[id].common.type) {
        oled_ctx.drawables[id].spec.textField.text = text;
    }
}

void OLED_textFieldSetReverse(uint8_t id, bool reverse) {
    if (TEXT_FIELD == oled_ctx.drawables[id].common.type) {
        oled_ctx.drawables[id].spec.textField.reverse = reverse;
    }
}

// === LINE ===
int OLED_createLine(
        uint8_t *id, uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) {
    getNextFreeId(id);
    if (id == NULL) {
        return -1; // all ids used
    }

    oled_ctx.drawables[*id].common.isUsed = 1;
    oled_ctx.drawables[*id].common.type = LINE;
    oled_ctx.drawables[*id].common.x0 = x0;
    oled_ctx.drawables[*id].common.y0 = y0;
    oled_ctx.drawables[*id].spec.line.x1 = x1;
    oled_ctx.drawables[*id].spec.line.y1 = y1;

    return 0;
}

void OLED_lineMoveEnd(uint8_t id, uint8_t x1, uint8_t y1) {
    if (x1 >= OLED_X_SIZE)
        x1 = OLED_X_SIZE - 1;
    if (y1 >= OLED_Y_SIZE)
        y1 = OLED_Y_SIZE - 1;
    oled_ctx.drawables[id].spec.line.x1 = x1;
    oled_ctx.drawables[id].spec.line.y1 = y1;
}

// === RECTANGLE ===
int OLED_createRectangle(
        uint8_t *id, uint8_t x0, uint8_t y0, uint8_t width, uint8_t height) {
    getNextFreeId(id);
    if (id == NULL) {
        return -1; // all ids used
    }

    oled_ctx.drawables[*id].common.isUsed = 1;
    oled_ctx.drawables[*id].common.type = RECTANGLE;
    oled_ctx.drawables[*id].common.x0 = x0;
    oled_ctx.drawables[*id].common.y0 = y0;
    oled_ctx.drawables[*id].spec.rectangle.width = width;
    oled_ctx.drawables[*id].spec.rectangle.height = height;

    return 0;
}

void OLED_rectangleSetDimensions(uint8_t id, uint8_t width, uint8_t height) {
    oled_ctx.drawables[id].spec.rectangle.width = width;
    oled_ctx.drawables[id].spec.rectangle.height = height;
}

// === IMAGE ===
int OLED_createImage(uint8_t *id,
                     uint8_t x0,
                     uint8_t y0,
                     const uint8_t *imageArray) {
    getNextFreeId(id);
    if (id == NULL) {
        return -1; // all ids used
    }

    oled_ctx.drawables[*id].common.isUsed = 1;
    oled_ctx.drawables[*id].common.type = IMAGE;
    oled_ctx.drawables[*id].common.x0 = x0;
    oled_ctx.drawables[*id].common.y0 = y0;
    oled_ctx.drawables[*id].spec.image.imageArray = imageArray;

    return 0;
}
