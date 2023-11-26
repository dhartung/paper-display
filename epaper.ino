#ifndef BOARD_HAS_PSRAM
#error "Please enable PSRAM !!!"
#endif

#include <Arduino.h>
#include <ESPmDNS.h>
#include <FFat.h>
#include <FS.h>
#include <HTTPClient.h>
#include <SD.h>
#include <SPI.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

#include "epaper_config.h"
#include "epd_driver.h"
#include "esp_adc_cal.h"
#include "opensans16.h"
#include "pins.h"
#include "response_parser.h"
#include "screen_io.h"

#define FILE_SYSTEM FFat

#define DBG_OUTPUT_PORT Serial

const int vref = 1100;

// rtc data fields are stored in rtc memory which will not get lost after
// deep sleep.
RTC_DATA_ATTR uint32_t image_id = 0;
RTC_DATA_ATTR uint32_t wakeup_count = 0;

float current_voltage;

wl_status_t start_wifi(const char *ssid, const char *password) {
  DBG_OUTPUT_PORT.println("\r\nConnecting to: " + String(ssid));
  IPAddress dns(8, 8, 8, 8);  // Use Google DNS
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);  // switch off AP
  WiFi.setAutoConnect(true);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    write_text("STA failed, retrying", 60);
    WiFi.disconnect(false);
    delay(500);
    WiFi.begin(ssid, password);
  }
  return WiFi.status();
}

void stop_wifi() {
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
}

float read_battery() {
  // When reading the battery voltage, POWER_EN must be turned on
  epd_poweron();
  delay(10);  // Make adc measurement more accurate
  uint16_t v = analogRead(BATT_PIN);
  return ((float)v / 4095.0) * 2.0 * 3.3 * (vref / 1000.0);
}

net_state_t send_request(uint32_t *imageId, uint32_t *sleepTime) {
  WiFiClientSecure client;
  client.stop();  // close connection before sending a new request
  HTTPClient http;
  client.setInsecure();
  http.begin(client, String(server_url) + "?version=" + String(*imageId) +
                         "&voltage=" + String(current_voltage) + "&wakeups=" +
                         String(wakeup_count) + "&key=" + String(device_key));
  int httpCode = http.GET();

  if (httpCode == 200) {
    epd_poweron();
    auto responseStream = http.getStreamPtr();
    write_text("Got response with content length: " + String(http.getSize()));

    net_state_t response = process_stream(responseStream, imageId, sleepTime);
    epd_poweroff();
    return response;
  } else {
    write_error("Got unexpected status code " + String(httpCode));
    return UNEXPECTED_STATUS_CODE;
  }
}

void setup() {
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  bool is_wakeup_from_deepsleep = (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER ||
                                   wakeup_reason == ESP_SLEEP_WAKEUP_ULP ||
                                   wakeup_reason == ESP_SLEEP_WAKEUP_EXT0);

  if (!is_wakeup_from_deepsleep) {
    Serial.begin(115200);
  } else {
    wakeup_count++;
  }

  epd_init();
  current_voltage = read_battery();

  if (!is_wakeup_from_deepsleep) {
    // We do not want to print anything on the display when leaving deepsleep so
    // we do not destroy the current image
    epd_clear();
    print_on_display = true;
  }

  write_text("Starting system");
  write_text("Connecting to wifi");
  write_text("Wifi SSID: " + String(wifi_ssid), 60);

  uint32_t sleep_time_in_s;

  wl_status_t status = start_wifi(wifi_ssid, wifi_password);
  write_text("Connection status: " + String(status), 60);
  if (status == WL_CONNECTED) {
    write_text("Wifi signal strength: " + String(WiFi.RSSI()) + "dbm", 60);
    write_text("Assigned IP: " + WiFi.localIP().toString(), 60);
    write_text("Start application loop");

    if (send_request(&image_id, &sleep_time_in_s) != SUCCSESS) {
      sleep_time_in_s = 2 * 60 * 60;
    }
    stop_wifi();
  } else {
    write_error("Could not connect to wifi network, check credentials!");
    sleep_time_in_s = 2 * 60 * 60;
  }

  Serial.end();
  esp_sleep_enable_timer_wakeup(sleep_time_in_s * 1000 * 1000);
  epd_poweroff_all();
  esp_deep_sleep_start();
}

void loop() {
  // not used, when waking from deepsleep the main setup function will be called
}
