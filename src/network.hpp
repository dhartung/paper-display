
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

extern const uint8_t rootca_crt_bundle_start[] asm(
    "_binary_data_cert_x509_crt_bundle_bin_start");

class NetworkClient {
 private:
  WiFiClientSecure client;
  HTTPClient http;

  static String get_hex(uint8_t b) {
    String s = String(b, 16);
    if (s.length() <= 0) {
      return "00";
    }

    if (s.length() <= 1) {
      return "0" + s;
    }

    return s;
  }

 public:
  int GET(String url) {
    // stop possible previous connections
    client.stop();
    client.setCACertBundle(rootca_crt_bundle_start);
    client.setTimeout(10000);

    addWifiSignalHeader();
    addDeviceIdHeader();

    http.begin(client, url);
    return http.GET();
  }

  String getString() { return http.getString(); }

  WiFiClient *getStreamPtr() { return http.getStreamPtr(); }

  int getSize() { return http.getSize(); }

  void addVoltageHeader(float voltage) {
    http.addHeader("Voltage", String(voltage));
  }

  void addWakeupCountHeader(uint32_t wakeup_count) {
    http.addHeader("Wakeup-Count", String(wakeup_count));
  }

  void addAuthorizationHeader(String token) {
    http.addHeader("Authorization", token);
  }

  void addDeviceIdHeader() {
    http.addHeader("Device-Id", getDeviceIdFromMac());
  }

  void addWifiSignalHeader() {
    http.addHeader("Wifi-Signal", String(WiFi.RSSI()));
  }

  void addImageIdHeader(uint8_t image_id) {
    http.addHeader("Image-Id", String(image_id));
  }

  void addAcceptVersionHeader(String versions) {
    http.addHeader("Accept-Version", versions);
  }

  String errorToString(int statusCode) {
    if (statusCode < 0) {
      return http.errorToString(statusCode);
    } else {
      return "unexpected status code: " + String(statusCode);
    }
  }

  static String getDeviceIdFromMac() {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    return get_hex(mac[3]) + get_hex(mac[4]) + get_hex(mac[5]);
  }

  static wl_status_t startWifi(const char *ssid, const char *password) {
    IPAddress dns(8, 8, 8, 8);  // Use Google DNS
    WiFi.disconnect();
    WiFi.mode(WIFI_STA);  // switch off AP
    WiFi.setAutoConnect(true);
    WiFi.setAutoReconnect(true);
    WiFi.begin(ssid, password);
    if (WiFi.waitForConnectResult() != WL_CONNECTED) {
      WiFi.disconnect(false);
      delay(500);
      WiFi.begin(ssid, password);
    }
    return WiFi.status();
  }

  static void stopWifi() {
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
  }
};