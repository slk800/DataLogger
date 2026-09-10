/**
 * @file    font6x8.h
 * @brief   6x8 printable ASCII font.
 */

#ifndef __FONT6X8_H
#define __FONT6X8_H

#include <stdint.h>

#define FONT6X8_FIRST_CHAR  32
#define FONT6X8_LAST_CHAR   126
#define FONT6X8_WIDTH       6
#define FONT6X8_HEIGHT      8

extern const uint8_t g_font6x8[(FONT6X8_LAST_CHAR - FONT6X8_FIRST_CHAR + 1)][FONT6X8_WIDTH];

#endif /* __FONT6X8_H */
