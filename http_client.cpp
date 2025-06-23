/**
 * @file http_client.cpp
 * @brief Implements the HttpClient class, a modern C++ wrapper for libcurl.
 * @author Martin Cordova
 * @date 2025-06-22
 */

#include "http_client.hpp"
#include <curl/curl.h>
#include <iostream>
#include <utility>
#include <limits>
#include <functional> // For std::less
#include <cstdlib> // for std::abort

namespace {

/**
 * @class CurlGlobalInitializer
 * @brief An RAII wrapper to manage libcurl's global state.
 *
 * This class ensures that `curl_global_init()` is called before any other
 * libcurl function and `curl_global_cleanup()` is called at program exit.
 * An instance of this class is created as a global constant, guaranteeing
 * proper initialization and cleanup.
 */
class CurlGlobalInitializer {
public:
    /**
     * @brief Constructor that initializes libcurl.
     */
    CurlGlobalInitializer() {
        // Use if-statement with initializer (C++17) to scope 'res'
        if (CURLcode res = curl_global_init(CURL_GLOBAL_DEFAULT); res != CURLE_OK) {
            // It's unsafe to throw from a global object's constructor before main(),
            // as it can lead to std::terminate. A message to stderr and aborting is safer.
            std::cerr << "Fatal: Failed to initialize libcurl globally." << std::endl;
            std::abort();
        }
    }

    /**
     * @brief Destructor that cleans up libcurl's global state.
     */
    ~CurlGlobalInitializer() {
        curl_global_cleanup();
    }

    // Delete copy and move semantics to ensure singleton-like behavior.
    CurlGlobalInitializer(const CurlGlobalInitializer&) = delete;
    CurlGlobalInitializer& operator=(const CurlGlobalInitializer&) = delete;
    CurlGlobalInitializer(CurlGlobalInitializer&&) = delete;
    CurlGlobalInitializer& operator=(CurlGlobalInitializer&&) = delete;
};

/// @brief The single global instance that manages libcurl's lifetime.
const CurlGlobalInitializer global_curl_initializer;

} // namespace


/**
 * @class HttpClient::Impl
 * @brief The private implementation of the HttpClient.
 *
 * This class encapsulates all libcurl details, hiding them from the public
 * interface (PImpl idiom).
 */
class HttpClient::Impl {
public:
    explicit Impl(HttpClientConfig config) : m_config(std::move(config)) {}

    [[nodiscard]] HttpResponse performRequest(const std::string& url,
                                              const std::optional<std::string>& postBody,
                                              const std::map<std::string, std::string, std::less<>>& headers) const;
private:
    HttpClientConfig m_config;

    /**
     * @brief A static libcurl callback function to handle response body data.
     * @param ptr Pointer to the received data chunk.
     * @param size Always 1.
     * @param nmemb Size of the data chunk.
     * @param userdata A void pointer to a user-provided object (here, a std::string).
     * @return The number of bytes handled.
     */
    static size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
        if (userdata == nullptr) {
            return 0;
        }
        auto* responseBody = static_cast<std::string*>(userdata);
        const size_t total_size = size * nmemb;
        try {
            responseBody->append(ptr, total_size);
        } catch (const std::bad_alloc&) {
            // Handle memory allocation failure if the response is huge
            return 0; 
        }
        return total_size;
    }

    /**
     * @brief A static libcurl callback function to handle response header data.
     * @param buffer Pointer to the received header line.
     * @param size Always 1.
     * @param nitems Size of the header line.
     * @param userdata A void pointer to a user-provided object (here, a std::map).
     * @return The number of bytes handled.
     */
    static size_t headerCallback(char* buffer, size_t size, size_t nitems, void* userdata) {
        if (userdata == nullptr) {
            return 0;
        }
        auto* responseHeaders = static_cast<std::map<std::string, std::string, std::less<>>*>(userdata);
        std::string header(buffer, size * nitems);
        
        if (const size_t colon_pos = header.find(':'); colon_pos != std::string::npos) {
            std::string key = header.substr(0, colon_pos);
            std::string value = header.substr(colon_pos + 1);

            // Trim leading/trailing whitespace and carriage returns
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t\r\n") + 1);

            (*responseHeaders)[key] = value;
        }
        return size * nitems;
    }
};


[[nodiscard]] HttpResponse HttpClient::Impl::performRequest(const std::string& url,
                                                            const std::optional<std::string>& postBody,
                                                            const std::map<std::string, std::string, std::less<>>& headers) const {
    // Each request gets its own handle for thread safety.
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw CurlException("Failed to create CURL easy handle.");
    }

    // Using unique_ptr for RAII cleanup of CURL and curl_slist
    auto curl_deleter = [](CURL* c) { curl_easy_cleanup(c); };
    std::unique_ptr<CURL, decltype(curl_deleter)> curl_ptr(curl, curl_deleter);

    auto slist_deleter = [](curl_slist* sl) { curl_slist_free_all(sl); };
    std::unique_ptr<curl_slist, decltype(slist_deleter)> header_slist(nullptr, slist_deleter);

    HttpResponse response;
    
    // --- Constants for settings ---
    static constexpr const char* USER_AGENT = "cpp-http-client/1.0";
    static constexpr long FOLLOW_REDIRECTS = 1L;

    // --- Configure CURL options ---
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, m_config.connectTimeoutMs);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, m_config.requestTimeoutMs);
    curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, FOLLOW_REDIRECTS);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, USER_AGENT);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response.headers);
    
    // Configure client certificates if provided
    if (m_config.clientCertPath) {
        curl_easy_setopt(curl, CURLOPT_SSLCERT, m_config.clientCertPath->c_str());
    }
    if (m_config.clientKeyPath) {
        curl_easy_setopt(curl, CURLOPT_SSLKEY, m_config.clientKeyPath->c_str());
    }
    if (m_config.clientKeyPassword) {
        curl_easy_setopt(curl, CURLOPT_KEYPASSWD, m_config.clientKeyPassword->c_str());
    }

    // Set custom headers
    curl_slist* current_slist = nullptr;
    for (const auto& [key, value] : headers) {
        std::string header_string = key + ": " + value;
        current_slist = curl_slist_append(current_slist, header_string.c_str());
    }
    header_slist.reset(current_slist);
    if (header_slist) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_slist.get());
    }

    // Configure for POST if a body is provided
    if (postBody) {
        const size_t body_length = postBody->length();
        if (body_length > static_cast<size_t>(std::numeric_limits<long>::max())) {
            throw CurlException("POST body is too large to be handled by libcurl.");
        }
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postBody->c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body_length));
    }

    // Perform the request
    if (CURLcode res = curl_easy_perform(curl); res != CURLE_OK) {
        throw CurlException(std::string("curl_easy_perform() failed: ") + curl_easy_strerror(res));
    }

    // Get the response status code
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.statusCode);

    return response;
}

// --- Public HttpClient methods ---

HttpClient::HttpClient(HttpClientConfig config)
    : pimpl(std::make_unique<Impl>(std::move(config))) {}

HttpClient::~HttpClient() = default;
HttpClient::HttpClient(HttpClient&&) noexcept = default;
HttpClient& HttpClient::operator=(HttpClient&&) noexcept = default;

[[nodiscard]] HttpResponse HttpClient::get(const std::string& url, const std::map<std::string, std::string, std::less<>>& headers) const {
    return pimpl->performRequest(url, std::nullopt, headers);
}

[[nodiscard]] HttpResponse HttpClient::post(const std::string& url, const std::string& body, const std::map<std::string, std::string, std::less<>>& headers) const {
    return pimpl->performRequest(url, body, headers);
}
