#include "http_client.hpp"
#include <curl/curl.h>
#include <stdexcept>

namespace strategia {

namespace {
size_t write_cb(char *ptr, size_t size, size_t nmemb, void *userdata) {
	std::string *out = static_cast<std::string*>(userdata);
	out->append(ptr, size * nmemb);
	return size * nmemb;
}
}

std::string HttpClient::get(const std::string &url_with_query, long &status_code) {
	CURL *curl = curl_easy_init();
	if (!curl) throw std::runtime_error("curl_easy_init failed");
	std::string body;
	curl_easy_setopt(curl, CURLOPT_URL, url_with_query.c_str());
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
	// reasonable timeouts
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 5000L);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 7000L);
	// TLS defaults
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

	CURLcode rc = curl_easy_perform(curl);
	if (rc != CURLE_OK) {
		curl_easy_cleanup(curl);
		throw std::runtime_error(curl_easy_strerror(rc));
	}
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
	curl_easy_cleanup(curl);
	return body;
}

}


