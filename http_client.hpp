#ifndef HTTP_CLIENT_HPP
#define HTTP_CLIENT_HPP

/**
 * @file http_client.hpp
 * @brief Defines the interface for a modern C++ HTTP client wrapper for libcurl.
 * @author Martin Cordova
 * @date 2025-06-22
 */

#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <optional>
#include <memory>
#include <functional> // For std::less

/**
 * @class CurlException
 * @brief Custom exception for libcurl-related errors.
 *
 * This exception is thrown for network failures, timeouts, or other errors
 * encountered during a libcurl operation. It inherits from std::runtime_error.
 */
class CurlException : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/**
 * @struct HttpResponse
 * @brief Represents an HTTP response from a server.
 *
 * Contains the status code, response body, and headers returned by the server.
 */
struct HttpResponse {
    /// @brief The HTTP status code (e.g., 200 for OK, 404 for Not Found).
    long statusCode;
    /// @brief The body of the HTTP response.
    std::string body;
    /// @brief A map of response headers. Keys are header names, values are header values.
    /// @note Uses std::less<> for transparent lookups.
    std::map<std::string, std::string, std::less<>> headers;
};

/**
 * @struct HttpClientConfig
 * @brief Configuration options for an HttpClient instance.
 *
 * Allows for setting timeouts and client-side SSL/TLS certificates.
 */
struct HttpClientConfig {
    /// @brief Connection timeout in milliseconds. Defaults to 10000ms. Set to 0 for no timeout.
    long connectTimeoutMs = 10000L;
    /// @brief Total request/read timeout in milliseconds. Defaults to 30000ms. Set to 0 for no timeout.
    long requestTimeoutMs = 30000L;
    /// @brief Optional path to the client SSL certificate file (e.g., in PEM format).
    std::optional<std::string> clientCertPath;
    /// @brief Optional path to the client SSL private key file.
    std::optional<std::string> clientKeyPath;
    /// @brief Optional password for the client SSL private key.
    std::optional<std::string> clientKeyPassword;
};

/**
 * @class HttpClient
 * @brief A modern, thread-safe C++23 wrapper for libcurl for making REST API calls.
 *
 * This class provides a simple interface for making HTTP GET and POST requests.
 * It is designed to be thread-safe by creating a new libcurl easy handle for each request.
 * Global libcurl state is managed via an internal RAII object.
 *
 * @note This class is move-only to ensure clear ownership semantics.
 */
class HttpClient {
public:
    /**
     * @brief Constructs an HttpClient with optional custom configuration.
     * @param config A HttpClientConfig struct with desired settings.
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
     * @param url The target URL for the GET request.
     * @param headers A map of request headers to be sent.
     * @return An HttpResponse struct containing the server's response.
     * @throws CurlException on network or libcurl-related failures.
     */
    [[nodiscard]] HttpResponse get(const std::string& url, const std::map<std::string, std::string, std::less<>>& headers = {}) const;

    /**
     * @brief Performs an HTTP POST request.
     * @param url The target URL for the POST request.
     * @param body The data to be sent in the request body.
     * @param headers A map of request headers to be sent. A "Content-Type" header is recommended.
     * @return An HttpResponse struct containing the server's response.
     * @throws CurlException on network or libcurl-related failures.
     */
    [[nodiscard]] HttpResponse post(const std::string& url, const std::string& body, const std::map<std::string, std::string, std::less<>>& headers = {}) const;

private:
    /// @brief Forward declaration for the private implementation (PImpl idiom).
    class Impl;
    /// @brief A unique pointer to the private implementation.
    std::unique_ptr<Impl> pimpl;
};

#endif // HTTP_CLIENT_HPP
