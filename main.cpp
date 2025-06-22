#include "http_client.hpp"
#include <iostream>
#include <thread> // For std::jthread
#include <vector>
#include <cassert>
#include <functional> // For std::less
#include <format>     // For std::format

void print_response(const std::string& test_name, const HttpResponse& response) {
    std::cout << std::format("--- {} ---\n", test_name);
    std::cout << std::format("Status Code: {}\n", response.statusCode);
    std::cout << "Headers:\n";
    for (const auto& [key, value] : response.headers) {
        std::cout << std::format("  {}: {}\n", key, value);
    }
    std::cout << std::format("Body:\n{}\n\n", response.body);
}

void test_simple_get() {
    try {
        HttpClient client;
        HttpResponse response = client.get("https://httpbin.org/get");
        print_response("Simple GET", response);
        assert(response.statusCode == 200);
        assert(!response.body.empty());
    } catch (const HttpException& e) {
        std::cerr << std::format("Test 'Simple GET' failed: {}\n", e.what());
    }
}

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
    } catch (const HttpException& e) {
        std::cerr << std::format("Test 'GET with Headers' failed: {}\n", e.what());
    }
}

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
    } catch (const HttpException& e) {
        std::cerr << std::format("Test 'Simple POST' failed: {}\n", e.what());
    }
}

void test_connection_failure() {
    try {
        HttpClient client;
        (void)client.get("http://192.0.2.1/test"); 
        std::cerr << "Test 'Connection Failure' failed: Expected an exception, but none was thrown.\n";
    } catch (const HttpException& e) {
        std::cout << std::format("--- Connection Failure ---\n");
        std::cout << std::format("Successfully caught expected exception: {}\n\n", e.what());
    }
}

void test_timeout() {
    try {
        HttpClientConfig config;
        config.requestTimeoutMs = 1000L;
        HttpClient client(config);
        (void)client.get("https://httpbin.org/delay/3");
        std::cerr << "Test 'Timeout' failed: Expected a timeout exception, but none was thrown.\n";
    } catch (const HttpException& e) {
        std::cout << std::format("--- Timeout ---\n");
        std::cout << std::format("Successfully caught expected timeout exception: {}\n\n", e.what());
    }
}

void test_invalid_certificate() {
    try {
        HttpClient client;
        (void)client.get("https://self-signed.badssl.com/");
        std::cerr << "Test 'Invalid Certificate' failed: Expected an exception, but none was thrown.\n";
    } catch (const HttpException& e) {
        std::cout << std::format("--- Invalid Certificate ---\n");
        std::cout << std::format("Successfully caught expected certificate validation exception: {}\n\n", e.what());
    }
}

void test_thread_safety() {
    std::cout << "--- Thread Safety ---\n";
    
    const HttpClient client;
    // Use std::jthread for automatic joining on destruction.
    std::vector<std::jthread> threads;
    const int num_threads = 10;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&client, i]() {
            try {
                auto response = client.get(std::format("https://httpbin.org/get?thread={}", i));
                if (response.statusCode == 200) {
                    std::cout << std::format("Thread {} completed successfully.\n", i);
                    assert(response.body.find(std::format("thread={}", i)) != std::string::npos);
                } else {
                    std::cerr << std::format("Thread {} failed with status code {}.\n", i, response.statusCode);
                }
            } catch (const HttpException& e) {
                std::cerr << std::format("Thread {} caught an exception: {}\n", i, e.what());
            }
        });
    }

    // No explicit join loop is needed. The std::jthread destructors
    // will be called when 'threads' goes out of scope, automatically joining them.
    
    std::cout << "Thread safety test completed.\n\n";
}

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
