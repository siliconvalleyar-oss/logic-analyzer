//==============================================================================
// protocol.cpp
// Construccion de mensajes JSON del protocolo WebSocket
// Licencia: MIT
//==============================================================================

#include "protocol.h"

std::string proto_build_waveform(const std::vector<Sample>& samples,
                                 const std::string& pins_json,
                                 int rate_hz, int trigger_idx,
                                 bool reset) {
    if (samples.empty()) return "";

    std::string json = "{\"type\":\"waveform\",\"pins\":" + pins_json;
    json += ",\"timestamps\":[";
    for (size_t i = 0; i < samples.size(); i++) {
        if (i > 0) json += ",";
        json += std::to_string(samples[i].timestamp_ns);
    }
    json += "],\"states\":[";
    for (size_t i = 0; i < samples.size(); i++) {
        if (i > 0) json += ",";
        json += std::to_string(samples[i].gpio_state);
    }
    json += "],\"t0\":" + std::to_string(samples[0].timestamp_ns);
    json += ",\"dt_us\":1";
    json += ",\"rate\":" + std::to_string(rate_hz);
    json += ",\"samples\":" + std::to_string(samples.size());
    json += ",\"trigger_index\":" + std::to_string(trigger_idx);
    if (reset) json += ",\"reset\":true";
    json += "}";
    return json;
}

std::string proto_build_state(int rate, const std::string& pins_json,
                              int buf_size) {
    return "{\"type\":\"state\",\"mode\":\"run\",\"rate\":" +
           std::to_string(rate) + ",\"pins\":" + pins_json +
           ",\"samples\":" + std::to_string(buf_size) + "}";
}

std::string proto_build_config(int timebase_us, int trigger_pin,
                               const std::string& trigger_type,
                               const std::string& labels_json,
                               const std::string& enabled_pins_json,
                               const std::string& pins_json) {
    return "{\"type\":\"config\""
           ",\"timebase_us\":" + std::to_string(timebase_us) +
           ",\"trigger_pin\":" + std::to_string(trigger_pin) +
           ",\"trigger_type\":\"" + trigger_type + "\""
           ",\"labels\":" + labels_json +
           ",\"enabled_pins\":" + enabled_pins_json +
           ",\"pins\":" + pins_json +
           "}";
}

std::string proto_build_labels_json(const std::map<int, std::string>& labels) {
    std::string json = "{";
    bool first = true;
    for (const auto& [pin, label] : labels) {
        if (!first) json += ",";
        first = false;
        json += "\"" + std::to_string(pin) + "\":\"" + label + "\"";
    }
    json += "}";
    return json;
}

std::string proto_extract_string(const std::string& json,
                                 const std::string& key) {
    size_t pos = json.find("\"" + key + "\"");
    if (pos == std::string::npos) return "";

    pos = json.find(':', pos) + 1;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t'))
        pos++;

    if (pos < json.size() && json[pos] == '"') {
        pos++;
        std::string val;
        while (pos < json.size() && json[pos] != '"') {
            val += json[pos];
            pos++;
        }
        return val;
    }
    return "";
}

int proto_extract_int(const std::string& json, const std::string& key,
                      int default_val) {
    size_t pos = json.find("\"" + key + "\"");
    if (pos == std::string::npos) return default_val;

    pos = json.find(':', pos) + 1;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t'))
        pos++;

    bool neg = false;
    if (pos < json.size() && json[pos] == '-') { neg = true; pos++; }
    int val = 0;
    while (pos < json.size() && json[pos] >= '0' && json[pos] <= '9') {
        val = val * 10 + (json[pos] - '0');
        pos++;
    }
    return neg ? -val : val;
}
