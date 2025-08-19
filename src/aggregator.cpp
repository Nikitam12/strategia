#include "config.hpp"
#include "time_utils.hpp"
#include "exchanges/binance_client.hpp"
#include "exchanges/okx_client.hpp"
#include "storage/csv_writer.hpp"
#ifdef STRATEGIA_ENABLE_POSTGRES
#include "storage/postgres_writer.hpp"
#endif

#include <mutex>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <condition_variable>
#ifdef STRATEGIA_ENABLE_REST_BACKFILL
#include <cpr/cpr.h>
#endif
#include <nlohmann/json.hpp>

namespace strategia {

struct InMemoryState {
	std::optional<double> last_price;
	std::optional<double> best_bid_price;
	std::optional<double> best_bid_amount;
	std::optional<double> best_ask_price;
	std::optional<double> best_ask_amount;
};

class Aggregator {
public:
	Aggregator(Config cfg)
		: cfg_(std::move(cfg)) {}

	void run() {
		CsvWriter csv(cfg_.csv_output_dir);
		StorageWriter *writer = &csv;
#ifdef STRATEGIA_ENABLE_POSTGRES
		std::unique_ptr<PostgresWriter> pg;
		if (cfg_.enable_postgres && !cfg_.postgres_dsn.empty()) {
			pg = std::make_unique<PostgresWriter>(cfg_.postgres_dsn);
			writer = pg.get();
		}
#endif
		writer->ensure_schema();

		// Add test data to verify REST backfill and CSV writing
		{
			std::lock_guard<std::mutex> lk(mu_);
			state_["binance:BTCUSDT"] = InMemoryState{};
			state_["okx:BTC-USDT"] = InMemoryState{};
		}

		BinanceClient binance(cfg_.symbol_binance);
		OkxClient okx(cfg_.symbol_okx);

#ifdef STRATEGIA_ENABLE_WEBSOCKETS
		binance.set_ticker_callback([this](const TickerData &t){ on_ticker(t); });
		okx.set_ticker_callback([this](const TickerData &t){ on_ticker(t); });
		binance.set_orderbook_callback([this](const OrderBookData &o){ on_orderbook(o); });
		okx.set_orderbook_callback([this](const OrderBookData &o){ on_orderbook(o); });

		binance.start();
		okx.start();
#endif

		std::atomic<bool> running{true};
		std::thread flusher([&]{
			std::int64_t current_bucket = minute_bucket_unix(current_unix_seconds());
			while (running.load()) {
				std::this_thread::sleep_for(std::chrono::seconds(1));
				auto now = current_unix_seconds();
				auto bucket = minute_bucket_unix(now);
				if (bucket > current_bucket) {
					// finalize previous minute
					auto rows = snapshot_and_rotate(current_bucket);
					// If no last price for some symbols, backfill via REST
#ifdef STRATEGIA_ENABLE_REST_BACKFILL
					for (auto &row : rows) {
						if (!row.last_price) {
							if (row.exchange == "binance") {
								try {
#ifdef STRATEGIA_USE_LIBCURL_REST
									long sc = 0; auto body = HttpClient::get(std::string("https://api.binance.com/api/v3/ticker/price?symbol=") + row.symbol, sc);
									if (sc == 200) { auto j = nlohmann::json::parse(body); row.last_price = std::stod(j.value("price", "0")); }
#else
									auto r = cpr::Get(cpr::Url{ "https://api.binance.com/api/v3/ticker/price" }, cpr::Parameters{{"symbol", row.symbol}});
									if (r.status_code == 200) { auto j = nlohmann::json::parse(r.text); row.last_price = std::stod(j.value("price", "0")); }
#endif
								} catch (...) {}
							} else if (row.exchange == "okx") {
								try {
#ifdef STRATEGIA_USE_LIBCURL_REST
									long sc = 0; auto body = HttpClient::get(std::string("https://www.okx.com/api/v5/market/ticker?instId=") + row.symbol, sc);
									if (sc == 200) { auto j = nlohmann::json::parse(body); if (j.contains("data") && !j["data"].empty()) { row.last_price = std::stod(j["data"][0].value("last", "0")); } }
#else
									auto r = cpr::Get(cpr::Url{ "https://www.okx.com/api/v5/market/ticker" }, cpr::Parameters{{"instId", row.symbol}});
									if (r.status_code == 200) { auto j = nlohmann::json::parse(r.text); if (j.contains("data") && !j["data"].empty()) { row.last_price = std::stod(j["data"][0].value("last", "0")); } }
#endif
								} catch (...) {}
							}
							// Backfill top-of-book if missing
							if (!row.best_bid_price || !row.best_ask_price) {
								if (row.exchange == "binance") {
									try {
#ifdef STRATEGIA_USE_LIBCURL_REST
										long sc = 0; auto body = HttpClient::get(std::string("https://api.binance.com/api/v3/depth?symbol=") + row.symbol + "&limit=5", sc);
										if (sc == 200) { auto j = nlohmann::json::parse(body); if (j.contains("bids") && !j["bids"].empty()) { row.best_bid_price = std::stod(j["bids"][0][0].get<std::string>()); row.best_bid_amount = std::stod(j["bids"][0][1].get<std::string>()); } if (j.contains("asks") && !j["asks"].empty()) { row.best_ask_price = std::stod(j["asks"][0][0].get<std::string>()); row.best_ask_amount = std::stod(j["asks"][0][1].get<std::string>()); } }
#else
										auto r = cpr::Get(cpr::Url{ "https://api.binance.com/api/v3/depth" }, cpr::Parameters{{"symbol", row.symbol}, {"limit", "5"}});
										if (r.status_code == 200) { auto j = nlohmann::json::parse(r.text); if (j.contains("bids") && !j["bids"].empty()) { row.best_bid_price = std::stod(j["bids"][0][0].get<std::string>()); row.best_bid_amount = std::stod(j["bids"][0][1].get<std::string>()); } if (j.contains("asks") && !j["asks"].empty()) { row.best_ask_price = std::stod(j["asks"][0][0].get<std::string>()); row.best_ask_amount = std::stod(j["asks"][0][1].get<std::string>()); } }
#endif
									} catch (...) {}
								} else if (row.exchange == "okx") {
									try {
#ifdef STRATEGIA_USE_LIBCURL_REST
										long sc = 0; auto body = HttpClient::get(std::string("https://www.okx.com/api/v5/market/books?instId=") + row.symbol + "&sz=5", sc);
										if (sc == 200) { auto j = nlohmann::json::parse(body); if (j.contains("data") && !j["data"].empty()) { auto &d = j["data"][0]; if (d.contains("bids") && !d["bids"].empty()) { row.best_bid_price = std::stod(d["bids"][0][0].get<std::string>()); row.best_bid_amount = std::stod(d["bids"][0][1].get<std::string>()); } if (d.contains("asks") && !d["asks"].empty()) { row.best_ask_price = std::stod(d["asks"][0][0].get<std::string>()); row.best_ask_amount = std::stod(d["asks"][0][1].get<std::string>()); } } }
#else
										auto r = cpr::Get(cpr::Url{ "https://www.okx.com/api/v5/market/books" }, cpr::Parameters{{"instId", row.symbol}, {"sz", "5"}});
										if (r.status_code == 200) { auto j = nlohmann::json::parse(r.text); if (j.contains("data") && !j["data"].empty()) { auto &d = j["data"][0]; if (d.contains("bids") && !d["bids"].empty()) { row.best_bid_price = std::stod(d["bids"][0][0].get<std::string>()); row.best_bid_amount = std::stod(d["bids"][0][1].get<std::string>()); } if (d.contains("asks") && !d["asks"].empty()) { row.best_ask_price = std::stod(d["asks"][0][0].get<std::string>()); row.best_ask_amount = std::stod(d["asks"][0][1].get<std::string>()); } } }
#endif
									} catch (...) {}
								}
							}
						}
#endif
					}
					if (!rows.empty()) writer->write_batch(rows);
					current_bucket = bucket;
				}
			}
		});

		// Блокируемся навсегда (остановка процессом/сервисом)
		flusher.join();
	}

private:
	void on_ticker(const TickerData &t) {
		std::lock_guard<std::mutex> lk(mu_);
		auto &state = state_[t.exchange + ":" + t.symbol];
		state.last_price = t.price;
	}

	void on_orderbook(const OrderBookData &o) {
		std::lock_guard<std::mutex> lk(mu_);
		auto &state = state_[o.exchange + ":" + o.symbol];
		if (!o.bids.empty()) {
			state.best_bid_price = o.bids.front().price;
			state.best_bid_amount = o.bids.front().amount;
		}
		if (!o.asks.empty()) {
			state.best_ask_price = o.asks.front().price;
			state.best_ask_amount = o.asks.front().amount;
		}
	}

	std::vector<MinuteSnapshot> snapshot_and_rotate(std::int64_t minute_bucket) {
		std::vector<MinuteSnapshot> rows;
		std::lock_guard<std::mutex> lk(mu_);
		for (auto &kv : state_) {
			const std::string &key = kv.first; // exchange:symbol
			const auto colon = key.find(':');
			if (colon == std::string::npos) continue;
			std::string exchange = key.substr(0, colon);
			std::string symbol = key.substr(colon + 1);
			InMemoryState &s = kv.second;
			MinuteSnapshot r{};
			r.minute_unix = minute_bucket;
			r.exchange = exchange;
			r.symbol = symbol;
			r.last_price = s.last_price;
			r.best_bid_price = s.best_bid_price;
			r.best_bid_amount = s.best_bid_amount;
			r.best_ask_price = s.best_ask_price;
			r.best_ask_amount = s.best_ask_amount;
			rows.push_back(r);
		}
		// do not clear; keep rolling state for next minute
		return rows;
	}

private:
	Config cfg_;
	std::mutex mu_;
	std::unordered_map<std::string, InMemoryState> state_;
};

void run_service(const Config &cfg) {
	Aggregator aggr(cfg);
	aggr.run();
}

}

