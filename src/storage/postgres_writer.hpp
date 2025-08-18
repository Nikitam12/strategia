#pragma once

#include "storage_writer.hpp"
#include <pqxx/pqxx>
#include <memory>

namespace strategia {

class PostgresWriter final : public StorageWriter {
public:
	explicit PostgresWriter(std::string dsn)
		: dsn_(std::move(dsn)) {}

	void ensure_schema() override;
	void write_batch(const std::vector<MinuteSnapshot>& rows) override;

private:
	std::string dsn_;
};

}


