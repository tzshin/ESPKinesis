#include <Arduino.h>

#include <WiFi.h>
#include <esp_now.h>
#include <sbus.h>

#include <TargetManager.h>
#include <NonBlockingTimer.h>

// --- Universal Channel Data Definition ---
struct ChannelData
{
  uint16_t channels[tmanager::TARGET_CHANNEL_COUNT];
}received_data;

// --- SBUS Configuration ---
static constexpr int SBUS_TX_PIN = 5;
bfs::SbusTx sbus_tx(&Serial0, -1, SBUS_TX_PIN, true);
bfs::SbusData sbus_data;
static constexpr int SBUS_SEND_HZ = 30;
static constexpr unsigned long SBUS_SEND_PERIOD_US = 1000000UL / SBUS_SEND_HZ;
NonBlockingTimer sbus_send_timer(SBUS_SEND_PERIOD_US);

// --- ESP-NOW Receive Callback ---
// This callback receives universal channel data, converts it to SBUS format,
// and outputs via SBUS.
void on_data_recv(const uint8_t *mac_addr, const uint8_t *incoming_data,
                  int len)
{
  if (len != sizeof(ChannelData))
  {
    Serial.print("Unexpected data length: ");
    Serial.println(len);
    return;
  }
  memcpy(&received_data, incoming_data, sizeof(ChannelData));

  // Optionally, print the received data
  Serial.print("Received channel data:\n");
  for (int i = 0; i < tmanager::TARGET_CHANNEL_COUNT; i++)
  {
    Serial.printf(">channel[%d]: %d\n", i, received_data.channels[i]);
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
  esp_now_register_recv_cb(on_data_recv);

  // Initialize SBUS transmitter
  sbus_tx.Begin();
}

void loop()
{
  if (sbus_send_timer.is_ringing())
  {
    sbus_send_timer.reset();
    
    for (int i = 0; i < tmanager::TARGET_CHANNEL_COUNT; i++)
    {
      sbus_data.ch[i] = map(received_data.channels[i], 1000, 2000, 172, 1811);
    }
    sbus_tx.data(sbus_data);
    sbus_tx.Write();
  }
}