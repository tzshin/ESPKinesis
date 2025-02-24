#include <Arduino.h>

#include <WiFi.h>
#include <esp_now.h>
#include <sbus.h>

#include <TargetManager.h>

// --- Universal Channel Data Definition ---
struct ChannelData {
  uint16_t channels[tmanager::TARGET_CHANNEL_COUNT];
};

// --- SBUS Configuration ---
static constexpr int SBUS_TX_PIN = 4;
bfs::SbusTx sbus_tx(&Serial0, -1, SBUS_TX_PIN, true);
bfs::SbusData sbus_data;

// --- ESP-NOW Receive Callback ---
// This callback receives universal channel data, converts it to SBUS format,
// and outputs via SBUS.
void on_data_recv(const uint8_t *mac_addr, const uint8_t *incoming_data,
                  int len) {
  if (len != sizeof(ChannelData)) {
    Serial.print("Unexpected data length: ");
    Serial.println(len);
    return;
  }
  ChannelData received_data;
  memcpy(&received_data, incoming_data, sizeof(ChannelData));

  // Convert universal data to SBUS data
  for (int i = 0; i < tmanager::TARGET_CHANNEL_COUNT; i++) {
    sbus_data.ch[i] = map(received_data.channels[i], 1000, 2000, 172, 1811);
  }
  sbus_tx.data(sbus_data);
  // Transmit via SBUS
  sbus_tx.Write();

  // Optionally, print the received data
  Serial.print("Received channel data:\n");
  for (int i = 0; i < tmanager::TARGET_CHANNEL_COUNT; i++) {
    Serial.printf(">channel[%d]: %d\n", i, received_data.channels[i]);
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  esp_now_register_recv_cb(on_data_recv);

  // Initialize SBUS transmitter
  sbus_tx.Begin();
}

void loop() {
  // Nothing to do here; processing is handled in the ESP-NOW callback.
}