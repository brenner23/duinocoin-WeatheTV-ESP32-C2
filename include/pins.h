#ifndef PINS_H
#define PINS_H

// ESP32-C2 "Pult" - ST7789V 240x240, Hardware-SPI
// CS liegt fest auf GND, kein MISO, Backlight LOW-aktiv.
#define SCREEN_W 240
#define SCREEN_H 240

#define TFT_SCK   4
#define TFT_MOSI  6
#define TFT_MISO -1
#define TFT_CS   -1
#define TFT_DC    5
#define TFT_RST   1

#define TFT_BACKLIGHT 18

#endif
