#pragma once

#include "exchange_client.hpp"
#ifdef STRATEGIA_ENABLE_WEBSOCKETS
#include <ixwebsocket/IXWebSocket.h>
#endif
#include <atomic>
#include <thread>
#include <memory>

namespace strategia {

class BinanceClient final : public ExchangeClient {
public:
	explicit BinanceClient(std::string symbol);
	~BinanceClient() override;

	void start() override;
	void stop() override;

	void set_ticker_callback(TickerCallback cb) override;
	void set_orderbook_callback(OrderBookCallback cb) override;

private:
	void run_ws();
	void subscribe();

private:
	std::string symbol_;
#ifdef STRATEGIA_ENABLE_WEBSOCKETS
	std::unique_ptr<ix::WebSocket> ws_;
#endif
	std::atomic<bool> running_{false};
	TickerCallback on_ticker_;
	OrderBookCallback on_orderbook_;
};

}


