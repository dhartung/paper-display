#include <WString.h>

String get_hex(uint8_t b) {
  String s = String(b, 16);
  if (s.length() <= 0) {
    return "00";
  }

  if (s.length() <= 1) {
    return "0" + s;
  }
  
  return s;
}