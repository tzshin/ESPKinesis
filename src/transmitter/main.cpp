#include <Arduino.h>

#include <PPMReader.h>
#include <WiFi.h>
#include <esp_now.h>

#include <TargetManager.h>
#include <NonBlockingTimer.h>

// --- PPM Configuration ---
static constexpr int PPM_PIN = 3;
static constexpr int PPM_CHANNEL_COUNT = 8;
PPMReader ppm_reader(PPM_PIN, PPM_CHANNEL_COUNT);

// --- Loop Rate Configuration ---
static constexpr int SEND_FREQ_HZ = 40; // Transmit at 20 Hz
static constexpr unsigned long SEND_PERIOD_US = 1000000UL / SEND_FREQ_HZ;
NonBlockingTimer send_timer(SEND_PERIOD_US);

// --- ESP-NOW / TargetManager Configuration ---
const uint8_t BROADCAST_ADDRS[][6] = {{0xb0, 0x81, 0x84, 0x06, 0x0e, 0xf0}};
static constexpr size_t NUM_BROADCAST_ADDRS =
    sizeof(BROADCAST_ADDRS) / sizeof(BROADCAST_ADDRS[0]);
tmanager::TargetManager target_manager;

// --- ESP-NOW Callback ---
void on_data_sent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
  char mac_str[18];
  snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4],
           mac_addr[5]);
  Serial.print("Packet to ");
  Serial.print(mac_str);
  Serial.print(" send status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

// --- Update Target Data from PPM ---
void update_channels()
{
  for (auto &target : target_manager.get_targets())
  {
    for (int i = 0; i < tmanager::TARGET_CHANNEL_COUNT; i++)
    {
      target.data.channels[i] = ppm_reader.rawChannelValue(i + 1);
    }
  }
}

// --- Send Target Data via ESP-NOW ---
void send_channels()
{
  for (auto &target : target_manager.get_targets())
  {
    esp_err_t result =
        esp_now_send(target.mac, (uint8_t *)&target.data, sizeof(tmanager::ChannelData));

    if (result == ESP_OK)
    {
      target.connection_state = true;
      target.last_successful_send = micros();
    }
    else
    {
      target.connection_state = false;
    }
  }
}

void setup()
{
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK)
  {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  esp_now_register_send_cb(on_data_sent);

  // Add each broadcast address as a target (auto-assigned IDs)
  for (size_t i = 0; i < NUM_BROADCAST_ADDRS; i++)
  {
    target_manager.add_target(BROADCAST_ADDRS[i], "Drone_" + String(i + 1));
  }
  // Register each target as an ESP-NOW peer
  for (const auto &target : target_manager.get_targets())
  {
    esp_now_peer_info_t peer_info = {};
    peer_info.channel = 0;
    peer_info.encrypt = false;
    memcpy(peer_info.peer_addr, target.mac, 6);
    if (esp_now_add_peer(&peer_info) != ESP_OK)
    {
      Serial.print("Failed to add peer for ");
      Serial.println(target.name);
    }
  }

  // Set PPM error tolerance
  ppm_reader.channelValueMaxError = 50;
}

void loop()
{
  update_channels();

  if (send_timer.is_ringing())
  {
    send_timer.reset();
    send_channels();
  }
}