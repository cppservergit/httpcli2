/**
 * @file http_client.cpp
 * @brief Implements the HttpClient class, a modern C++ wrapper for libcurl.
 * @author Martin Cordova
 * @date 2025-06-23
 */

#include "http_client.hpp"
#include <curl/curl.h>
#include <iostream>
#include <utility>
#include <limits>
#include <functional> // For std::less
#include <cstdlib>    // for std::abort

namespace {

// RAII wrapper for curl_global_init and curl_global_cleanup.
class CurlGlobalInitializer {
public:
    CurlGlobalInitializer() {
        if (CURLcode res = curl_global_init(CURL_GLOBAL_DEFAULT); res != CURLE_OK) {
            std::cerr << "Fatal: Failed to initialize libcurl globally." << std::endl;
            std::abort();
        }
    }
    ~CurlGlobalInitializer() {
        curl_global_cleanup();
    }
    CurlGlobalInitializer(const CurlGlobalInitializer&) = delete;
    CurlGlobalInitializer& operator=(const CurlGlobalInitializer&) = delete;
    CurlGlobalInitializer(CurlGlobalInitializer&&) = delete;
    CurlGlobalInitializer& operator=(CurlGlobalInitializer&&) = delete;
};

const CurlGlobalInitializer global_curl_initializer;

} // namespace


// Private implementation of the HttpClient.
class HttpClient::Impl {
public:
    explicit Impl(HttpClientConfig config) : m_config(std::move(config)) {}

    [[nodiscard]] HttpResponse performRequest(const std::string& url,
                                              const std::optional<std::string>& postBody,
                                              const std::optional<std::vector<HttpFormPart>>& formParts,
                                              const std::map<std::string, std::string, std::less<>>& headers) const;
private:
    HttpClientConfig m_config;

    // Helper functions to refactor performRequest
    void configure_common_options(/* NOSONAR */ CURL* curl, const std::string& url, HttpResponse& response) const;
    [[nodiscard]] curl_slist* build_headers(const std::map<std::string, std::string, std::less<>>& headers) const;
    void configure_post_body(/* NOSONAR */ CURL* curl, const std::string& postBody) const;
    [[nodiscard]] curl_mime* build_multipart_form(/* NOSONAR */ CURL* curl, const std::vector<HttpFormPart>& formParts) const;
    
    // Callbacks for libcurl
    static size_t writeCallback(const char* ptr, size_t size, size_t nmemb, /* NOSONAR */ void* userdata);
    static size_t headerCallback(const char* buffer, size_t size, size_t nitems, /* NOSONAR */ void* userdata);
};

// --- Callback Implementations ---

size_t HttpClient::Impl::writeCallback(const char* ptr, size_t size, size_t nmemb, /* NOSONAR */ void* userdata) {
    if (userdata == nullptr) return 0;
    auto* responseBody = static_cast<std::string*>(userdata);
    const size_t total_size = size * nmemb;
    try {
        responseBody->append(ptr, total_size);
    } catch (const std::bad_alloc&) {
        return 0; 
    }
    return total_size;
}

size_t HttpClient::Impl::headerCallback(const char* buffer, size_t size, size_t nitems, /* NOSONAR */ void* userdata) {
    if (userdata == nullptr) return 0;
    auto* responseHeaders = static_cast<std::map<std::string, std::string, std::less<>>*>(userdata);
    std::string header(buffer, size * nitems);
    if (const size_t colon_pos = header.find(':'); colon_pos != std::string::npos) {
        std::string key = header.substr(0, colon_pos);
        std::string value = header.substr(colon_pos + 1);
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t\r\n") + 1);
        (*responseHeaders)[key] = value;
    }
    return size * nitems;
}

// --- Refactored Helper Functions ---

void HttpClient::Impl::configure_common_options(/* NOSONAR */ CURL* curl, const std::string& url, HttpResponse& response) const {
    static constexpr const char* USER_AGENT = "cpp-http-client/1.0";
    static constexpr long FOLLOW_REDIRECTS = 1L;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, m_config.connectTimeoutMs);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, m_config.requestTimeoutMs);
    curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2); //NOSONAR
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, FOLLOW_REDIRECTS);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, USER_AGENT);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response.headers);
    
    if (m_config.clientCertPath) curl_easy_setopt(curl, CURLOPT_SSLCERT, m_config.clientCertPath->c_str());
    if (m_config.clientKeyPath) curl_easy_setopt(curl, CURLOPT_SSLKEY, m_config.clientKeyPath->c_str());
    if (m_config.clientKeyPassword) curl_easy_setopt(curl, CURLOPT_KEYPASSWD, m_config.clientKeyPassword->c_str());
}

[[nodiscard]] curl_slist* HttpClient::Impl::build_headers(const std::map<std::string, std::string, std::less<>>& headers) const {
    curl_slist* header_list = nullptr;
    for (const auto& [key, value] : headers) {
        std::string header_string = key + ": " + value;
        header_list = curl_slist_append(header_list, header_string.c_str());
    }
    return header_list;
}

void HttpClient::Impl::configure_post_body(/* NOSONAR */ CURL* curl, const std::string& postBody) const {
    const size_t body_length = postBody.length();
    if (body_length > static_cast<size_t>(std::numeric_limits<long>::max())) {
        throw CurlException("POST body is too large to be handled by libcurl.");
    }
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postBody.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body_length));
}

[[nodiscard]] curl_mime* HttpClient::Impl::build_multipart_form(/* NOSONAR */ CURL* curl, const std::vector<HttpFormPart>& formParts) const {
    curl_mime* mime = curl_mime_init(curl);
    if (!mime) {
        throw CurlException("curl_mime_init() failed.");
    }

    for (const auto& part : formParts) {
        curl_mimepart* mime_part = curl_mime_addpart(mime);
        curl_mime_name(mime_part, part.name.c_str());

        if (std::holds_alternative<std::string>(part.contents)) {
            const auto& value = std::get<std::string>(part.contents);
            curl_mime_data(mime_part, value.c_str(), value.length());
        } else {
            const auto& file = std::get<HttpFormFile>(part.contents);
            curl_mime_filedata(mime_part, file.filePath.c_str());
            if (file.contentType) {
                curl_mime_type(mime_part, file.contentType->c_str());
            }
        }
    }
    return mime;
}


// --- Main Request Function ---

[[nodiscard]] HttpResponse HttpClient::Impl::performRequest(const std::string& url,
                                                            const std::optional<std::string>& postBody,
                                                            const std::optional<std::vector<HttpFormPart>>& formParts,
                                                            const std::map<std::string, std::string, std::less<>>& headers) const {
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw CurlException("Failed to create CURL easy handle.");
    }

    // RAII for all libcurl resources
    auto curl_deleter = [](CURL* c) { curl_easy_cleanup(c); };
    std::unique_ptr<CURL, decltype(curl_deleter)> curl_ptr(curl, curl_deleter); //NOSONAR

    auto slist_deleter = [](curl_slist* sl) { curl_slist_free_all(sl); };
    std::unique_ptr<curl_slist, decltype(slist_deleter)> header_slist_ptr(build_headers(headers), slist_deleter); //NOSONAR
    
    auto mime_deleter = [](curl_mime* m) { if(m) curl_mime_free(m); };
    std::unique_ptr<curl_mime, decltype(mime_deleter)> mime_ptr(nullptr, mime_deleter); //NOSONAR
    
    HttpResponse response;
    
    // Step 1: Configure all common options
    configure_common_options(curl, url, response);

    // Step 2: Set headers
    if (header_slist_ptr) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_slist_ptr.get());
    }

    // Step 3: Configure POST data (if any)
    if (formParts) {
        mime_ptr.reset(build_multipart_form(curl, *formParts));
        curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime_ptr.get());
    } else if (postBody) {
        configure_post_body(curl, *postBody);
    }

    // Step 4: Perform the request
    if (CURLcode res = curl_easy_perform(curl); res != CURLE_OK) {
        throw CurlException(std::string("curl_easy_perform() failed: ") + curl_easy_strerror(res));
    }

    // Step 5: Retrieve the status code
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.statusCode);
    
    return response;
}

// --- Public HttpClient methods ---

HttpClient::HttpClient(HttpClientConfig config) : pimpl(std::make_unique<Impl>(std::move(config))) {}
HttpClient::~HttpClient() = default;
HttpClient::HttpClient(HttpClient&&) noexcept = default;
HttpClient& HttpClient::operator=(HttpClient&&) noexcept = default;

[[nodiscard]] HttpResponse HttpClient::get(const std::string& url, const std::map<std::string, std::string, std::less<>>& headers) const {
    return pimpl->performRequest(url, std::nullopt, std::nullopt, headers);
}

[[nodiscard]] HttpResponse HttpClient::post(const std::string& url, const std::string& body, const std::map<std::string, std::string, std::less<>>& headers) const {
    return pimpl->performRequest(url, body, std::nullopt, headers);
}

[[nodiscard]] HttpResponse HttpClient::post(const std::string& url, const std::vector<HttpFormPart>& formParts, const std::map<std::string, std::string, std::less<>>& headers) const {
    return pimpl->performRequest(url, std::nullopt, formParts, headers);
}
