/*
 * OLED.h
 *
 *  Created on: Nov 19, 2020
 *      Author: Wiktor Lechowicz
 *
 *      This is library for SSD1306 based OLED with I2C interface.
 *      To use this you have to specify I2C module ?where?
 *
 *
 *      rendering procedure description
 *
 *      1. User create drawable objects using one of OLED_create... function.
 *      2. In main loop the oled_update() function must is called at every
 * iteration 3a. If transmission of previous buffer is ongoing, do nothing 3b.
 * Else if previous transmission is finished, DMA transmission of buffer updated
 * at previous function call is started. Then buffers are swaped and the new
 * content based on drawable objects properties is written into a buffer which
 * will be sent to OLED at next function call.
 *      4. User can modify drawable object properties using their IDs outside of
 * the update() function.
 */

#ifndef OLED_H_
#    define OLED_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if CONFIG_ANJAY_CLIENT_OLED

// * DEFINES TO BE MODIFIED BY USER */
#    define OLED_I2C_HANDLE hi2c2
#    define OLED_I2C I2C2
#    define OLED_X_SIZE 128
#    define OLED_Y_SIZE 64
#    define OLED_NUM_OF_PAGES 8

// modify these according to the maximum count of objects used simultaneously to
// save RAM
#    define OLED_MAX_DRAWABLES_COUNT 40

#    define OLED_TRUE 1
#    define OLED_FALSE 0

#    define OLED_MAX_CHAR_PER_LINE \
        21 // maximal number of char that fit in one line (with the smallest
           // font)

enum OLED_Color { WHITE, BLACK };

// I2C_TypeDef OLED_I2C = I2C2;

/* API FUNCTIONS */

/**
 * @brief initialize SSD1306 driver according to initSequence
 * @return void
 */
void oled_init();

/**
 *  @brief Update display with values from buffer.
 *  @return void
 */
void oled_update();

void oled_set_display_on();

void oled_set_display_off();

void oled_set_inversed(uint8_t tf);

/**
 * @brief set object position
 * @param id - object id
 * @param x0 - new x coordinate
 * @param y0 - new y coordniate
 * @retval none
 */
void oled_move_object(uint8_t id, uint8_t x0, uint8_t y0);

/**
 * @brief delete object
 * @param id - object id
 * @retval none
 */
void oled_delete_object(uint8_t id);

// === TEXT FIELD ===

/*
 * @brief create a text field
 * @param id - pointer to variable where textField id will be stored
 * @param x0 - x coordinate of left side of the text field
 * @param verse - verse of text (0 to OLED_NUM_OF_VERSES)
 * @param fontSize - font size (in range 1 to 4)
 * @param reverse - reverse text color (WORKS ONLY WITH SIZE == 1)
 * @retval -1 if failed, 0 otherwise
 */
int oled_create_text_field(uint8_t *id,
                           uint8_t x0,
                           uint8_t y0,
                           char *text,
                           uint8_t fontSize,
                           bool reverse);

/*
 * @brief set text of given textField
 * @param id - textField id
 * @param text - string
 * @retval none
 */
void oled_text_field_set_text(uint8_t id, char *text);

/*
 * @brief set reverse of giben textField
 * @param id - textField id
 * @param reverse - new reverse value
 * @retval none
 */
void oled_text_field_set_reverse(uint8_t id, bool reverse);

// === LINE ===

/*
 * @brief create a lines
 * @param id - pointer to variable where the line id will be stored
 * @param x0 - start point x coordinate
 * @param y0 - start point y coordinate
 * @param x1 - end point x coordinate
 * @param y1 - end point y coordinate
 * @retval -1 if failed, 0 otherwise
 */
int oled_create_line(
        uint8_t *id, uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1);

/*
 * @brief move end point of line
 * @param id - line id
 * @param x1 - x coordinate
 * @param y1 - y coordinate
 * @retval void
 */
void oled_line_move_end(uint8_t id, uint8_t x1, uint8_t y1);

// === RECTANGLE ===

/*
 * @brief create a rectangle
 * @param id - pointer to variable where the rectangle id will be stored
 * @param x0 - upper left corner x coordinate
 * @param y0 - upper left corner y coordinate
 * @param width - rectangle width
 * @param height - rectangle height
 * @param color - BLACK or WHITE
 * @retval -1 if failed, 0 otherwise
 */
int oled_create_rectangle(
        uint8_t *id, uint8_t x0, uint8_t y0, uint8_t width, uint8_t height);

/*
 * @brief set rectangle dimensions
 * @oaram d - rectangle id
 * @param width
 * @param height
 * @retval void
 */
void oled_rectangle_set_dimensions(uint8_t id, uint8_t width, uint8_t height);

// === IMAGE ===

/*
 * @brief create an image
 * @param id - pointer to variable where the image id will be stored
 * @param x0 - upper left corner x coordinate
 * @param y0 - upper left corner y coordinate
 * @param imageArray - pointer to image representation saved in array
 *                      in which the first byte is image width and second byte
 * is image height.
 * @retval -1 if failed, 0 otherwise
 */
int oled_create_image(uint8_t *id,
                      uint8_t x0,
                      uint8_t y0,
                      const uint8_t *imageArray);

#endif // CONFIG_ANJAY_CLIENT_OLED

#endif