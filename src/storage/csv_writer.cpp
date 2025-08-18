#include "csv_writer.hpp"
#include <fstream>
#include <iomanip>

namespace fs = std::filesystem;

namespace strategia {

CsvWriter::CsvWriter(std::string directory)
	: dir_(std::move(directory)) {}

void CsvWriter::ensure_schema() {
	fs::create_directories(dir_);
}

static std::string file_name_for(const MinuteSnapshot &row) {
	// One file per exchange+symbol, e.g., binance_BTCUSDT.csv
	return row.exchange + "_" + row.symbol + ".csv";
}

void CsvWriter::write_batch(const std::vector<MinuteSnapshot>& rows) {
	for (const auto &row : rows) {
		fs::path file = dir_ / file_name_for(row);
		bool exists = fs::exists(file);
		std::ofstream out(file, std::ios::app);
		if (!exists) {
			out << "minute_unix,exchange,symbol,last_price,best_bid_price,best_bid_amount,best_ask_price,best_ask_amount\n";
		}
		out << row.minute_unix << ','
			<< row.exchange << ','
			<< row.symbol << ','
			<< (row.last_price ? std::to_string(*row.last_price) : "") << ','
			<< (row.best_bid_price ? std::to_string(*row.best_bid_price) : "") << ','
			<< (row.best_bid_amount ? std::to_string(*row.best_bid_amount) : "") << ','
			<< (row.best_ask_price ? std::to_string(*row.best_ask_price) : "") << ','
			<< (row.best_ask_amount ? std::to_string(*row.best_ask_amount) : "")
			<< "\n";
	}
	// rely on RAII to close files
}

}


