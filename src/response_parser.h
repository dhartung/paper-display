#include <Arduino.h>
#include <miniz.h>

#include "epd_driver.h"  // Definitions for screen width and height
#include "screen_io.h"
#include "stream.cpp"

#define SUPPORTED_VERSIONS "1,2"
#define DBG_OUTPUT_PORT Serial

typedef enum {
  SUCCESS = 0,
  UNEXPECTED_STATUS_CODE = 1,
  UNKOWN_VERSION = 2,
  INVALID_SLEEP_TIME = 3,
  UNEXPECTED_END_OF_STREAM = 4,
  WIDTH_TOO_HIGH = 5,
  HEIGHT_TOO_HIGH = 6,
  PAYLOAD_TOO_LARGE = 7,
  UNKNOWN_ERROR = 8,
} net_state_t;

net_state_t process_stream_V1(ResponseStream *stream, uint32_t *imageId,
                              uint32_t *sleepTime) {
  stream->readUint32(imageId);
  if (stream->getStatus()) {
    write_error("Stream ended unexpectedly while reading imageId");
    return UNEXPECTED_END_OF_STREAM;
  }
  DBG_OUTPUT_PORT.printf("Image id: %u\n", imageId);

  stream->readUint32(sleepTime);
  if (stream->getStatus()) {
    write_error("Stream ended unexpectedly while reading sleepTime");
    return UNEXPECTED_END_OF_STREAM;
  }
  DBG_OUTPUT_PORT.printf("Sleep time: %u\n", sleepTime);

  if (&sleepTime <= 0) {
    write_error("Received sleep time with value 0");
    return INVALID_SLEEP_TIME;
  }

  // Repeat as long as the stream continues
  while (true) {
    uint16_t x = stream->readUint16();
    if (stream->getStatus() == ST_STREAM_END) {
      // stream is finished, nothing more to read; we are done here!
      break;
    } else if (stream->getStatus()) {
      write_error("Stream ended unexpectedly while reading x");
      return UNEXPECTED_END_OF_STREAM;
    }
    DBG_OUTPUT_PORT.printf("x: %u\n", x);

    uint16_t y = stream->readUint16();
    if (stream->getStatus()) {
      write_error("Stream ended unexpectedly while reading y");
      return UNEXPECTED_END_OF_STREAM;
    }
    DBG_OUTPUT_PORT.printf("y: %u\n", y);

    uint16_t width = stream->readUint16();
    if (stream->getStatus()) {
      write_error("Stream ended unexpectedly while reading width");
      return UNEXPECTED_END_OF_STREAM;
    }
    DBG_OUTPUT_PORT.printf("width: %u\n", width);
    if (width > EPD_WIDTH) {
      write_error("Image returned from server is to wide: " + String(width));
      return WIDTH_TOO_HIGH;
    }

    uint16_t height = stream->readUint16();
    if (stream->getStatus()) {
      write_error("Stream ended unexpectedly while reading height");
      return UNEXPECTED_END_OF_STREAM;
    }
    DBG_OUTPUT_PORT.printf("height: %u\n", height);
    if (height > EPD_HEIGHT) {
      write_error("Image returned from server is to tall: " + String(height));
      return HEIGHT_TOO_HIGH;
    }

    uint32_t size = (uint32_t)width * height / 2;
    if (size == 0) {
      return SUCCESS;
    }

    uint8_t *pixel = (uint8_t *)ps_malloc(size);
    Rect_t area = {
        .x = x,
        .y = y,
        .width = width,
        .height = height,
    };

    stream->readBytes(pixel, size);
    if (stream->getStatus()) {
      write_error("Stream ended unexpectedly while reading image data");
      free(pixel);
      return UNEXPECTED_END_OF_STREAM;
    }

    epd_clear_area(area);
    epd_draw_image(area, pixel, BLACK_ON_WHITE);
    free(pixel);
  }

  return SUCCESS;
}

net_state_t process_stream_V2(HttpStream *stream, uint32_t *imageId,
                              uint32_t *sleepTime) {
  // The response length is not perfect, but if it is set, we try to read
  // exactly the specified length
  long compressed_length = stream->getExpectedRemainingSize();
  if (compressed_length <= 0) {
    compressed_length = MAX_SIZE;
  }
  unsigned char *compressed_bytes =
      (unsigned char *)ps_malloc(compressed_length);
  size_t compressed_size =
      stream->readBytes(compressed_bytes, compressed_length);

  if (compressed_size >= MAX_SIZE) {
    free(compressed_bytes);
    write_error("Payload too large: " + String(compressed_size));
    return PAYLOAD_TOO_LARGE;
  }

  mz_ulong extracted_size = MAX_SIZE;
  unsigned char *extracted_bytes = (unsigned char *)ps_malloc(extracted_size);
  int result_code = mz_uncompress(extracted_bytes, &extracted_size,
                                  compressed_bytes, compressed_size);
  free(compressed_bytes);
  DBG_OUTPUT_PORT.printf("Compressed size: %u, Decompressed size: %u\n",
                         compressed_size, extracted_size);

  if (result_code != MZ_OK) {
    write_error("Decompression error: MZ_" + String(result_code));
    free(extracted_bytes);
    return UNKNOWN_ERROR;
  }

  BufferedStream buffer(extracted_bytes, extracted_size);

  // v2 is v1 with compressed payload, so we can just call the v1 version
  net_state_t result = process_stream_V1(&buffer, imageId, sleepTime);
  free(extracted_bytes);
  return result;
}

net_state_t process_stream(HttpStream *stream, uint32_t *imageId,
                           uint32_t *sleepTime) {
  uint8_t version = stream->readUint8();
  if (stream->getStatus()) {
    write_error("Stream ended unexpectedly while reading schema version");
    return UNEXPECTED_END_OF_STREAM;
  }
  DBG_OUTPUT_PORT.printf("Schema Version: %u", version);

  switch (version) {
    case 1:
      return process_stream_V1(stream, imageId, sleepTime);
    case 2:
      return process_stream_V2(stream, imageId, sleepTime);
    default:
      write_error("Unexpected schema version: " + String(version));
      return UNKOWN_VERSION;
  }
}