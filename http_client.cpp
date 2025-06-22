#include "http_client.hpp"
#include <curl/curl.h>
#include <iostream>
#include <utility>
#include <limits>
#include <functional> // For std::less

// RAII wrapper for curl_global_init and curl_global_cleanup.
// This ensures that libcurl is initialized before first use and cleaned up at exit.
class CurlGlobalInitializer {
public:
    CurlGlobalInitializer() {
        // CURL_GLOBAL_DEFAULT is fine for most cases.
        // For SSL and thread safety, CURL_GLOBAL_SSL is a good choice.
        CURLcode res = curl_global_init(CURL_GLOBAL_DEFAULT);
        if (res != CURLE_OK) {
            throw std::runtime_error("Failed to initialize libcurl globally.");
        }
    }

    ~CurlGlobalInitializer() {
        curl_global_cleanup();
    }

    // Delete copy and move semantics
    CurlGlobalInitializer(const CurlGlobalInitializer&) = delete;
    CurlGlobalInitializer& operator=(const CurlGlobalInitializer&) = delete;
    CurlGlobalInitializer(CurlGlobalInitializer&&) = delete;
    CurlGlobalInitializer& operator=(CurlGlobalInitializer&&) = delete;
};

// A single static instance will be created, ensuring initialization happens once.
static CurlGlobalInitializer global_curl_initializer;


// Private implementation class (PImpl Idiom)
class HttpClient::Impl {
public:
    explicit Impl(HttpClientConfig config) : m_config(std::move(config)) {}

    [[nodiscard]] HttpResponse performRequest(const std::string& url,
                                              const std::optional<std::string>& postBody,
                                              const std::map<std::string, std::string, std::less<>>& headers) const;
private:
    HttpClientConfig m_config;

    // C-style callbacks that are SonarCloud/guideline-friendly.
    // They are static and use a userdata pointer to interact with C++ state.
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

    static size_t headerCallback(char* buffer, size_t size, size_t nitems, void* userdata) {
        if (userdata == nullptr) {
            return 0;
        }
        auto* responseHeaders = static_cast<std::map<std::string, std::string, std::less<>>*>(userdata);
        std::string header(buffer, size * nitems);
        
        // Use if-statement with initializer (C++17) to scope colon_pos
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
        throw HttpException("Failed to create CURL easy handle.");
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

    // Set URL
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

    // Set timeouts
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, m_config.connectTimeoutMs);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, m_config.requestTimeoutMs);
    
    // **SECURITY**: Enforce a minimum of TLS 1.2
    curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);

    // Follow redirects
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, FOLLOW_REDIRECTS);
    
    // Provide a user agent
    curl_easy_setopt(curl, CURLOPT_USERAGENT, USER_AGENT);

    // Set callbacks for response body and headers
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
    header_slist.reset(current_slist); // transfer ownership to unique_ptr
    if (header_slist) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_slist.get());
    }

    // Configure for POST if a body is provided
    if (postBody) {
        const size_t body_length = postBody->length();
        // **SAFETY**: Prevent narrowing conversion from size_t to long if body is too large.
        if (body_length > static_cast<size_t>(std::numeric_limits<long>::max())) {
            throw HttpException("POST body is too large to be handled by libcurl.");
        }
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postBody->c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body_length));
    }

    // Perform the request
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        throw HttpException("curl_easy_perform() failed: " + std::string(curl_easy_strerror(res)));
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
