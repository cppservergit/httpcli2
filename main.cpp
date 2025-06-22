#include "http_client.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <cassert>

void print_response(const std::string& test_name, const HttpResponse& response) {
    std::cout << "--- " << test_name << " ---\n";
    std::cout << "Status Code: " << response.statusCode << "\n";
    std::cout << "Headers:\n";
    for (const auto& [key, value] : response.headers) {
        std::cout << "  " << key << ": " << value << "\n";
    }
    std::cout << "Body:\n" << response.body << "\n\n";
}

void test_simple_get() {
    try {
        HttpClient client;
        HttpResponse response = client.get("https://httpbin.org/get");
        print_response("Simple GET", response);
        assert(response.statusCode == 200);
        assert(!response.body.empty());
    } catch (const HttpException& e) {
        std::cerr << "Test 'Simple GET' failed: " << e.what() << '\n';
    }
}

void test_get_with_headers() {
    try {
        HttpClient client;
        std::map<std::string, std::string> headers = {
            {"X-My-Header", "Hello C++23"},
            {"Accept", "application/json"}
        };
        HttpResponse response = client.get("https://httpbin.org/headers", headers);
        print_response("GET with Headers", response);
        assert(response.statusCode == 200);
        assert(response.body.find("Hello C++23") != std::string::npos);
    } catch (const HttpException& e) {
        std::cerr << "Test 'GET with Headers' failed: " << e.what() << '\n';
    }
}

void test_simple_post() {
    try {
        HttpClient client;
        std::string post_body = R"({"name": "test", "value": 42})";
        std::map<std::string, std::string> headers = {
            {"Content-Type", "application/json"}
        };
        HttpResponse response = client.post("https://httpbin.org/post", post_body, headers);
        print_response("Simple POST", response);
        assert(response.statusCode == 200);
        
        // The httpbin/post response escapes quotes in the "data" field.
        // It's more robust to check for the key-value pairs in the "json" field.
        assert(response.body.find("\"name\": \"test\"") != std::string::npos);
        assert(response.body.find("\"value\": 42") != std::string::npos);

        assert(response.body.find("application/json") != std::string::npos);
    } catch (const HttpException& e) {
        std::cerr << "Test 'Simple POST' failed: " << e.what() << '\n';
    }
}

void test_connection_failure() {
    try {
        HttpClient client;
        // This address is reserved for documentation and should not be routable.
        // We cast to (void) to explicitly ignore the [[nodiscard]] return value.
        (void)client.get("http://192.0.2.1/test"); 
        std::cerr << "Test 'Connection Failure' failed: Expected an exception, but none was thrown.\n";
    } catch (const HttpException& e) {
        std::cout << "--- Connection Failure ---\n";
        std::cout << "Successfully caught expected exception: " << e.what() << "\n\n";
    }
}

void test_timeout() {
    try {
        // Configure a very short timeout.
        HttpClientConfig config;
        config.requestTimeoutMs = 1000L; // 1 second
        HttpClient client(config);

        // We cast to (void) to explicitly ignore the [[nodiscard]] return value.
        (void)client.get("https://httpbin.org/delay/3");
        std::cerr << "Test 'Timeout' failed: Expected a timeout exception, but none was thrown.\n";
    } catch (const HttpException& e) {
        std::cout << "--- Timeout ---\n";
        std::cout << "Successfully caught expected timeout exception: " << e.what() << "\n\n";
    }
}

void test_invalid_certificate() {
    try {
        HttpClient client;
        // self-signed.badssl.com uses a self-signed certificate which should be rejected.
        (void)client.get("https://self-signed.badssl.com/");
        std::cerr << "Test 'Invalid Certificate' failed: Expected an exception, but none was thrown.\n";
    } catch (const HttpException& e) {
        std::cout << "--- Invalid Certificate ---\n";
        std::cout << "Successfully caught expected certificate validation exception: " << e.what() << "\n\n";
    }
}

void test_thread_safety() {
    std::cout << "--- Thread Safety ---\n";
    
    // Create one client instance shared across all threads.
    const HttpClient client;
    std::vector<std::thread> threads;
    const int num_threads = 10;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&client, i]() {
            try {
                // Each thread sends a slightly different request.
                auto response = client.get("https://httpbin.org/get?thread=" + std::to_string(i));
                if (response.statusCode == 200) {
                    std::cout << "Thread " << i << " completed successfully.\n";
                    assert(response.body.find("thread=" + std::to_string(i)) != std::string::npos);
                } else {
                    std::cerr << "Thread " << i << " failed with status code " << response.statusCode << ".\n";
                }
            } catch (const HttpException& e) {
                std::cerr << "Thread " << i << " caught an exception: " << e.what() << '\n';
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }
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
