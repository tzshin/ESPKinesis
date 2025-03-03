#include <Arduino.h>

#include <PPMReader.h>
#include <WiFi.h>
#include <esp_now.h>
#include <ArduinoJson.h>

#include <TargetManager.h>
#include <NonBlockingTimer.h>

// --- PPM Configuration ---
static constexpr int PPM_PIN = 3;
static constexpr int PPM_CHANNEL_COUNT = 8;
PPMReader ppm_reader(PPM_PIN, PPM_CHANNEL_COUNT);

// --- JSON Configuration ---
static constexpr size_t JSON_SIZE = 1024; // Adjust based on your needs
static constexpr int SERIAL_BAUD = 115200;
static constexpr int JSON_SEND_HZ = 2;
static constexpr unsigned long JSON_SEND_PERIOD_US = 1000000UL / JSON_SEND_HZ;
NonBlockingTimer json_send_timer(1000000); // Send status every 1 second

// --- Loop Rate Configuration ---
static constexpr int RADIO_SEND_FREQ_HZ = 10;
static constexpr unsigned long RADIO_SEND_PERIOD_US = 1000000UL / RADIO_SEND_FREQ_HZ;
NonBlockingTimer radio_send_timer(RADIO_SEND_PERIOD_US);

// --- ESP-NOW / TargetManager Configuration ---
const uint8_t BROADCAST_ADDRS[][6] = {{0xb0, 0x81, 0x84, 0x06, 0x0e, 0xf0}};
static constexpr size_t NUM_BROADCAST_ADDRS =
    sizeof(BROADCAST_ADDRS) / sizeof(BROADCAST_ADDRS[0]);
tmanager::TargetManager target_manager;

// --- ESP-NOW Callback ---
void on_radio_send(const uint8_t *mac_addr, esp_now_send_status_t status)
{
  char mac_str[18];
  snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4],
           mac_addr[5]);
  // Serial.print("Packet to ");
  // Serial.print(mac_str);
  // Serial.print(" send status: ");
  // Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");

  auto *target = target_manager.get_target_by_mac(mac_addr);
  if (target)
  {
    target->connection_state = status == ESP_NOW_SEND_SUCCESS;
    if (target->connection_state)
    {
      target->last_successful_send = micros();
    }
  }
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
void send_radio()
{
  for (auto &target : target_manager.get_targets())
  {
    esp_now_send(target.mac, (uint8_t *)&target.data, sizeof(tmanager::ChannelData));
  }
}

// --- JSON Command Processing ---
/**
 * Process a JSON command received from the control panel
 * @param json_string The JSON string to process
 */
void parse_json(const String &json_string)
{
  StaticJsonDocument<JSON_SIZE> doc;
  DeserializationError error = deserializeJson(doc, json_string);

  if (error)
  {
    // Create error response
    StaticJsonDocument<JSON_SIZE> error_doc;
    error_doc["type"] = "error";
    error_doc["message"] = String("JSON parsing error: ") + error.c_str();

    String response;
    serializeJson(error_doc, response);
    Serial.println(response);
    return;
  }

  // Get command type
  const char *cmd_type = doc["command"];
  if (!cmd_type)
  {
    // Create error response for missing command
    StaticJsonDocument<JSON_SIZE> error_doc;
    error_doc["type"] = "error";
    error_doc["message"] = "Missing 'command' field in JSON";

    String response;
    serializeJson(error_doc, response);
    Serial.println(response);
    return;
  }

  // Process commands
  StaticJsonDocument<JSON_SIZE> response_doc;
  response_doc["type"] = "response";
  response_doc["command"] = cmd_type;

  String cmd = String(cmd_type);

  if (cmd == "override_channels")
  {
  }
  else
  {
    response_doc["status"] = "error";
    response_doc["message"] = "Unknown command: " + String(cmd_type);
  }

  // Send the response
  String response;
  serializeJson(response_doc, response);
  Serial.println(response);
}

/**
 * Send the current system status as JSON
 */
void send_json()
{
  StaticJsonDocument<JSON_SIZE> doc;

  doc["type"] = "target_state";
  doc["targets"] = target_manager.get_all_targets_json();

  String json_string;
  serializeJson(doc, json_string);
  Serial.println(json_string);
}

void setup()
{
  Serial.begin(SERIAL_BAUD);
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK)
  {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  esp_now_register_send_cb(on_radio_send);

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

/**
 * Process any available serial input as JSON commands
 */
void process_serial_input()
{
  if (Serial.available())
  {
    String input = Serial.readStringUntil('\n');
    input.trim();

    if (input.length() > 0)
    {
      parse_json(input);
    }
  }
}

void loop()
{
  // Process any serial input
  process_serial_input();

  // Update channel data from PPM input
  update_channels();

  // Send channel data to targets at configured frequency
  if (radio_send_timer.is_ringing())
  {
    radio_send_timer.reset();
    send_radio();
  }

  // Periodically send status update via JSON
  if (json_send_timer.is_ringing())
  {
    json_send_timer.reset();
    send_json();
  }
}