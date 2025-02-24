#include <Arduino.h>

#include <PPMReader.h>
#include <WiFi.h>
#include <esp_now.h>

#include <TargetManager.h>

// --- Universal Channel Data Definition ---
struct ChannelData {
  uint16_t channels[tmanager::TARGET_CHANNEL_COUNT];
};

// --- PPM Configuration ---
static constexpr int PPM_PIN = 0;
static constexpr int PPM_CHANNEL_COUNT = 12;
PPMReader ppm(PPM_PIN, PPM_CHANNEL_COUNT);

// --- Loop Rate Configuration ---
static constexpr int LOOP_FREQ_HZ = 40; // Transmit at 20 Hz
static constexpr unsigned long LOOP_PERIOD_US = 1000000UL / LOOP_FREQ_HZ;

// --- ESP-NOW / TargetManager Configuration ---
const uint8_t BROADCAST_ADDRS[][6] = {{0x70, 0x04, 0x1D, 0x30, 0x64, 0x14},
                                      {0x70, 0x04, 0x1D, 0x30, 0x6B, 0x90}};
static constexpr size_t NUM_BROADCAST_ADDRS =
    sizeof(BROADCAST_ADDRS) / sizeof(BROADCAST_ADDRS[0]);
tmanager::TargetManager target_manager;

// --- ESP-NOW Callback ---
void on_data_sent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  char mac_str[18];
  snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4],
           mac_addr[5]);
  Serial.print("Packet to ");
  Serial.print(mac_str);
  Serial.print(" send status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

// --- Loop Rate Regulation ---
void loop_at_freq(int freq, unsigned long start_time) {
  unsigned long period_us = 1000000UL / freq;
  while ((micros() - start_time) < period_us) {
    // Busy-wait until the period has elapsed.
  }
}

// --- Upload Channels from PPM ---
ChannelData upload_channels() {
  ChannelData data;
  // Read channels 1 through CHANNEL_COUNT (PPM channels are 1-indexed)
  for (int i = 1; i <= tmanager::TARGET_CHANNEL_COUNT; i++) {
    data.channels[i - 1] = ppm.rawChannelValue(i);
  }
  return data;
}

// --- Send Universal Data via ESP-NOW ---
void send_channels(const ChannelData &data) {
  for (auto &target : target_manager.get_targets()) {
    esp_err_t result =
        esp_now_send(target.mac, (uint8_t *)&data, sizeof(ChannelData));
    if (result == ESP_OK) {
      Serial.print("Sent to ");
      Serial.println(target.name);
    } else {
      Serial.print("Error sending to ");
      Serial.println(target.name);
    }
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  esp_now_register_send_cb(on_data_sent);

  // Add each broadcast address as a target (auto-assigned IDs)
  for (size_t i = 0; i < NUM_BROADCAST_ADDRS; i++) {
    target_manager.add_target(BROADCAST_ADDRS[i], "Drone_" + String(i + 1));
  }
  // Register each target as an ESP-NOW peer
  for (const auto &target : target_manager.get_targets()) {
    esp_now_peer_info_t peer_info = {};
    peer_info.channel = 0;
    peer_info.encrypt = false;
    memcpy(peer_info.peer_addr, target.mac, 6);
    if (esp_now_add_peer(&peer_info) != ESP_OK) {
      Serial.print("Failed to add peer for ");
      Serial.println(target.name);
    }
  }

  // Set PPM error tolerance
  ppm.channelValueMaxError = 50;
}

int i = 0;

void loop() {
  unsigned long loop_start = micros();

  if (Serial) {
    Serial.printf("i: %d\n", i);
    Serial.println(i++);
  }

  ChannelData data = upload_channels();
  send_channels(data);

  loop_at_freq(LOOP_FREQ_HZ, loop_start);
}