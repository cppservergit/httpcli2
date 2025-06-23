/**
 * @file main.cpp
 * @brief Test suite for the HttpClient class.
 * @author Martin Cordova
 * @date 2025-06-22
 *
 * This file contains a series of tests to validate the functionality
 * of the HttpClient, including GET/POST requests, error handling,
 * and thread safety.
 */

#include "http_client.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <cassert>
#include <functional> // For std::less

/**
 * @brief Prints the contents of an HttpResponse to the console.
 * @param test_name The name of the test being run.
 * @param response The HttpResponse object to print.
 */
void print_response(const std::string& test_name, const HttpResponse& response) {
    std::cout << "--- " << test_name << " ---\n";
    std::cout << "Status Code: " << response.statusCode << "\n";
    std::cout << "Headers:\n";
    for (const auto& [key, value] : response.headers) {
        std::cout << "  " << key << ": " << value << "\n";
    }
    std::cout << "Body:\n" << response.body << "\n\n";
}

/**
 * @brief Tests a simple HTTP GET request.
 */
void test_simple_get() {
    try {
        HttpClient client;
        HttpResponse response = client.get("https://httpbin.org/get");
        print_response("Simple GET", response);
        assert(response.statusCode == 200);
        assert(!response.body.empty());
    } catch (const CurlException& e) {
        std::cerr << "Test 'Simple GET' failed: " << e.what() << '\n';
    }
}

/**
 * @brief Tests an HTTP GET request with custom headers.
 */
void test_get_with_headers() {
    try {
        HttpClient client;
        std::map<std::string, std::string, std::less<>> headers = {
            {"X-My-Header", "Hello C++23"},
            {"Accept", "application/json"}
        };
        HttpResponse response = client.get("https://httpbin.org/headers", headers);
        print_response("GET with Headers", response);
        assert(response.statusCode == 200);
        assert(response.body.find("Hello C++23") != std::string::npos);
    } catch (const CurlException& e) {
        std::cerr << "Test 'GET with Headers' failed: " << e.what() << '\n';
    }
}

/**
 * @brief Tests a simple HTTP POST request with a JSON body.
 */
void test_simple_post() {
    try {
        HttpClient client;
        std::string post_body = R"({"name": "test", "value": 42})";
        std::map<std::string, std::string, std::less<>> headers = {
            {"Content-Type", "application/json"}
        };
        HttpResponse response = client.post("https://httpbin.org/post", post_body, headers);
        print_response("Simple POST", response);
        assert(response.statusCode == 200);
        
        assert(response.body.find("\"name\": \"test\"") != std::string::npos);
        assert(response.body.find("\"value\": 42") != std::string::npos);
        assert(response.body.find("application/json") != std::string::npos);
    } catch (const CurlException& e) {
        std::cerr << "Test 'Simple POST' failed: " << e.what() << '\n';
    }
}

/**
 * @brief Tests connection failure to a non-routable address.
 */
void test_connection_failure() {
    try {
        HttpClient client;
        (void)client.get("http://192.0.2.1/test"); 
        std::cerr << "Test 'Connection Failure' failed: Expected an exception, but none was thrown.\n";
    } catch (const CurlException& e) {
        std::cout << "--- Connection Failure ---\n";
        std::cout << "Successfully caught expected exception: " << e.what() << "\n\n";
    }
}

/**
 * @brief Tests the request timeout functionality.
 */
void test_timeout() {
    try {
        HttpClientConfig config;
        config.requestTimeoutMs = 1000L;
        HttpClient client(config);
        (void)client.get("https://httpbin.org/delay/3");
        std::cerr << "Test 'Timeout' failed: Expected a timeout exception, but none was thrown.\n";
    } catch (const CurlException& e) {
        std::cout << "--- Timeout ---\n";
        std::cout << "Successfully caught expected timeout exception: " << e.what() << "\n\n";
    }
}

/**
 * @brief Tests failure when connecting to a server with an invalid SSL certificate.
 */
void test_invalid_certificate() {
    try {
        HttpClient client;
        (void)client.get("https://self-signed.badssl.com/");
        std::cerr << "Test 'Invalid Certificate' failed: Expected an exception, but none was thrown.\n";
    } catch (const CurlException& e) {
        std::cout << "--- Invalid Certificate ---\n";
        std::cout << "Successfully caught expected certificate validation exception: " << e.what() << "\n\n";
    }
}

/**
 * @brief Tests the thread safety of the HttpClient class by making concurrent requests.
 */
void test_thread_safety() {
    std::cout << "--- Thread Safety ---\n";
    
    const HttpClient client;
    std::vector<std::thread> threads;
    const int num_threads = 10;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&client, i]() {
            try {
                auto response = client.get("https://httpbin.org/get?thread=" + std::to_string(i));
                if (response.statusCode == 200) {
                    std::cout << "Thread " << i << " completed successfully.\n";
                    assert(response.body.find("thread=" + std::to_string(i)) != std::string::npos);
                } else {
                    std::cerr << "Thread " << i << " failed with status code " << response.statusCode << ".\n";
                }
            } catch (const CurlException& e) {
                std::cerr << "Thread " << i << " caught an exception: " << e.what() << '\n';
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }
    std::cout << "Thread safety test completed.\n\n";
}

/**
 * @brief Main entry point for the test application.
 * @return 0 on successful execution.
 */
int main() {
    test_simple_get();
    test_get_with_headers();
    test_simple_post();
    test_connection_failure();
    test_timeout();
    test_invalid_certificate();
    test_thread_safety();

    std::cout << "All tests finished.\n";
    return 0;
}
