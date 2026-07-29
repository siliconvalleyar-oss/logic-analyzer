//==============================================================================
// config.cpp
// Parseo de argumentos CLI y carga de config.json
// Licencia: MIT
//==============================================================================

#include "config.h"
#include <iostream>
#include <cstring>
#include <cstdio>
#include <cstdlib>

ServerConfig config_parse_args(int argc, char* argv[]) {
    ServerConfig cfg;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "-p" || arg == "--port") {
            if (i + 1 < argc) cfg.http_port = atoi(argv[++i]);
        } else if (arg[0] != '-' && argc > 1 && i == 1) {
            cfg.http_port = atoi(argv[i]);
        } else if (arg == "-r" || arg == "--rate") {
            if (i + 1 < argc) cfg.sample_rate_hz = atoi(argv[++i]);
        } else if (arg == "-c" || arg == "--config") {
            if (i + 1 < argc) {
                cfg = config_load_file(argv[++i]);
                cfg.config_path = argv[i];  // recordar ruta para guardar
            }
        } else if (arg == "--simulate") {
            cfg.simulate = true;
        } else if (arg == "-v" || arg == "--verbose") {
            cfg.log_level = "DEBUG";
        } else if (arg == "-l" || arg == "--log") {
            if (i + 1 < argc) cfg.log_file = argv[++i];
        } else if (arg == "--version") {
            std::cout << "Logic Analyzer Server v1.1.0" << std::endl;
            exit(0);
        } else if (arg == "--help") {
            config_print_help(argv[0]);
            exit(0);
        }
    }
    return cfg;
}

ServerConfig config_load_file(const std::string& filepath) {
    ServerConfig cfg;
    FILE* f = fopen(filepath.c_str(), "rb");
    if (!f) {
        std::cerr << "[Config] Cannot open: " << filepath << std::endl;
        return cfg;
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::string json(len, '\0');
    if (fread(&json[0], 1, len, f) != (size_t)len) {
        std::cerr << "[Config] Failed to read: " << filepath << std::endl;
        fclose(f);
        return cfg;
    }
    fclose(f);

    // Simple JSON key:value extraction (no full parser)
    auto extract = [&](const std::string& key, int def) -> int {
        size_t p = json.find("\"" + key + "\"");
        if (p == std::string::npos) return def;
        p = json.find(':', p) + 1;
        while (p < json.size() && (json[p]==' '||json[p]=='\t')) p++;
        int v = 0; bool neg = false;
        if (p < json.size() && json[p]=='-') { neg=true; p++; }
        while (p < json.size() && json[p]>='0' && json[p]<='9')
            { v=v*10+(json[p]-'0'); p++; }
        return neg ? -v : v;
    };

    cfg.http_port = extract("http_port", 8080);
    cfg.sample_rate_hz = extract("rate_hz", 500000);
    cfg.buffer_size = extract("buffer_size", 4096);
    cfg.trigger_pin = extract("pin", -1);
    cfg.timebase_us = extract("timebase_us", 500000);

    // enabled_pins from JSON array
    {
        size_t p = json.find("\"enabled_pins\"");
        if (p != std::string::npos) {
            p = json.find('[', p);
            if (p != std::string::npos) {
                p++;
                cfg.enabled_pins.clear();
                while (p < json.size() && json[p] != ']') {
                    while (p < json.size() && (json[p]==' '||json[p]=='\t'||json[p]=='\n')) p++;
                    if (p >= json.size() || json[p] == ']') break;
                    if (json[p] >= '0' && json[p] <= '9') {
                        int val = 0;
                        while (p < json.size() && json[p] >= '0' && json[p] <= '9') {
                            val = val * 10 + (json[p] - '0');
                            p++;
                        }
                        cfg.enabled_pins.push_back(val);
                    } else p++;
                    while (p < json.size() && json[p] == ',') p++;
                }
            }
        }
    }

    // channel_labels from JSON object
    {
        size_t p = json.find("\"labels\"");
        if (p != std::string::npos) {
            p = json.find('{', p);
            if (p != std::string::npos) {
                p++; // skip {
                while (p < json.size()) {
                    // skip whitespace
                    while (p < json.size() && (json[p]==' '||json[p]=='\t'||json[p]=='\n')) p++;
                    if (p >= json.size() || json[p] == '}') break;
                    // find key (pin number as string)
                    if (json[p] != '"') break;
                    p++;
                    std::string key;
                    while (p < json.size() && json[p] != '"') { key += json[p]; p++; }
                    if (p >= json.size()) break;
                    p++; // skip "
                    // find :
                    while (p < json.size() && json[p] != ':') p++;
                    if (p >= json.size()) break;
                    p++; // skip :
                    while (p < json.size() && (json[p]==' '||json[p]=='\t')) p++;
                    // find value
                    if (p >= json.size() || json[p] != '"') break;
                    p++;
                    std::string val;
                    while (p < json.size() && json[p] != '"') { val += json[p]; p++; }
                    if (p >= json.size()) break;
                    p++; // skip "
                    int pin = atoi(key.c_str());
                    if (pin > 0) cfg.channel_labels[pin] = val;
                    // look for comma or }
                    while (p < json.size() && json[p] != ',' && json[p] != '}') p++;
                    if (p < json.size() && json[p] == ',') p++;
                }
            }
        }
    }

    // trigger_type string
    {
        size_t p = json.find("\"trigger_type\"");
        if (p != std::string::npos) {
            p = json.find(':', p) + 1;
            while (p < json.size() && (json[p]==' '||json[p]=='\t')) p++;
            if (p < json.size() && json[p] == '"') {
                p++;
                cfg.trigger_type = "";
                while (p < json.size() && json[p] != '"') {
                    cfg.trigger_type += json[p];
                    p++;
                }
            }
        }
    }

    // decoder_config JSON string (capturar hasta el final del objeto)
    {
        size_t p = json.find("\"decoder\"");
        if (p != std::string::npos) {
            p = json.find(':', p) + 1;
            while (p < json.size() && (json[p]==' '||json[p]=='\t')) p++;
            // Capturar todo el objeto JSON hasta el } de cierre, contando anidacion
            if (p < json.size() && json[p] == '{') {
                int depth = 0;
                size_t start = p;
                while (p < json.size()) {
                    if (json[p] == '{') depth++;
                    else if (json[p] == '}') { depth--; if (depth == 0) { p++; break; } }
                    p++;
                }
                cfg.decoder_config_json = json.substr(start, p - start);
            }
        }
    }

    return cfg;
}

bool config_save_file(const ServerConfig& cfg, const std::string& filepath) {
    std::string save_path = filepath.empty() ? cfg.config_path : filepath;
    FILE* f = fopen(save_path.c_str(), "wb");
    if (!f) {
        std::cerr << "[Config] Cannot write: " << filepath << std::endl;
        return false;
    }

    std::string json = "{\n";
    json += "  \"http_port\": "    + std::to_string(cfg.http_port) + ",\n";
    json += "  \"rate_hz\": "     + std::to_string(cfg.sample_rate_hz) + ",\n";
    json += "  \"buffer_size\": " + std::to_string(cfg.buffer_size) + ",\n";
    json += "  \"timebase_us\": " + std::to_string(cfg.timebase_us) + ",\n";
    json += "  \"pin\": "         + std::to_string(cfg.trigger_pin) + ",\n";
    json += "  \"trigger_type\": \"" + cfg.trigger_type + "\",\n";
    // Enabled pins
    json += "  \"enabled_pins\": [";
    for (size_t i = 0; i < cfg.enabled_pins.size(); i++) {
        if (i > 0) json += ",";
        json += std::to_string(cfg.enabled_pins[i]);
    }
    json += "],\n";
    // Decoder config (si no esta vacio)
    if (!cfg.decoder_config_json.empty()) {
        json += "  \"decoder\": " + cfg.decoder_config_json + ",\n";
    }
    // Labels
    json += "  \"labels\": {\n";
    {
        bool first = true;
        for (const auto& [pin, label] : cfg.channel_labels) {
            if (!first) json += ",\n";
            first = false;
            json += "    \"" + std::to_string(pin) + "\": \"" + label + "\"";
        }
    }
    json += "\n  }\n";
    json += "}\n";

    bool ok = (fwrite(json.data(), 1, json.size(), f) == json.size());
    fclose(f);

    if (ok) {
        std::cout << "[Config] Saved: " << filepath << std::endl;
    } else {
        std::cerr << "[Config] Write failed: " << filepath << std::endl;
    }
    return ok;
}

void config_print_help(const char* name) {
    std::cout << "Logic Analyzer Server - Usage:\n"
              << "  " << name << " [port] [options]\n\n"
              << "Options:\n"
              << "  port                   HTTP port (default: 8080)\n"
              << "  -p, --port <port>     HTTP port (default: 8080)\n"
              << "  -c, --config <file>   Config file (default: config.json)\n"
              << "  -r, --rate <hz>       Sample rate (default: 500000)\n"
              << "  --simulate            Simulation mode (no GPIO)\n"
              << "  -v, --verbose         Verbose logging\n"
              << "  -l, --log <file>      Log file path\n"
              << "  --version             Show version\n"
              << "  --help                Show this help\n";
}
