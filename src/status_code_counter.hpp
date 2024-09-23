#include <array>
#include <sstream>
#include <string>

class StatusCodeCounter {
 private:
  int* arr;
  uint8_t size;

 public:
  StatusCodeCounter(int array[], uint8_t size) : arr(array), size(size) {}

  void add(int num) {
    for (int i = size - 1; i >= 0; i--) {
      arr[i] = arr[i - 1];
    }

    arr[0] = num;
  }

  bool last_n_have_status(uint8_t n, int status) {
    if (n >= size) {
      return false;
    }

    for (int i = 0; i < n; i++) {
      if (arr[i] != status) {
        return false;
      }
    }
    return true;
  }

  std::string to_string() {
    std::ostringstream oss;
    for (int i = 0; i < size; i++) {
      oss << arr[i] << " ";
    }
    std::string str = oss.str();
    return str.substr(0, str.length() - 1);  // Get rid of trailing space
  }
};