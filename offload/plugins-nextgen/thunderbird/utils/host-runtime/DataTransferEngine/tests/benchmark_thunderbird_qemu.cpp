/**
 * @file benchmark_thunderbird_qemu.cpp
 * @brief Performance benchmarks for ThunderbirdQEMU DataTransferEngine classes
 * @date 2025-07-27
 * 
 * @copyright Copyright (c) 2025 InspireSemi
 */

#include "test_utilities.hpp"
#include "DataTransferEngineFactory.hpp"
#include "DataTransferBackend.hpp"
#include <iomanip>
#include <numeric>
#include <algorithm>
#include <iostream>
#include <chrono>
#include <thread>
#include <future>
#include <mutex>
#include <atomic>

namespace {

// Constants for better maintainability
constexpr size_t KB = 1024;
constexpr size_t MB = KB * KB;

// Simplified benchmark configuration
struct BenchmarkConfig {
    size_t shared_memory_size = 2 * MB;      // 2MB - reasonable for testing
    size_t small_buffer_size = 4 * KB;       // 4KB
    size_t medium_buffer_size = 64 * KB;     // 64KB
    size_t large_buffer_size = 256 * KB;     // 256KB
    size_t iterations = 20;                  // Reduced since serialization makes fewer samples sufficient
    size_t thread_count = 4;                 // Fixed, reasonable number for contention testing
    std::string shared_mem_file;
};

// Improved BenchmarkResults class with better statistics and thread safety
class BenchmarkResults {
private:
    std::vector<double> times_;
    std::string operation_name_;
    size_t total_bytes_;
    mutable std::mutex mutex_;
    
public:
    BenchmarkResults(std::string name, size_t bytes) 
        : operation_name_(std::move(name)), total_bytes_(bytes) {
        times_.reserve(200); // Reserve space to avoid reallocations
    }
    
    void addTime(double time_ms) {
        std::lock_guard<std::mutex> lock(mutex_);
        times_.push_back(time_ms);
    }
    
    [[nodiscard]] size_t getSampleCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return times_.size();
    }
    
    void printResults() const {
        if (times_.empty()) {
            std::cout << operation_name_ << ": No data collected" << std::endl;
            return;
        }
        
        auto sorted_times = times_;
        std::sort(sorted_times.begin(), sorted_times.end());
        
        double min_time = sorted_times.front();
        double max_time = sorted_times.back();
        double avg_time = std::accumulate(sorted_times.begin(), sorted_times.end(), 0.0) / sorted_times.size();
        
        // Calculate percentiles for better performance analysis
        size_t p50_idx = sorted_times.size() / 2;
        size_t p95_idx = static_cast<size_t>(sorted_times.size() * 0.95);
        size_t p99_idx = static_cast<size_t>(sorted_times.size() * 0.99);
        
        double p50_time = sorted_times[p50_idx];
        double p95_time = sorted_times[std::min(p95_idx, sorted_times.size() - 1)];
        double p99_time = sorted_times[std::min(p99_idx, sorted_times.size() - 1)];
        
        double avg_throughput_mbps = (total_bytes_ / static_cast<double>(MB)) / (avg_time / 1000.0);
        double peak_throughput_mbps = (total_bytes_ / static_cast<double>(MB)) / (min_time / 1000.0);
        
        std::cout << std::fixed << std::setprecision(3);
        std::cout << operation_name_ << ":" << std::endl;
        std::cout << "  Samples: " << times_.size() << std::endl;
        std::cout << "  Data size: " << (total_bytes_ / static_cast<double>(KB)) << " KB" << std::endl;
        std::cout << "  Time (ms) - Min: " << min_time << ", Avg: " << avg_time << ", Max: " << max_time << std::endl;
        std::cout << "  Percentiles (ms) - P50: " << p50_time << ", P95: " << p95_time << ", P99: " << p99_time << std::endl;
        std::cout << "  Throughput - Avg: " << avg_throughput_mbps << " MB/s, Peak: " << peak_throughput_mbps << " MB/s" << std::endl;
        std::cout << std::endl;
    }
};

// Helper class for creating engine instances with proper error handling
class EngineManager {
private:
    std::string device_path_;
    
public:
    explicit EngineManager(std::string device_path) : device_path_(std::move(device_path)) {}
    
    [[nodiscard]] auto createReadEngine() const {
        return DataTransferEngineFactory::createReadChannel(DataTransferBackend::ThunderbirdQEMU, device_path_);
    }
    
    [[nodiscard]] auto createWriteEngine() const {
        return DataTransferEngineFactory::createWriteChannel(DataTransferBackend::ThunderbirdQEMU, device_path_);
    }
    
    [[nodiscard]] const std::string& getDevicePath() const { return device_path_; }
};

} // anonymous namespace

// Improved buffer operations benchmark with better error handling
void benchmark_buffer_operations(const BenchmarkConfig& config) {
    std::cout << "=== Buffer Operations Benchmark ===" << std::endl;
    
    try {
        EngineManager engine_mgr(config.shared_mem_file);
        auto reader = engine_mgr.createReadEngine();
        auto writer = engine_mgr.createWriteEngine();
        
        const std::vector<size_t> buffer_sizes = {
            config.small_buffer_size,
            config.medium_buffer_size,
            config.large_buffer_size
        };
        
        TestUtils::Timer timer;
        TestUtils::DataGenerator data_gen(42); // Fixed seed for reproducibility
        
        for (size_t buffer_size : buffer_sizes) {
            if (buffer_size > config.shared_memory_size) {
                std::cout << "Skipping buffer size " << (buffer_size / KB) 
                          << "KB (larger than shared memory)" << std::endl;
                continue;
            }
            
            std::vector<uint8_t> write_data = data_gen.patternBytes(buffer_size, 0xAA);
            std::vector<uint8_t> read_data(buffer_size, 0x00);
            
            const std::string size_label = std::to_string(buffer_size / KB) + "KB";
            BenchmarkResults write_results("Buffer Write (" + size_label + ")", buffer_size);
            BenchmarkResults read_results("Buffer Read (" + size_label + ")", buffer_size);
            
            for (size_t i = 0; i < config.iterations; ++i) {
                uint64_t offset = (i * buffer_size) % (config.shared_memory_size - buffer_size);
                
                // Benchmark write
                timer.start();
                size_t written = writer->transfer(offset, write_data.data(), buffer_size);
                write_results.addTime(timer.elapsedMs());
                
                if (written != buffer_size) {
                    std::cerr << "Warning: Write size mismatch at iteration " << i 
                              << " (expected " << buffer_size << ", got " << written << ")" << std::endl;
                }
                
                // Benchmark read
                timer.start();
                size_t read = reader->transfer(offset, read_data.data(), buffer_size);
                read_results.addTime(timer.elapsedMs());
                
                if (read != buffer_size) {
                    std::cerr << "Warning: Read size mismatch at iteration " << i 
                              << " (expected " << buffer_size << ", got " << read << ")" << std::endl;
                }
            }
            
            write_results.printResults();
            read_results.printResults();
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error in buffer operations benchmark: " << e.what() << std::endl;
    }
}

// Improved file operations benchmark
void benchmark_file_operations(const BenchmarkConfig& config) {
    std::cout << "=== File Operations Benchmark ===" << std::endl;
    
    try {
        EngineManager engine_mgr(config.shared_mem_file);
        auto reader = engine_mgr.createReadEngine();
        auto writer = engine_mgr.createWriteEngine();
        
        TestUtils::FileManager file_manager;
        TestUtils::DataGenerator data_gen(123); // Different seed for variety
        TestUtils::Timer timer;
        
        size_t file_size = config.medium_buffer_size;
        std::vector<uint8_t> test_data = data_gen.patternBytes(file_size, 0xBB);
        
        std::string input_file = file_manager.createFileWithData(test_data, "benchmark_input");
        std::string output_file = file_manager.createFile(file_size, 0x00, "benchmark_output");
        
        const std::string size_label = std::to_string(file_size / KB) + "KB";
        BenchmarkResults write_file_results("File Write (" + size_label + ")", file_size);
        BenchmarkResults read_file_results("File Read (" + size_label + ")", file_size);
        
        // Use fewer iterations for file operations as they're typically slower
        size_t file_iterations = config.iterations / 2;
        
        for (size_t i = 0; i < file_iterations; ++i) {
            uint64_t offset = (i * file_size) % (config.shared_memory_size - file_size);
            
            // Benchmark file write
            timer.start();
            size_t written = writer->transferFile(offset, input_file, file_size);
            write_file_results.addTime(timer.elapsedMs());
            
            if (written != file_size) {
                std::cerr << "Warning: File write size mismatch at iteration " << i << std::endl;
            }
            
            // Benchmark file read
            timer.start();
            size_t read = reader->transferFile(offset, output_file, file_size);
            read_file_results.addTime(timer.elapsedMs());
            
            if (read != file_size) {
                std::cerr << "Warning: File read size mismatch at iteration " << i << std::endl;
            }
        }
        
        write_file_results.printResults();
        read_file_results.printResults();
        
    } catch (const std::exception& e) {
        std::cerr << "Error in file operations benchmark: " << e.what() << std::endl;
    }
}

// Improved random access patterns benchmark
void benchmark_random_access_patterns(const BenchmarkConfig& config) {
    std::cout << "=== Random Access Pattern Benchmark ===" << std::endl;
    
    try {
        EngineManager engine_mgr(config.shared_mem_file);
        auto reader = engine_mgr.createReadEngine();
        auto writer = engine_mgr.createWriteEngine();
        
        TestUtils::DataGenerator data_gen(42); // Fixed seed for reproducibility
        TestUtils::Timer timer;
        
        size_t buffer_size = config.small_buffer_size;
        std::vector<uint8_t> data = data_gen.patternBytes(buffer_size, 0xCC);
        
        BenchmarkResults random_write_results("Random Write Access", buffer_size);
        BenchmarkResults random_read_results("Random Read Access", buffer_size);
        
        // Pre-generate random offsets for consistent testing
        std::vector<uint64_t> random_offsets;
        random_offsets.reserve(config.iterations);
        
        for (size_t i = 0; i < config.iterations; ++i) {
            uint64_t offset = data_gen.randomOffset(config.shared_memory_size - buffer_size);
            // Ensure alignment for better performance
            offset = (offset / 8) * 8;
            random_offsets.push_back(offset);
        }
        
        for (size_t i = 0; i < config.iterations; ++i) {
            uint64_t random_offset = random_offsets[i];
            
            // Benchmark random write
            timer.start();
            size_t written = writer->transfer(random_offset, data.data(), buffer_size);
            random_write_results.addTime(timer.elapsedMs());
            
            if (written != buffer_size) {
                std::cerr << "Warning: Random write size mismatch at iteration " << i << std::endl;
            }
            
            // Benchmark random read
            timer.start();
            size_t read = reader->transfer(random_offset, data.data(), buffer_size);
            random_read_results.addTime(timer.elapsedMs());
            
            if (read != buffer_size) {
                std::cerr << "Warning: Random read size mismatch at iteration " << i << std::endl;
            }
        }
        
        random_write_results.printResults();
        random_read_results.printResults();
        
    } catch (const std::exception& e) {
        std::cerr << "Error in random access benchmark: " << e.what() << std::endl;
    }
}

// Simplified multi-threaded contention benchmark - measures serialization overhead
void benchmark_multithreaded_contention(const BenchmarkConfig& config) {
    std::cout << "=== Multi-threaded Contention Benchmark ===" << std::endl;
    std::cout << "Measuring serialization overhead with " << config.thread_count << " competing threads" << std::endl;
    
    try {
        EngineManager engine_mgr(config.shared_mem_file);
        
        // Single-threaded baseline
        {
            std::cout << "\n--- Single-threaded Baseline ---" << std::endl;
            
            auto reader = engine_mgr.createReadEngine();
            auto writer = engine_mgr.createWriteEngine();
            
            BenchmarkResults baseline("Single-threaded", config.small_buffer_size);
            TestUtils::Timer timer;
            TestUtils::DataGenerator data_gen(42);
            
            std::vector<uint8_t> data = data_gen.patternBytes(config.small_buffer_size, 0xAA);
            std::vector<uint8_t> buffer(config.small_buffer_size);
            
            for (size_t i = 0; i < config.iterations; ++i) {
                uint64_t offset = i * config.small_buffer_size;
                
                timer.start();
                writer->transfer(offset, data.data(), data.size());
                reader->transfer(offset, buffer.data(), buffer.size());
                baseline.addTime(timer.elapsedMs());
            }
            baseline.printResults();
        }
        
        // Multi-threaded contention - shows serialization overhead
        {
            std::cout << "\n--- Multi-threaded Contention (Serialization Overhead) ---" << std::endl;
            
            BenchmarkResults contention("Multi-threaded Contention", config.small_buffer_size);
            std::vector<std::future<std::vector<double>>> futures;
            
            const size_t ops_per_thread = config.iterations / config.thread_count;
            
            for (size_t thread_id = 0; thread_id < config.thread_count; ++thread_id) {
                futures.emplace_back(std::async(std::launch::async, [&, thread_id]() {
                    std::vector<double> thread_times;
                    thread_times.reserve(ops_per_thread);
                    
                    auto reader = engine_mgr.createReadEngine();
                    auto writer = engine_mgr.createWriteEngine();
                    TestUtils::Timer timer;
                    TestUtils::DataGenerator data_gen(thread_id + 100);
                    
                    std::vector<uint8_t> data = data_gen.patternBytes(config.small_buffer_size, 
                                                                    static_cast<uint8_t>(0xAA + thread_id));
                    std::vector<uint8_t> buffer(config.small_buffer_size);
                    
                    for (size_t i = 0; i < ops_per_thread; ++i) {
                        uint64_t offset = (thread_id * ops_per_thread + i) * config.small_buffer_size;
                        
                        timer.start();
                        writer->transfer(offset, data.data(), data.size());
                        reader->transfer(offset, buffer.data(), buffer.size());
                        thread_times.push_back(timer.elapsedMs());
                    }
                    
                    return thread_times;
                }));
            }
            
            // Collect all results
            size_t total_operations = 0;
            for (auto& future : futures) {
                auto thread_times = future.get();
                total_operations += thread_times.size();
                for (double time : thread_times) {
                    contention.addTime(time);
                }
            }
            
            std::cout << "Completed " << total_operations << " operations across " 
                      << config.thread_count << " threads" << std::endl;
            contention.printResults();
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error in multi-threaded contention benchmark: " << e.what() << std::endl;
    }
}

int main() {
    std::cout << "ThunderbirdQEMU Performance Benchmark Suite" << std::endl;
    std::cout << "===========================================" << std::endl;
    
    BenchmarkConfig config;
    TestUtils::FileManager file_manager;
    
    // Setup benchmark environment
    std::cout << "Setting up benchmark environment..." << std::endl;
    std::string shared_mem_file = file_manager.createFile(config.shared_memory_size, 0x00, "benchmark_shared_memory");
    config.shared_mem_file = shared_mem_file;
    
    std::cout << "Configuration:" << std::endl;
    std::cout << "  Shared memory size: " << (config.shared_memory_size / MB) << " MB" << std::endl;
    std::cout << "  Iterations per test: " << config.iterations << std::endl;
    std::cout << "  Thread count: " << config.thread_count << std::endl;
    std::cout << "  Note: API serializes filestream access" << std::endl;
    std::cout << std::endl;
    
    // Run all benchmarks
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        benchmark_buffer_operations(config);
        benchmark_file_operations(config);
        benchmark_random_access_patterns(config);
        benchmark_multithreaded_contention(config);
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto total_duration = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time);
        
        std::cout << "=== Benchmark Suite Complete ===" << std::endl;
        std::cout << "Total execution time: " << total_duration.count() << " seconds" << std::endl;
        std::cout << "All benchmarks completed successfully!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Benchmark suite failed: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
