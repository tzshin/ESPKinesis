#include <Arduino.h>

#include <WiFi.h>
#include <esp_wifi.h>

void read_mac_address() {
  uint8_t base_mac[6];
  esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, base_mac);
  if (ret == ESP_OK) {
    Serial.printf("%02x:%02x:%02x:%02x:%02x:%02x\n", base_mac[0], base_mac[1],
                  base_mac[2], base_mac[3], base_mac[4], base_mac[5]);
  } else {
    Serial.println("Failed to read MAC address");
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
}

void loop() {
  Serial.print("[DEFAULT] ESP32 Board MAC Address: ");
  read_mac_address();
  delay(500);
}