#pragma once

#include <string>
#include <functional>
#include <optional>
#include <vector>
#include <mutex>

namespace strategia {

struct TickerData {
	std::string exchange; // "binance" or "okx"
	std::string symbol;   // e.g. "BTCUSDT"
	double price = 0.0;
	std::int64_t ts_ms = 0; // exchange timestamp in ms if available
};

struct OrderBookLevel {
	double price = 0.0;
	double amount = 0.0;
};

struct OrderBookData {
	std::string exchange;
	std::string symbol;
	std::vector<OrderBookLevel> bids; // sorted desc by price
	std::vector<OrderBookLevel> asks; // sorted asc by price
	std::int64_t ts_ms = 0;
};

using TickerCallback = std::function<void(const TickerData&)>;
using OrderBookCallback = std::function<void(const OrderBookData&)>;

class ExchangeClient {
public:
	virtual ~ExchangeClient() = default;
	virtual void start() = 0;
	virtual void stop() = 0;

	virtual void set_ticker_callback(TickerCallback cb) = 0;
	virtual void set_orderbook_callback(OrderBookCallback cb) = 0;
};

}


