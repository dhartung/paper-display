#include <Arduino.h>
#include <Stream.h>
#include <miniz.h>
#include <stddef.h>

#define MAX_SIZE 1024 * 500  // 500 KB

enum {
  ST_OK = 0,
  ST_STREAM_END = 1,
  ST_STREAM_END_UNEXPECTED = 2,
  ST_TOO_LARGE = 3,
  ST_DECOMPRESSION_ERROR = 4
} typedef st_status;

class ResponseStream {
 protected:
  st_status status = ST_OK;
  virtual size_t readBytesRaw(uint8_t* buffer, size_t length);

 public:
  size_t readBytes(uint8_t* buffer, size_t length) {
    size_t size = readBytesRaw(buffer, length);
    if (size == 0) {
      status = ST_STREAM_END;
    } else if (size < length) {
      status = ST_STREAM_END_UNEXPECTED;
    }
    return size;
  }

  st_status getStatus() { return status; }

  void readUint8(uint8_t* value) { readBytes(value, 1); }

  uint8_t readUint8() {
    uint8_t value;
    readUint8(&value);
    return value;
  }

  void readUint16(uint16_t* value) { readBytes((uint8_t*)value, 2); }

  uint16_t readUint16() {
    uint16_t value;
    readUint16(&value);
    return value;
  }

  void readUint32(uint32_t* value) { readBytes((uint8_t*)value, 4); }

  uint32_t readUint32() {
    uint32_t value;
    readUint32(&value);
    return value;
  }
};

class HttpStream : public ResponseStream {
 private:
  Stream* stream;
  int expectedSize;

 protected:
  size_t readBytesRaw(uint8_t* buffer, size_t length) {
    if (expectedSize > 0) expectedSize -= length;

    if (length > 10000) {
      // Increase timeout for large payloads
      stream->setTimeout(15000);
    } else {
      stream->setTimeout(1000);
    }
    return stream->readBytes(buffer, length);
  }

 public:
  HttpStream(Stream* stream, int expectedSize)
      : stream(stream), expectedSize(expectedSize) {
    Serial.println(String("Creation Size: " + String(expectedSize)).c_str());
  }
  HttpStream(Stream* stream) : stream(stream), expectedSize(-1) {}

  size_t getExpectedRemainingSize() {
    Serial.println(String("Ex Size: " + String(expectedSize)).c_str());
    return expectedSize;
  }
};

class BufferedStream : public ResponseStream {
 protected:
  uint8_t* memory;
  size_t memory_size = 0;
  unsigned int offset = 0;
  size_t readBytesRaw(uint8_t* buffer, size_t length) {
    size_t real_length =
        offset + length > memory_size ? memory_size - offset : length;
    memcpy(buffer, memory + offset, real_length);
    offset += real_length;
    return real_length;
  }

 public:
  BufferedStream(uint8_t* data, int size) : memory(data), memory_size(size) {}
};
