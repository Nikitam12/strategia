//
// Created by Никита on 18.08.2025.
//
#include <iostream>
#include <cstdlib>
#include "config.hpp"

namespace strategia {
void run_service(const Config &cfg);
}

int main() {
    strategia::Config cfg;
    // SYMBOL_BINANCE / SYMBOL_OKX переопределяют значения по умолчанию
    if (const char* v = std::getenv("SYMBOL_BINANCE")) cfg.symbol_binance = v;
    if (const char* v = std::getenv("SYMBOL_OKX")) cfg.symbol_okx = v;
    if (const char* v = std::getenv("CSV_DIR")) cfg.csv_output_dir = v;
    if (const char* v = std::getenv("POSTGRES_DSN")) { cfg.postgres_dsn = v; cfg.enable_postgres = true; }

    try {
        strategia::run_service(cfg);
    } catch (const std::exception &e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}