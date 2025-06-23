/**
 * @file main.cpp
 * @brief Test suite for the HttpClient class.
 * @author Your Name
 * @date 2025-06-23
 *
 * This file contains a series of tests to validate the functionality
 * of the HttpClient, including GET/POST requests, error handling,
 * and thread safety.
 */

#include "http_client.hpp"
#include <iostream>
#include <thread> // For std::jthread
#include <vector>
#include <cassert>
#include <functional> // For std::less
#include <format>     // For std::format
#include <fstream>    // For file I/O in tests
#include <cstdio>     // For std::remove

/**
 * @brief Prints the contents of an HttpResponse to the console.
 * @param test_name The name of the test being run.
 * @param response The HttpResponse object to print.
 */
void print_response(const std::string& test_name, const HttpResponse& response) {
    std::cout << std::format("--- {} ---\n", test_name);
    std::cout << std::format("Status Code: {}\n", response.statusCode);
    std::cout << "Headers:\n";
    for (const auto& [key, value] : response.headers) {
        std::cout << std::format("  {}: {}\n", key, value);
    }
    std::cout << std::format("Body:\n{}\n\n", response.body);
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
        std::cerr << std::format("Test 'Simple GET' failed: {}\n", e.what());
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
        std::cerr << std::format("Test 'GET with Headers' failed: {}\n", e.what());
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
        std::cerr << std::format("Test 'Simple POST' failed: {}\n", e.what());
    }
}

/**
 * @brief Tests a multipart/form-data POST request with a field and a file.
 */
void test_multipart_post() {
    const std::string temp_filename = "test_upload_file.txt";
    const std::string file_content = "This is the content of the file to upload.";

    // Create a dummy file for uploading
    {
        std::ofstream outfile(temp_filename);
        outfile << file_content;
    }

    try {
        HttpClient client;
        std::vector<HttpFormPart> parts = {
            {"field1", "value1"},
            {"file1", HttpFormFile{temp_filename, "text/plain"}}
        };
        
        HttpResponse response = client.post("https://httpbin.org/post", parts);
        print_response("Multipart POST", response);
        
        assert(response.statusCode == 200);
        // httpbin returns form fields in a 'form' object
        assert(response.body.find("\"field1\": \"value1\"") != std::string::npos);
        // httpbin returns file content in a 'files' object
        assert(response.body.find(std::format("\"file1\": \"{}\"", file_content)) != std::string::npos);
        
    } catch (const CurlException& e) {
        std::cerr << std::format("Test 'Multipart POST' failed: {}\n", e.what());
    }

    // Clean up the dummy file
    std::remove(temp_filename.c_str());
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
        std::cout << std::format("--- Connection Failure ---\n");
        std::cout << std::format("Successfully caught expected exception: {}\n\n", e.what());
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
        std::cout << std::format("--- Timeout ---\n");
        std::cout << std::format("Successfully caught expected timeout exception: {}\n\n", e.what());
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
        std::cout << std::format("--- Invalid Certificate ---\n");
        std::cout << std::format("Successfully caught expected certificate validation exception: {}\n\n", e.what());
    }
}

/**
 * @brief Tests the thread safety of the HttpClient class by making concurrent requests.
 */
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
            } catch (const CurlException& e) {
                std::cerr << std::format("Thread {} caught an exception: {}\n", i, e.what());
            }
        });
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
    test_multipart_post();
    test_connection_failure();
    test_timeout();
    test_invalid_certificate();
    test_thread_safety();

    std::cout << "All tests finished.\n";
    return 0;
}
