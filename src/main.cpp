#ifndef BOARD_HAS_PSRAM
#error "Please enable PSRAM !!!"
#endif

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <FFat.h>
#include <FS.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

#include "epaper_config.h"
#include "epd_driver.h"
#include "esp_adc_cal.h"
#include "network.hpp"
#include "opensans16.h"
#include "pins.h"
#include "response_parser.h"
#include "screen_io.h"
#include "status_code_counter.hpp"

#define FILE_SYSTEM FFat
#define DBG_OUTPUT_PORT Serial
#define STATUS_CODE_HISTORY 5

extern const uint8_t rootca_crt_bundle_start[] asm(
    "_binary_data_cert_x509_crt_bundle_bin_start");

const int vref = 1100;

// rtc data fields are stored in rtc memory which will not get lost after
// deep sleep.
RTC_DATA_ATTR uint32_t image_id = 0;
RTC_DATA_ATTR uint32_t wakeup_count = 0;
RTC_DATA_ATTR uint32_t error_count = 0;
RTC_DATA_ATTR int _status_codes[STATUS_CODE_HISTORY] = {0, 0, 0, 0, 0};
StatusCodeCounter status_codes(_status_codes, STATUS_CODE_HISTORY);

float current_voltage;
String device_id;  // derived from mac adress
String device_token = "";

float read_battery() {
  // When reading the battery voltage, POWER_EN must be turned on
  epd_poweron();
  delay(10);  // Make adc measurement more accurate
  uint16_t v = analogRead(BATT_PIN);
  return ((float)v / 4095.0) * 2.0 * 3.3 * (vref / 1000.0);
}

net_state_t request_device_image(uint32_t *imageId, uint32_t *sleepTime) {
  NetworkClient client;
  client.addImageIdHeader(image_id);
  client.addAcceptVersionHeader(SUPPORTED_VERSIONS);
  client.addVoltageHeader(current_voltage);
  client.addWakeupCountHeader(wakeup_count);
  client.addAuthorizationHeader(device_token);

  int httpCode = client.GET(server_url);

  // Track status codes of recent requests to trigger actions on certain
  // repeated codes
  status_codes.add(httpCode);

  if (httpCode == 200) {
    epd_poweron();
    auto responseStream = client.getStreamPtr();
    size_t response_length = client.getSize();
    write_text("Got response with content length: " + String(response_length));

    HttpStream stream(responseStream, response_length);
    net_state_t response = process_stream(&stream, imageId, sleepTime);
    epd_poweroff();
    return response;
  } else {
    if (httpCode < 0) {
      write_error(client.errorToString(httpCode));
    } else {
      write_error("Status code " + String(httpCode) + ": " +
                  client.getString());
    }
    return UNEXPECTED_STATUS_CODE;
  }
}

net_state_t request_device_token(Preferences preferences) {
  print_on_display = true;
  epd_clear();
  reset_text_cursor();
  write_text("Request device token");

  NetworkClient client;
  client.addVoltageHeader(current_voltage);
  client.addWakeupCountHeader(wakeup_count);
  client.addAuthorizationHeader(device_token);

  int httpCode = client.GET(server_url + "/token");

  if (httpCode == 200) {
    JsonDocument doc;
    write_text("Received successful response from server");
    deserializeJson(doc, client.getString());

    cursor_y += 20;
    write_text("Device: " + String((const char *)doc["deviceId"]));
    write_text("Challenge: " + String((const char *)doc["challenge"]));
    cursor_y += 20;

    write_text("Please approve this token on the server.");

    device_token = String((const char *)doc["token"]);
    preferences.putString("token", device_token);
    return SUCCESS;
  } else {
    write_error("Status code " + String(httpCode) + ": " + client.getString());
    return UNEXPECTED_STATUS_CODE;
  }
}

uint32_t get_sleep_time_for_error() {
  uint32_t sleep_time_in_s;
  switch (error_count) {
    case 0:
      sleep_time_in_s = 30;
      break;

    case 1:
      sleep_time_in_s = 30;
      break;

    case 2:
      sleep_time_in_s = 30;  // 1 * 60 * 60;
      break;

    default:
      sleep_time_in_s = 12 * 60 * 60;
      break;
  }

  error_count++;
  return sleep_time_in_s;
}

void setup() {
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  bool is_wakeup_from_deepsleep = (wakeup_count > 0);
  if (!is_wakeup_from_deepsleep) {
    Serial.begin(115200);
  }

  wakeup_count++;

  epd_init();
  current_voltage = read_battery();

  if (!is_wakeup_from_deepsleep) {
    // We do not want to print anything on the display when leaving deepsleep so
    // we do not destroy the current image
    epd_clear();
    print_on_display = true;
  }

  device_id = NetworkClient::getDeviceIdFromMac();
  write_header(device_id);

  write_text("Starting system");
  write_text("Voltage: " + String(current_voltage) + "V");
  write_text("Connecting to wifi");
  write_text("Wifi SSID: " + String(wifi_ssid), 60);

  uint32_t sleep_time_in_s = 0;

  wl_status_t status = NetworkClient::startWifi(wifi_ssid, wifi_password);
  write_text("Connection status: " + String(status), 60);
  if (status == WL_CONNECTED) {
    write_text("Wifi signal strength: " + String(WiFi.RSSI()) + "dbm", 60);
    write_text("Assigned IP: " + WiFi.localIP().toString(), 60);

    Preferences preferences;
    preferences.begin("epaper", false);
    device_token = preferences.getString("token", device_token);

    if (device_token.equals("")) {
      request_device_token(preferences);
      write_text("Device will go to sleep, restart manually");
      sleep_time_in_s = 30 * 60;
    } else {
      if (request_device_image(&image_id, &sleep_time_in_s) == SUCCESS) {
        error_count = 0;
      } else if (status_codes.last_n_have_status(3, 401)) {
        // If the last three occurrences of status codes are 401, our token
        // seems to be invalidated. Reset the stored token.
        request_device_token(preferences);
        write_text("Device will go to sleep, restart manually");
        sleep_time_in_s = 30 * 60;
      }
    }

    preferences.end();
  } else {
    write_error("Could not connect to wifi network, check credentials!");
  }

  // If an error occurs we cannot receive the sleep time from the server and
  // must set it to sensible defaults.
  if (sleep_time_in_s <= 0) {
    image_id = 0;
    sleep_time_in_s = get_sleep_time_for_error();
  }

  NetworkClient::stopWifi();
  Serial.end();
  esp_sleep_enable_timer_wakeup(sleep_time_in_s * 1000 * 1000);
  epd_poweroff_all();
  esp_deep_sleep_start();
}

void loop() {
  // not used, when waking from deepsleep the main setup function will be called
}
