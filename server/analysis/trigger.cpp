//==============================================================================
// trigger.cpp
// Deteccion de disparo por flanco en muestras GPIO
// Licencia: MIT
//==============================================================================

#include "trigger.h"
#include <algorithm>

TriggerType TriggerConfig::from_string(const std::string& s) {
    if (s == "rising")  return TriggerType::RISING;
    if (s == "falling") return TriggerType::FALLING;
    if (s == "both")    return TriggerType::BOTH;
    if (s == "high")    return TriggerType::HIGH;
    if (s == "low")     return TriggerType::LOW;
    return TriggerType::NONE;
}

std::string TriggerConfig::type_to_string(TriggerType t) {
    switch (t) {
        case TriggerType::RISING:  return "rising";
        case TriggerType::FALLING: return "falling";
        case TriggerType::BOTH:    return "both";
        case TriggerType::HIGH:    return "high";
        case TriggerType::LOW:     return "low";
        default:                   return "none";
    }
}

int trigger_find_index(const std::vector<Sample>& samples,
                       const TriggerConfig& config) {
    if (config.pin < 0 || config.type == TriggerType::NONE || samples.size() < 2)
        return -1;

    int last_state = (samples[0].gpio_state >> config.pin) & 1;
    for (size_t i = 1; i < samples.size(); i++) {
        int cur = (samples[i].gpio_state >> config.pin) & 1;
        bool match = false;
        switch (config.type) {
            case TriggerType::RISING:  match = (last_state == 0 && cur == 1); break;
            case TriggerType::FALLING: match = (last_state == 1 && cur == 0); break;
            case TriggerType::BOTH:    match = (last_state != cur); break;
            case TriggerType::HIGH:    match = (cur == 1); break;
            case TriggerType::LOW:     match = (cur == 0); break;
            default: break;
        }
        if (match) return (int)i;
        last_state = cur;
    }
    return -1;
}
