#pragma once

#include <Arduino.h>
#include "epd_driver.h"
#include "opensans16.h"

#define INITIAL_TEXT_CURSOR_X 200
#define INITIAL_TEXT_CURSOR_Y 100

int cursor_x = INITIAL_TEXT_CURSOR_X;
int cursor_y = INITIAL_TEXT_CURSOR_Y;

bool print_on_display = false;

void reset_text_cursor() {
    cursor_x = INITIAL_TEXT_CURSOR_X;
    cursor_y = INITIAL_TEXT_CURSOR_Y;
}

void write_text(String string, int offset) {
    if (print_on_display) {
        cursor_x += offset;
        writeln((GFXfont *)&OpenSans16, (char *)string.c_str(), &cursor_x, &cursor_y, NULL);
        cursor_x = 200;
        cursor_y += 35;
        delay(500); // Make sure text is seen and not directly overwritten.
    } else {
        Serial.println(string);
    }
}

void write_text(String string) { write_text(string, true); }

void write_error(String string, bool clearArea) {
    Rect_t area = {
        .x = 0,
        .y = EPD_HEIGHT - 100,
        .width = EPD_WIDTH,
        .height = 100,
    };

    uint8_t *framebuffer = (uint8_t *)ps_calloc(sizeof(uint8_t), area.height * area.width / 2);
    memset(framebuffer, 0xFF, area.height * area.width / 2);

    epd_draw_rect(30, 20, area.width - 70, 60, 0, framebuffer);

    int x = 50;
    int y = 65;
    writeln((GFXfont *)&OpenSans16, (char *)string.c_str(), &x, &y, framebuffer);

    epd_poweron();
    if (clearArea) {
        epd_clear_area(area);
    }

    epd_draw_grayscale_image(area, framebuffer);
    free(framebuffer);

    epd_poweroff();
    delay(2000); // Make sure text is seen!
}

void write_error(String string) {
    write_error(string, true);
}

void write_header(String string) {
  if (print_on_display) {
    int x = EPD_WIDTH - 150;
    int y = 50;
    writeln((GFXfont *)&OpenSans16, (char *)string.c_str(), &x, &y, NULL);
  }
}
