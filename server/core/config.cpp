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
        } else if (arg == "-r" || arg == "--rate") {
            if (i + 1 < argc) cfg.sample_rate_hz = atoi(argv[++i]);
        } else if (arg == "-c" || arg == "--config") {
            if (i + 1 < argc) {
                cfg = config_load_file(argv[++i]);
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

    return cfg;
}

void config_print_help(const char* name) {
    std::cout << "Logic Analyzer Server - Usage:\n"
              << "  " << name << " [options]\n\n"
              << "Options:\n"
              << "  -p, --port <port>     HTTP port (default: 8080)\n"
              << "  -c, --config <file>   Config file (default: config.json)\n"
              << "  -r, --rate <hz>       Sample rate (default: 500000)\n"
              << "  --simulate            Simulation mode (no GPIO)\n"
              << "  -v, --verbose         Verbose logging\n"
              << "  -l, --log <file>      Log file path\n"
              << "  --version             Show version\n"
              << "  --help                Show this help\n";
}
