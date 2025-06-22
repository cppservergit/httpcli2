#ifndef HTTP_CLIENT_HPP
#define HTTP_CLIENT_HPP

#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <optional>
#include <memory>

/**
 * @brief Custom exception for HTTP client-related errors.
 *
 * Thrown for network failures, timeouts, or other libcurl errors.
 */
class HttpException : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/**
 * @brief Represents an HTTP response.
 */
struct HttpResponse {
    long statusCode;
    std::string body;
    std::map<std::string, std::string> headers;
};

/**
 * @brief Configuration for the HttpClient.
 */
struct HttpClientConfig {
    /// Connection timeout in milliseconds. 0 for no timeout.
    long connectTimeoutMs = 10000L;
    /// Total request/read timeout in milliseconds. 0 for no timeout.
    long requestTimeoutMs = 30000L;
    /// Optional path to the client certificate file.
    std::optional<std::string> clientCertPath;
    /// Optional path to the client private key file.
    std::optional<std::string> clientKeyPath;
    /// Optional password for the private key.
    std::optional<std::string> clientKeyPassword;
};

/**
 * @brief A modern C++23 wrapper for libcurl for making REST API calls.
 *
 * This class is thread-safe. Each call to get() or post() uses a new
 * libcurl easy handle, ensuring that requests can be made concurrently
 * from multiple threads without interfering with each other.
 *
 * Libcurl's global state is managed via a static RAII helper to ensure
 * curl_global_init and curl_global_cleanup are called correctly.
 */
class HttpClient {
public:
    /**
     * @brief Constructs an HttpClient with the given configuration.
     * @param config Configuration options for timeouts and certificates.
     */
    explicit HttpClient(HttpClientConfig config = {});

    /**
     * @brief Destructor.
     */
    ~HttpClient();

    // The class is non-copyable to prevent slicing and unclear ownership.
    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;

    // Move semantics are enabled.
    HttpClient(HttpClient&&) noexcept;
    HttpClient& operator=(HttpClient&&) noexcept;

    /**
     * @brief Performs an HTTP GET request.
     * @param url The URL to request.
     * @param headers A map of request headers.
     * @return The HTTP response.
     * @throws HttpException on failure.
     */
    [[nodiscard]] HttpResponse get(const std::string& url, const std::map<std::string, std::string>& headers = {}) const;

    /**
     * @brief Performs an HTTP POST request.
     * @param url The URL to post to.
     * @param body The request body.
     * @param headers A map of request headers. A "Content-Type" header is recommended.
     * @return The HTTP response.
     * @throws HttpException on failure.
     */
    [[nodiscard]] HttpResponse post(const std::string& url, const std::string& body, const std::map<std::string, std::string>& headers = {}) const;

private:
    // PImpl idiom to hide libcurl details from the header.
    class Impl;
    std::unique_ptr<Impl> pimpl;
};

#endif // HTTP_CLIENT_HPP
