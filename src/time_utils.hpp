#pragma once

#include <chrono>
#include <cstdint>

namespace strategia {

inline std::int64_t to_unix_seconds(std::chrono::system_clock::time_point tp) {
	return std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch()).count();
}

inline std::int64_t current_unix_seconds() {
	return to_unix_seconds(std::chrono::system_clock::now());
}

inline std::int64_t minute_bucket_unix(std::int64_t unix_seconds) {
	return (unix_seconds / 60) * 60;
}

}


