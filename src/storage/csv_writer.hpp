#pragma once

#include "storage_writer.hpp"
#include <string>
#include <filesystem>

namespace strategia {

class CsvWriter final : public StorageWriter {
public:
	explicit CsvWriter(std::string directory);
	void ensure_schema() override;
	void write_batch(const std::vector<MinuteSnapshot>& rows) override;

private:
	std::filesystem::path dir_;
};

}


