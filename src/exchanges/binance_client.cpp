#include "binance_client.hpp"
#include <nlohmann/json.hpp>
#include <cpr/cpr.h>
#include <iostream>
#include <cctype>

using json = nlohmann::json;

namespace strategia {

static std::string to_lower(std::string s) { for (auto &c : s) c = static_cast<char>(::tolower(c)); return s; }

BinanceClient::BinanceClient(std::string symbol)
	: symbol_(std::move(symbol)) {}

BinanceClient::~BinanceClient() { stop(); }

void BinanceClient::set_ticker_callback(TickerCallback cb) { on_ticker_ = std::move(cb); }
void BinanceClient::set_orderbook_callback(OrderBookCallback cb) { on_orderbook_ = std::move(cb); }

void BinanceClient::start() {
	if (running_.exchange(true)) return;
	ws_ = std::make_unique<ix::WebSocket>();
	run_ws();
}

void BinanceClient::stop() {
	if (!running_.exchange(false)) return;
	if (ws_) {
		ws_->stop();
		ws_.reset();
	}
}

void BinanceClient::run_ws() {
	const std::string stream_symbol = to_lower(symbol_);
	const std::string url = "wss://stream.binance.com:9443/stream?streams=" + stream_symbol + "@ticker/" + stream_symbol + "@depth5@100ms";
	ws_->setUrl(url);

	ws_->setOnMessageCallback([this](const ix::WebSocketMessagePtr &msg) {
		if (msg->type == ix::WebSocketMessageType::Open) {
			// Opened
		} else if (msg->type == ix::WebSocketMessageType::Message) {
			try {
				auto j = json::parse(msg->str);
				if (j.contains("stream") && j.contains("data")) {
					auto d = j["data"];
					if (d.contains("e") && d["e"].is_string()) {
						std::string ev = d["e"].get<std::string>();
						if (ev == "24hrTicker") {
							TickerData t{};
							t.exchange = "binance";
							t.symbol = symbol_;
							t.price = std::stod(d.value("c", "0"));
							t.ts_ms = d.value("E", 0ll);
							if (on_ticker_) on_ticker_(t);
						} else if (ev == "depthUpdate") {
							OrderBookData ob{};
							ob.exchange = "binance";
							ob.symbol = symbol_;
							ob.ts_ms = d.value("E", 0ll);
							ob.bids.clear();
							ob.asks.clear();
							if (d.contains("b")) {
								for (auto &b : d["b"]) {
									if (b.size() >= 2) {
										ob.bids.push_back({ std::stod(b[0].get<std::string>()), std::stod(b[1].get<std::string>()) });
									}
								}
							}
							if (d.contains("a")) {
								for (auto &a : d["a"]) {
									if (a.size() >= 2) {
										ob.asks.push_back({ std::stod(a[0].get<std::string>()), std::stod(a[1].get<std::string>()) });
									}
								}
							}
							if (on_orderbook_) on_orderbook_(ob);
						}
					}
				}
			} catch (const std::exception &e) {
				std::cerr << "Binance WS parse error: " << e.what() << "\n";
			}
		}
	});

	ws_->start();
}

}


