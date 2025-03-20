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
static constexpr int SERIAL_BAUD = 115200;
static constexpr int JSON_UPDATE_HZ = 2;
static constexpr unsigned long JSON_UPDATE_PERIOD_US = 1000000UL / JSON_UPDATE_HZ;
NonBlockingTimer json_update_timer(JSON_UPDATE_PERIOD_US);

// --- Loop Rate Configuration ---
static constexpr int RADIO_SEND_FREQ_HZ = 2;
static constexpr unsigned long RADIO_SEND_PERIOD_US = 1000000UL / RADIO_SEND_FREQ_HZ;
NonBlockingTimer radio_send_timer(RADIO_SEND_PERIOD_US);

// --- Command Handling ---
typedef bool (*CommandHandler)(const JsonDocument &, JsonDocument &);
struct CommandEntry
{
  const char *name;
  CommandHandler handler;
};
bool handle_override_channels(const JsonDocument &doc, JsonDocument &response_doc);
const CommandEntry COMMAND_REGISTRY[] = {
    {"override_channels", handle_override_channels},
    // Add additional commands here
};
const size_t COMMAND_COUNT = sizeof(COMMAND_REGISTRY) / sizeof(CommandEntry);

// --- ESP-NOW / TargetManager Configuration ---
const uint8_t BROADCAST_ADDRS[][6] = {{0xb0, 0x81, 0x84, 0x03, 0x9f, 0x74},
                                      {0xb0, 0x81, 0x84, 0x03, 0xa5, 0xf0},
                                      {0xb0, 0x81, 0x84, 0x06, 0x12, 0xa0},
                                      {0x18, 0x8b, 0x0e, 0x91, 0xac, 0xac},
                                      {0x18, 0x8b, 0x0e, 0x93, 0x48, 0xc0},
                                      {0xb0, 0x81, 0x84, 0x03, 0xa1, 0xc4},
                                      {0xb0, 0x81, 0x84, 0x06, 0x03, 0xa8},
                                      {0xb0, 0x81, 0x84, 0x06, 0x07, 0xbc}};
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

// --- Update Targets Channels Data ---
void update_channels()
{
  unsigned long current_time = micros();

  for (auto &target : target_manager.get_targets())
  {
    if (target.is_channels_overridden && current_time >= target.override_timeout)
    {
      target.is_channels_overridden = false;
    }

    if (!target.is_channels_overridden)
    {
      for (int i = 0; i < tmanager::TARGET_CHANNEL_COUNT; i++)
      {
        target.data.channels[i] = ppm_reader.rawChannelValue(i + 1);
      }
    }
  }
}

// --- Send Target Data via ESP-NOW ---
void send_espnow_radio()
{
  for (auto &target : target_manager.get_targets())
  {
    esp_now_send(target.mac, (uint8_t *)&target.data, sizeof(tmanager::ChannelData));
  }
}

/**
 * Sends a JSON response to the serial port
 * @param response_doc The document to serialize and send
 */
void send_json_response(const JsonDocument &response_doc)
{
  String response;
  serializeJson(response_doc, response);
  Serial.println(response);
}

/**
 * Creates and sends an error response
 * @param error_type The type of error
 * @param error_message The error message
 * @param command Optional command that caused the error
 */
void send_error_response(
    const char *error_type,
    const String &error_message,
    const char *command = nullptr)
{
  JsonDocument error_doc;
  error_doc["type"] = error_type;
  error_doc["message"] = error_message;

  if (command)
  {
    error_doc["command"] = command;
  }

  send_json_response(error_doc);
}

/**
 * Handles the override_channels command
 * @param doc The command document
 * @param response_doc The response document to fill
 * @return true if successful, false otherwise
 */
bool handle_override_channels(const JsonDocument &doc, JsonDocument &response_doc)
{
  // Validate required fields
  if (!doc["target_id"].is<int>() || !doc["channels"].is<JsonArrayConst>() || !doc["duration"].is<unsigned long>())
  {
    response_doc["status"] = "error";
    response_doc["message"] = "Missing required fields: target_id, channels, and/or duration";
    return false;
  }

  int target_id = doc["target_id"];
  JsonArrayConst channels = doc["channels"].as<JsonArrayConst>();
  unsigned long duration_ms = doc["duration"].as<unsigned long>();

  // Find the target by ID
  auto *target = target_manager.get_target_by_id(target_id);
  if (!target)
  {
    response_doc["status"] = "error";
    response_doc["message"] = "Target not found with ID: " + String(target_id);
    return false;
  }

  // Validate channels array
  if (channels.size() == 0 || channels.size() > tmanager::TARGET_CHANNEL_COUNT)
  {
    response_doc["status"] = "error";
    response_doc["message"] = "Invalid channel count. Expected 1-" +
                              String(tmanager::TARGET_CHANNEL_COUNT) +
                              ", got " + String(channels.size());
    return false;
  }

  // Validate duration
  if (duration_ms < 1)
  {
    response_doc["status"] = "error";
    response_doc["message"] = "Duration must be at least 1ms";
    return false;
  }

  // Update channels from the received data
  for (size_t i = 0; i < channels.size(); i++)
  {
    if (!channels[i].is<int>())
    {
      response_doc["status"] = "error";
      response_doc["message"] = "Channel values must be integers";
      return false;
    }

    int value = channels[i];

    if (value == -1)
    {
      continue;
    }
    else if (value < 1000 || value > 2000)
    {
      response_doc["status"] = "error";
      response_doc["message"] = "Channel values must be between 1000-2000 or -1 to skip";
      return false;
    }

    target->data.channels[i] = value;
  }

  // Set the override flag and timeout
  target->is_channels_overridden = true;
  target->override_timeout = micros() + (duration_ms * 1000); // Convert ms to microseconds

  response_doc["status"] = "success";
  response_doc["message"] = "Channels updated for target " + String(target_id) +
                            " with " + String(duration_ms) + "ms timeout";
  return true;
}

/**
 * Process a JSON command received from the control panel
 * @param json_string The JSON string to process
 */
void parse_json(const String &json_string)
{
  // Parse the incoming JSON
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, json_string);

  // Handle parsing errors
  if (error)
  {
    send_error_response("error", String("JSON parsing error: ") + error.c_str());
    return;
  }

  // Validate command field
  if (!doc["command"].is<const char *>())
  {
    send_error_response("error", "Missing 'command' field in JSON");
    return;
  }

  const char *command = doc["command"];

  // Prepare response
  JsonDocument response_doc;
  response_doc["type"] = "response";
  response_doc["command"] = command;

  // Find and execute the command handler
  bool command_found = false;
  for (size_t i = 0; i < COMMAND_COUNT; i++)
  {
    if (strcmp(command, COMMAND_REGISTRY[i].name) == 0)
    {
      command_found = true;
      COMMAND_REGISTRY[i].handler(doc, response_doc);
      break;
    }
  }

  // Handle unknown commands
  if (!command_found)
  {
    response_doc["status"] = "error";
    response_doc["message"] = "Unknown command: " + String(command);
  }

  // Send the response
  send_json_response(response_doc);
}

/**
 * Send the current system status as JSON
 */
void send_targets_update()
{
  JsonDocument doc;
  JsonDocument targets_doc;

  // Parse the pre-formatted JSON string from get_all_targets_json
  deserializeJson(targets_doc, target_manager.get_all_targets_json());

  doc["type"] = "targets_update";
  doc["targets"] = targets_doc.as<JsonArrayConst>(); // Add as a proper JSON array

  String json_string;
  serializeJson(doc, json_string);
  Serial.println(json_string);
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

void loop()
{
  // Process any serial input
  process_serial_input();

  // Update targets channels data
  update_channels();

  // Send channel data to targets at configured frequency
  if (radio_send_timer.is_ringing())
  {
    radio_send_timer.reset();
    send_espnow_radio();
  }

  // Periodically send status update via JSON
  if (json_update_timer.is_ringing())
  {
    json_update_timer.reset();
    send_targets_update();
  }
}