#include <Arduino.h>

#include "epd_driver.h"  // Definitions for screen width and height
#include "screen_io.h"

#define DBG_OUTPUT_PORT Serial

typedef enum {
  SUCCSESS = 0,
  UNEXPECTED_STATUS_CODE = 1,
  UNKOWN_VERSION = 2,
  INVALID_SLEEP_TIME = 3,
  UNEXPECTED_END_OF_STREAM = 4,
  WIDTH_TOO_HIGH = 5,
  HEIGHT_TOO_HIGH = 6
} net_state_t;

net_state_t process_stream_V1(Stream *stream, uint32_t *imageId,
                              uint32_t *sleepTime) {
  size_t response_length;

  response_length = stream->readBytes((uint8_t *)imageId, 4);
  if (response_length < 4) {
    write_error("Stream ended unexpectedly while reading imageId");
    return UNEXPECTED_END_OF_STREAM;
  }
  DBG_OUTPUT_PORT.printf("Image id: %u", imageId);

  response_length = stream->readBytes((uint8_t *)sleepTime, 4);
  if (response_length < 4) {
    write_error("Stream ended unexpectedly while reading sleepTime");
    return UNEXPECTED_END_OF_STREAM;
  }
  DBG_OUTPUT_PORT.printf("Sleep time: %u", sleepTime);

  if (&sleepTime <= 0) {
    write_error("Recieved sleep time with value 0");
    return INVALID_SLEEP_TIME;
  }

  // Repeat as long as the stream continues
  while (true) {
    // Set one second timeout as long as we recieve the metadata
    stream->setTimeout(1000);

    uint16_t x;
    response_length = stream->readBytes((uint8_t *)&x, 2);
    if (response_length == 0) {
      // stream is finished, nothing more to read; we are done here!
      break;
    } else if (response_length < 2) {
      write_error("Stream ended unexpectedly while reading x");
      return UNEXPECTED_END_OF_STREAM;
    }
    DBG_OUTPUT_PORT.printf("x: %u", x);

    uint16_t y;
    stream->readBytes((uint8_t *)&y, 2);
    if (response_length < 2) {
      write_error("Stream ended unexpectedly while reading y");
      return UNEXPECTED_END_OF_STREAM;
    }
    DBG_OUTPUT_PORT.printf("y: %u", y);

    uint16_t width;
    response_length = stream->readBytes((uint8_t *)&width, 2);
    if (response_length < 2) {
      write_error("Stream ended unexpectedly while reading width");
      return UNEXPECTED_END_OF_STREAM;
    }
    DBG_OUTPUT_PORT.printf("width: %u", width);
    if (width > EPD_WIDTH) {
      write_error("Image returned from server is to wide: " + String(width));
      return WIDTH_TOO_HIGH;
    }

    uint16_t height;
    response_length = stream->readBytes((uint8_t *)&height, 2);
    if (response_length < 2) {
      write_error("Stream ended unexpectedly while reading height");
      return UNEXPECTED_END_OF_STREAM;
    }
    DBG_OUTPUT_PORT.printf("height: %u", height);
    if (height > EPD_HEIGHT) {
      write_error("Image returned from server is to tall: " + String(height));
      return HEIGHT_TOO_HIGH;
    }

    uint32_t size = (uint32_t)width * height / 2;
    if (size == 0) {
      return SUCCSESS;
    }

    uint8_t *pixel = (uint8_t *)ps_malloc(size);
    Rect_t area = {
        .x = x,
        .y = y,
        .width = width,
        .height = height,
    };

    // Take all the time we need to read the final data stream
    stream->setTimeout(30 * 1000);
    response_length = stream->readBytes(pixel, size);
    if (response_length < size) {
      write_error("Stream ended unexpectedly while reading image data");
      return UNEXPECTED_END_OF_STREAM;
    }

    epd_clear_area(area);
    epd_draw_image(area, pixel, BLACK_ON_WHITE);
    free(pixel);
  }

  return SUCCSESS;
}

net_state_t process_stream(Stream *stream, uint32_t *imageId,
                           uint32_t *sleepTime) {
  uint8_t version;
  size_t response_length;

  response_length = stream->readBytes(&version, 1);
  if (response_length < 1) {
    write_error("Stream ended unexpectedly while reading schema version");
    return UNEXPECTED_END_OF_STREAM;
  }
  DBG_OUTPUT_PORT.printf("Schema Version: %u", version);

  switch (version) {
    case 1:
      return process_stream_V1(stream, imageId, sleepTime);
    default:
      write_error("Unexpected schema version: " + String(version));
      return UNKOWN_VERSION;
  }
}