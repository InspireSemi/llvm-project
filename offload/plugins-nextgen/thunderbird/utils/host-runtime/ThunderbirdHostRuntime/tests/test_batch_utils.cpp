/**
 * @file test_batch_utils.cpp
 * @brief Unit tests for BatchUtils internal API functions
 * @details Tests functions like BatchUtils::run_batch_integrity_tests and other internal batch utilities
 *          used by the ThunderbirdHostRuntime API.
 */

#include "BatchUtils.hpp"
#include <iostream>

int main(int argc, char** argv) {
    std::cout << "Running BatchUtils internal API tests..." << std::endl;

    // Run the batch integrity tests
    bool batch_tests_passed = BatchUtils::run_batch_integrity_tests();

    if (batch_tests_passed) {
        std::cout << "✅ All BatchUtils tests passed!" << std::endl;
        return 0;
    } else {
        std::cout << "❌ Some BatchUtils tests failed!" << std::endl;
        return 1;
    }
}