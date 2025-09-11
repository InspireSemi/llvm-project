/**
 * @file test_utilities.hpp
 * @brief Shared utilities for ThunderbirdQEMU tests and benchmarks
 * @date 2025-07-27
 * 
 * This header provides clean, reusable utilities for testing with
 * separated concerns and no unnecessary complexity.
 * 
 * @copyright Copyright (c) 2025 InspireSemi
 */

#pragma once

#include <vector>
#include <string>
#include <memory>
#include <random>
#include <atomic>
#include <functional>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <algorithm>

namespace TestUtils {

// =============================================================================
// FILE MANAGEMENT
// =============================================================================

/**
 * @brief Simple file manager with automatic cleanup
 * Single responsibility: file creation and cleanup only
 */
class FileManager {
private:
    std::vector<std::string> managed_files_;
    static std::atomic<size_t> file_counter_;
    
    /**
     * @brief Helper to generate unique file path and open file
     */
    std::pair<std::string, std::ofstream> createFileHandle(const std::string& prefix) {
        size_t id = file_counter_.fetch_add(1);
        std::string filepath = "./" + prefix + "_" + std::to_string(id) + ".bin";
        
        std::ofstream file(filepath, std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to create file: " + filepath);
        }
        
        return {filepath, std::move(file)};
    }
    
    /**
     * @brief Helper to finalize file creation
     */
    std::string finalizeFile(std::string filepath, std::ofstream file) {
        file.close();
        managed_files_.push_back(filepath);
        return filepath;
    }
    
public:
    /**
     * @brief Create a file with specified size and fill value
     */
    std::string createFile(size_t size, uint8_t fill_value = 0x00, 
                          const std::string& prefix = "test") {
        auto [filepath, file] = createFileHandle(prefix);
        
        // Write in chunks for large files
        constexpr size_t CHUNK_SIZE = 64 * 1024;
        std::vector<uint8_t> chunk(std::min(CHUNK_SIZE, size), fill_value);
        
        size_t remaining = size;
        while (remaining > 0) {
            size_t write_size = std::min(chunk.size(), remaining);
            file.write(reinterpret_cast<const char*>(chunk.data()), write_size);
            remaining -= write_size;
        }
        
        return finalizeFile(std::move(filepath), std::move(file));
    }
    
    /**
     * @brief Create a file with specific data content
     */
    std::string createFileWithData(const std::vector<uint8_t>& data, 
                                  const std::string& prefix = "data") {
        auto [filepath, file] = createFileHandle(prefix);
        file.write(reinterpret_cast<const char*>(data.data()), data.size());
        return finalizeFile(std::move(filepath), std::move(file));
    }
    
    /**
     * @brief Read file contents into vector
     */
    std::vector<uint8_t> readFile(const std::string& filepath) {
        std::ifstream file(filepath, std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file: " + filepath);
        }
        
        return std::vector<uint8_t>(
            std::istreambuf_iterator<char>(file),
            std::istreambuf_iterator<char>()
        );
    }
    
    /**
     * @brief Clean up all managed files
     */
    void cleanup() {
        for (const auto& filepath : managed_files_) {
            try {
                std::filesystem::remove(filepath);
            } catch (const std::exception& e) {
                std::cerr << "Warning: Failed to remove " << filepath 
                          << ": " << e.what() << std::endl;
            }
        }
        managed_files_.clear();
    }
    
    /**
     * @brief Get count of managed files
     */
    size_t getManagedFileCount() const {
        return managed_files_.size();
    }
    
    ~FileManager() { cleanup(); }
};

// =============================================================================
// DATA GENERATION
// =============================================================================

/**
 * @brief Pure data generation utilities
 * Single responsibility: generating test data only
 */
class DataGenerator {
private:
    std::mt19937 rng_;
    
    /**
     * @brief Helper for generating random values in a range
     */
    template<typename T>
    T randomInRange(T min_val, T max_val) {
        if (min_val >= max_val) return min_val;
        std::uniform_int_distribution<T> dist(min_val, max_val);
        return dist(rng_);
    }
    
public:
    explicit DataGenerator(uint32_t seed = std::random_device{}()) : rng_(seed) {}
    
    /**
     * @brief Generate random bytes
     */
    std::vector<uint8_t> randomBytes(size_t count) {
        std::vector<uint8_t> data(count);
        std::generate(data.begin(), data.end(), [this]() { 
            return randomInRange<uint8_t>(0, 255); 
        });
        return data;
    }
    
    /**
     * @brief Generate random offset within bounds
     */
    size_t randomOffset(size_t max_value) {
        return (max_value == 0) ? 0 : randomInRange<size_t>(0, max_value - 1);
    }
    
    /**
     * @brief Generate random size within bounds
     */
    size_t randomSize(size_t min_size, size_t max_size) {
        return randomInRange(min_size, max_size);
    }
    
    /**
     * @brief Generate deterministic pattern data
     */
    std::vector<uint8_t> patternBytes(size_t count, uint8_t pattern) {
        return std::vector<uint8_t>(count, pattern);
    }
};

// =============================================================================
// TEST ASSERTIONS
// =============================================================================

/**
 * @brief Simple test assertions without macros
 * Single responsibility: validating test conditions
 */
class Assertion {
private:
    /**
     * @brief Helper to create assertion error messages
     */
    static std::string createErrorMessage(const std::string& base_message, 
                                         const std::string& detail = "") {
        std::string result = "Assertion failed: " + base_message;
        if (!detail.empty()) {
            result += " (" + detail + ")";
        }
        return result;
    }
    
public:
    /**
     * @brief Assert a boolean condition
     */
    static void assertTrue(bool condition, const std::string& message) {
        if (!condition) {
            throw std::runtime_error(createErrorMessage(message));
        }
    }
    
    /**
     * @brief Assert equality of vectors
     */
    static void assertEqual(const std::vector<uint8_t>& expected, 
                           const std::vector<uint8_t>& actual,
                           const std::string& message) {
        if (expected.size() != actual.size()) {
            std::string detail = "size mismatch: expected " + std::to_string(expected.size()) +
                               ", got " + std::to_string(actual.size());
            throw std::runtime_error(createErrorMessage(message, detail));
        }
        
        if (expected != actual) {
            // Find first difference for better error reporting
            for (size_t i = 0; i < expected.size(); ++i) {
                if (expected[i] != actual[i]) {
                    std::string detail = "first difference at index " + std::to_string(i) +
                                       ": expected 0x" + std::to_string(expected[i]) +
                                       ", got 0x" + std::to_string(actual[i]);
                    throw std::runtime_error(createErrorMessage(message, detail));
                }
            }
        }
    }
    
    /**
     * @brief Assert equality of values
     */
    template<typename T>
    static void assertEqual(const T& expected, const T& actual, const std::string& message) {
        if (expected != actual) {
            throw std::runtime_error(createErrorMessage(message));
        }
    }
    
    /**
     * @brief Expect an exception to be thrown
     */
    static void expectException(std::function<void()> operation, const std::string& message) {
        bool exception_thrown = false;
        try {
            operation();
        } catch (const std::exception&) {
            exception_thrown = true;
        }
        
        if (!exception_thrown) {
            throw std::runtime_error("Expected exception not thrown: " + message);
        }
    }
};

// =============================================================================
// PERFORMANCE MEASUREMENT
// =============================================================================

/**
 * @brief High-precision timer for performance measurement
 */
class Timer {
private:
    std::chrono::high_resolution_clock::time_point start_time_;
    
    /**
     * @brief Helper to get elapsed time with specific duration casting
     */
    template<typename DurationUnit>
    double getElapsedTime(double conversion_factor) const {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<DurationUnit>(end_time - start_time_);
        return duration.count() / conversion_factor;
    }
    
public:
    /**
     * @brief Start the timer
     */
    void start() {
        start_time_ = std::chrono::high_resolution_clock::now();
    }
    
    /**
     * @brief Get elapsed time in milliseconds
     */
    double elapsedMs() const {
        return getElapsedTime<std::chrono::microseconds>(1000.0);
    }
};

} // namespace TestUtils

// Static member definition
std::atomic<size_t> TestUtils::FileManager::file_counter_{0};
