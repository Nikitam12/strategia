#pragma once

#include <string>
#include <vector>
#include <utility>

#ifdef STRATEGIA_USE_LIBCURL_REST
#include "http/http_client.hpp"
#endif

namespace cpr {

struct Url {
	std::string url;
	Url(std::string u) : url(std::move(u)) {}
	Url(const char* u) : url(u) {}
};

struct Parameters : public std::vector<std::pair<std::string, std::string>> {
	using std::vector<std::pair<std::string, std::string>>::vector;

	Parameters() = default;
	Parameters(std::initializer_list<std::pair<const char*, const char*>> init) {
		this->reserve(init.size());
		for (const auto &p : init) this->emplace_back(std::string(p.first), std::string(p.second));
	}
	Parameters(std::initializer_list<std::pair<std::string, std::string>> init) {
		this->reserve(init.size());
		for (const auto &p : init) this->emplace_back(p.first, p.second);
	}
};

struct Response {
	long status_code {0};
	std::string text;
};

inline std::string build_query(const Parameters &params) {
	std::string q;
	for (size_t i = 0; i < params.size(); ++i) {
		if (i > 0) q.push_back('&');
		q += params[i].first;
		q.push_back('=');
		q += params[i].second;
	}
	return q;
}

inline Response Get(const Url &url, const Parameters &params) {
	Response r;
#ifdef STRATEGIA_USE_LIBCURL_REST
	std::string full = url.url;
	const std::string q = build_query(params);
	if (!q.empty()) {
		full.push_back(full.find('?') == std::string::npos ? '?' : '&');
		full += q;
	}
	try {
		long sc = 0;
		r.text = strategia::HttpClient::get(full, sc);
		r.status_code = sc;
	} catch (...) {
		r.status_code = 0;
		r.text.clear();
	}
#else
	(void)url; (void)params;
	r.status_code = 0; r.text.clear();
#endif
	return r;
}

} // namespace cpr


