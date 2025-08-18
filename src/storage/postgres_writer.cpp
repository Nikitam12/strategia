#include "postgres_writer.hpp"

namespace strategia {

void PostgresWriter::ensure_schema() {
	pqxx::connection conn(dsn_);
	pqxx::work tx(conn);
	tx.exec(R"SQL(
CREATE TABLE IF NOT EXISTS minute_snapshots (
    minute_unix BIGINT NOT NULL,
    exchange TEXT NOT NULL,
    symbol TEXT NOT NULL,
    last_price DOUBLE PRECISION,
    best_bid_price DOUBLE PRECISION,
    best_bid_amount DOUBLE PRECISION,
    best_ask_price DOUBLE PRECISION,
    best_ask_amount DOUBLE PRECISION,
    PRIMARY KEY (minute_unix, exchange, symbol)
);
)SQL");
	tx.commit();
}

void PostgresWriter::write_batch(const std::vector<MinuteSnapshot>& rows) {
	if (rows.empty()) return;
	pqxx::connection conn(dsn_);
	pqxx::work tx(conn);
	for (const auto &row : rows) {
		// upsert
		auto last_price = row.last_price ? std::to_string(*row.last_price) : "NULL";
		auto bbp = row.best_bid_price ? std::to_string(*row.best_bid_price) : "NULL";
		auto bba = row.best_bid_amount ? std::to_string(*row.best_bid_amount) : "NULL";
		auto bap = row.best_ask_price ? std::to_string(*row.best_ask_price) : "NULL";
		auto baa = row.best_ask_amount ? std::to_string(*row.best_ask_amount) : "NULL";
		tx.exec(
			"INSERT INTO minute_snapshots (minute_unix, exchange, symbol, last_price, best_bid_price, best_bid_amount, best_ask_price, best_ask_amount) VALUES (" +
			std::to_string(row.minute_unix) + ", " +
			tx.quote(row.exchange) + ", " +
			tx.quote(row.symbol) + ", " +
			last_price + ", " + bbp + ", " + bba + ", " + bap + ", " + baa +
			") ON CONFLICT (minute_unix, exchange, symbol) DO UPDATE SET "
			"last_price = EXCLUDED.last_price, "
			"best_bid_price = EXCLUDED.best_bid_price, "
			"best_bid_amount = EXCLUDED.best_bid_amount, "
			"best_ask_price = EXCLUDED.best_ask_price, "
			"best_ask_amount = EXCLUDED.best_ask_amount"
		);
	}
	tx.commit();
}

}


