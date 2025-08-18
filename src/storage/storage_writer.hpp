#pragma once

#include <string>
#include <vector>
#include <optional>

namespace strategia {

struct MinuteSnapshot {
	std::int64_t minute_unix = 0; // start of minute (unix sec)
	std::string exchange;
	std::string symbol;
	std::optional<double> last_price;
	// Best levels (if available)
	std::optional<double> best_bid_price;
	std::optional<double> best_bid_amount;
	std::optional<double> best_ask_price;
	std::optional<double> best_ask_amount;
};

class StorageWriter {
public:
	virtual ~StorageWriter() = default;
	virtual void ensure_schema() = 0;
	virtual void write_batch(const std::vector<MinuteSnapshot>& rows) = 0;
};

}


