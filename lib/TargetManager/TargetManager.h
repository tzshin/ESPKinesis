#ifndef TARGET_MANAGER_H
#define TARGET_MANAGER_H

#include <Arduino.h>

#include <stdint.h>
#include <vector>

namespace tmanager {

// Define a constant for the number of channels.
static constexpr uint8_t TARGET_CHANNEL_COUNT = 8;

// Structure to hold individual target information.
struct Target {
  uint8_t id;     // Automatically assigned ID (order of addition)
  uint8_t mac[6]; // Receiver MAC address
  uint16_t channels[TARGET_CHANNEL_COUNT];   // Current channel outputs
  bool connection_state;              // ESP-NOW send success flag
  unsigned long last_successful_send; // Timestamp of the last successful send
  String name;                        // Optional human-readable name
};

class TargetManager {
public:
  TargetManager() : next_id(1) {}

  // Add a new target with an automatically assigned ID.
  void add_target(const uint8_t mac[6], const String &name = "") {
    Target new_target;
    new_target.id = next_id++;
    memcpy(new_target.mac, mac, 6);
    for (int i = 0; i < TARGET_CHANNEL_COUNT; i++) {
      new_target.channels[i] = 0;
    }
    new_target.connection_state = false;
    new_target.last_successful_send = 0;
    new_target.name = name;
    targets.push_back(new_target);
  }

  // Retrieve a target by its assigned ID.
  const Target *get_target_by_id(uint8_t id) const {
    for (const auto &target : targets) {
      if (target.id == id) {
        return &target;
      }
    }
    return nullptr;
  }

  // Retrieve a target by its index in the list.
  const Target *get_target_by_index(size_t index) const {
    if (index < targets.size()) {
      return &targets[index];
    }
    return nullptr;
  }

  // Get the total number of targets managed.
  size_t get_target_count() const { return targets.size(); }

  // Public accessor to edit or iterate over all targets.
  std::vector<Target> &get_targets() { return targets; }
  const std::vector<Target> &get_targets() const { return targets; }

  // Update the connection state and timestamp for a given target ID.
  void update_target_connection(uint8_t id, bool success,
                                unsigned long timestamp) {
    for (auto &target : targets) {
      if (target.id == id) {
        target.connection_state = success;
        if (success) {
          target.last_successful_send = timestamp;
        }
        break;
      }
    }
  }

  // Update the channel data for a given target ID.
  void update_target_channels(uint8_t id,
                              const uint16_t new_channels[TARGET_CHANNEL_COUNT]) {
    for (auto &target : targets) {
      if (target.id == id) {
        memcpy(target.channels, new_channels, sizeof(uint16_t) * TARGET_CHANNEL_COUNT);
        break;
      }
    }
  }

  // Generate a JSON string representing a specific target.
  String get_target_json(uint8_t id) const {
    const Target *target = get_target_by_id(id);
    if (!target)
      return "{}";

    String json = "{";
    json += "\"id\": " + String(target->id) + ",";
    json += "\"name\": \"" + target->name + "\",";
    json += "\"mac\": \"" + _mac_to_string(target->mac) + "\",";
    json += "\"channels\": [";
    for (int i = 0; i < TARGET_CHANNEL_COUNT; i++) {
      json += String(target->channels[i]);
      if (i < TARGET_CHANNEL_COUNT - 1)
        json += ",";
    }
    json += "],";
    json += "\"connection_state\": " +
            String(target->connection_state ? "true" : "false") + ",";
    json += "\"last_successful_send\": " + String(target->last_successful_send);
    json += "}";
    return json;
  }

  // Generate a JSON string representing all targets.
  String get_all_targets_json() const {
    String json = "[";
    for (size_t i = 0; i < targets.size(); i++) {
      json += get_target_json(targets[i].id);
      if (i < targets.size() - 1) {
        json += ",";
      }
    }
    json += "]";
    return json;
  }

private:
  // Helper function (with underscore prefix) to convert a MAC address to a
  // colon-separated string.
  String _mac_to_string(const uint8_t mac[6]) const {
    char mac_str[18];
    sprintf(mac_str, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2],
            mac[3], mac[4], mac[5]);
    return String(mac_str);
  }

  std::vector<Target> targets; // Ordered list of targets.
  uint8_t next_id;             // Next available ID for a new target.
};

} // namespace tm

#endif // TARGET_MANAGER_H