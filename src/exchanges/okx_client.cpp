#include "okx_client.hpp"
#include <nlohmann/json.hpp>
#include <iostream>

using json = nlohmann::json;

namespace strategia {

OkxClient::OkxClient(std::string symbol)
	: symbol_(std::move(symbol)) {}

OkxClient::~OkxClient() { stop(); }

void OkxClient::set_ticker_callback(TickerCallback cb) { on_ticker_ = std::move(cb); }
void OkxClient::set_orderbook_callback(OrderBookCallback cb) { on_orderbook_ = std::move(cb); }

void OkxClient::start() {
	if (running_.exchange(true)) return;
#ifdef STRATEGIA_ENABLE_WEBSOCKETS
	ws_ = std::make_unique<ix::WebSocket>();
	run_ws();
#endif
}

void OkxClient::stop() {
	if (!running_.exchange(false)) return;
#ifdef STRATEGIA_ENABLE_WEBSOCKETS
	if (ws_) {
		ws_->stop();
		ws_.reset();
	}
#endif
}

void OkxClient::run_ws() {
#ifdef STRATEGIA_ENABLE_WEBSOCKETS
	const std::string url = "wss://ws.okx.com:8443/ws/v5/public";
	ws_->setUrl(url);
	ws_->setOnMessageCallback([this](const ix::WebSocketMessagePtr &msg) {
		if (msg->type == ix::WebSocketMessageType::Open) {
			json sub = {
				{"op", "subscribe"},
				{"args", json::array({
					json{{"channel", "tickers"}, {"instId", symbol_}},
					json{{"channel", "books5"}, {"instId", symbol_}}
				})}
			};
			ws_->send(sub.dump());
		} else if (msg->type == ix::WebSocketMessageType::Message) {
			try {
				auto j = json::parse(msg->str);
				if (j.contains("event")) {
					// ignore subscription acks
					return;
				}
				if (j.contains("arg") && j.contains("data")) {
					auto arg = j["arg"];
					std::string channel = arg.value("channel", "");
					if (channel == "tickers") {
						// data is array with one object
						if (!j["data"].empty()) {
							auto d = j["data"][0];
							TickerData t{};
							t.exchange = "okx";
							t.symbol = symbol_;
							t.price = std::stod(d.value("last", "0"));
							t.ts_ms = std::stoll(d.value("ts", "0"));
							if (on_ticker_) on_ticker_(t);
						}
					} else if (channel == "books5") {
						if (!j["data"].empty()) {
							auto d = j["data"][0];
							OrderBookData ob{};
							ob.exchange = "okx";
							ob.symbol = symbol_;
							ob.ts_ms = std::stoll(d.value("ts", "0"));
							for (auto &b : d["bids"]) {
								if (b.size() >= 2) ob.bids.push_back({ std::stod(b[0].get<std::string>()), std::stod(b[1].get<std::string>()) });
							}
							for (auto &a : d["asks"]) {
								if (a.size() >= 2) ob.asks.push_back({ std::stod(a[0].get<std::string>()), std::stod(a[1].get<std::string>()) });
							}
							if (on_orderbook_) on_orderbook_(ob);
						}
					}
				}
			} catch (const std::exception &e) {
				std::cerr << "OKX WS parse error: " << e.what() << "\n";
			}
		}
	});

	ws_->start();
#endif
}

}


