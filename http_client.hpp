#ifndef HTTP_CLIENT_HPP
#define HTTP_CLIENT_HPP

/**
 * @file http_client.hpp
 * @brief Defines the interface for a modern C++ HTTP client wrapper for libcurl.
 * @author Martin Cordova
 * @date 2025-06-23
 */

#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <optional>
#include <memory>
#include <functional> // For std::less
#include <variant>    // For std::variant

/**
 * @class CurlException
 * @brief Custom exception for libcurl-related errors.
 */
class CurlException : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/**
 * @struct HttpResponse
 * @brief Represents an HTTP response from a server.
 */
struct HttpResponse {
    /// @brief The HTTP status code (e.g., 200 for OK, 404 for Not Found).
    long statusCode;
    /// @brief The body of the HTTP response.
    std::string body;
    /// @brief A map of response headers.
    std::map<std::string, std::string, std::less<>> headers;
};

/**
 * @struct HttpClientConfig
 * @brief Configuration options for an HttpClient instance.
 */
struct HttpClientConfig {
    /// @brief Connection timeout in milliseconds. Defaults to 10000ms.
    long connectTimeoutMs = 10000L;
    /// @brief Total request/read timeout in milliseconds. Defaults to 30000ms.
    long requestTimeoutMs = 30000L;
    /// @brief Optional path to the client SSL certificate file.
    std::optional<std::string> clientCertPath;
    /// @brief Optional path to the client SSL private key file.
    std::optional<std::string> clientKeyPath;
    /// @brief Optional password for the client SSL private key.
    std::optional<std::string> clientKeyPassword;
};

/**
 * @struct HttpFormFile
 * @brief Represents a file to be sent as part of a multipart form.
 */
struct HttpFormFile {
    /// @brief The local path to the file.
    std::string filePath;
    /// @brief The optional MIME type of the file (e.g., "image/jpeg").
    std::optional<std::string> contentType;
};

/**
 * @struct HttpFormPart
 * @brief Represents a single part of a multipart/form-data request.
 */
struct HttpFormPart {
    /// @brief The name of the form field.
    std::string name;
    /// @brief The content of the part, which can be a simple string value or a file.
    std::variant<std::string, HttpFormFile> contents;
};


/**
 * @class HttpClient
 * @brief A modern, thread-safe C++23 wrapper for libcurl for making REST API calls.
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

    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;
    HttpClient(HttpClient&&) noexcept;
    HttpClient& operator=(HttpClient&&) noexcept;

    /**
     * @brief Performs an HTTP GET request.
     * @param url The target URL for the GET request.
     * @param headers A map of request headers to be sent.
     * @return An HttpResponse struct containing the server's response.
     * @throws CurlException on failure.
     */
    [[nodiscard]] HttpResponse get(const std::string& url, const std::map<std::string, std::string, std::less<>>& headers = {}) const;

    /**
     * @brief Performs an HTTP POST request with a raw string body.
     * @param url The target URL for the POST request.
     * @param body The data to be sent in the request body.
     * @param headers A map of request headers. A "Content-Type" header is recommended.
     * @return An HttpResponse struct containing the server's response.
     * @throws CurlException on failure.
     */
    [[nodiscard]] HttpResponse post(const std::string& url, const std::string& body, const std::map<std::string, std::string, std::less<>>& headers = {}) const;
    
    /**
     * @brief Performs a multipart/form-data HTTP POST request.
     * @param url The target URL for the POST request.
     * @param formParts A vector of HttpFormPart objects representing the form fields and files.
     * @param headers A map of request headers. "Content-Type" is handled automatically.
     * @return An HttpResponse struct containing the server's response.
     * @throws CurlException on failure.
     */
    [[nodiscard]] HttpResponse post(const std::string& url, const std::vector<HttpFormPart>& formParts, const std::map<std::string, std::string, std::less<>>& headers = {}) const;

private:
    /// @brief Forward declaration for the private implementation (PImpl idiom).
    class Impl;
    /// @brief A unique pointer to the private implementation.
    std::unique_ptr<Impl> pimpl;
};

#endif // HTTP_CLIENT_HPP
