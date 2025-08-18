#pragma once

#include <string>

namespace strategia {

struct Config {
	// Symbols like "BTCUSDT" for Binance, "BTC-USDT" for OKX
	std::string symbol_binance = "BTCUSDT";
	std::string symbol_okx = "BTC-USDT";

	// CSV storage
	std::string csv_output_dir = "data";

	// Postgres DSN, e.g. "postgresql://user:pass@localhost:5432/market"
	std::string postgres_dsn;
	bool enable_postgres = false;
};

}


