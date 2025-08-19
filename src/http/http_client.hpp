#pragma once

#include <string>

namespace strategia {

class HttpClient {
public:
	static std::string get(const std::string &url_with_query, long &status_code);
};

}


