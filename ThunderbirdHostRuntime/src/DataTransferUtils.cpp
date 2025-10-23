#include "DataTransferUtils.hpp"

namespace DataTransferUtils {

/**
 * @brief Generate a random vector of floats
 * @param size Number of elements
 * @param min_val Minimum value
 * @param max_val Maximum value
 * @return Vector of random floats
 */
std::vector<float> generate_random_vector(size_t size, float min_val, float max_val) {
    std::vector<float> data(size);

    // Use a fixed seed for reproducible results (or use random_device for true randomness)
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(min_val, max_val);

    for (size_t i = 0; i < size; ++i) {
        data[i] = dist(gen);
    }

    std::cout << "Generated " << size << " random floats in range ["
              << min_val << ", " << max_val << "]" << std::endl;
    std::cout << "First few values: ";
    for (size_t i = 0; i < std::min(size_t(5), size); ++i) {
        std::cout << data[i] << " ";
    }
    std::cout << "..." << std::endl;

    return data;
}

} // namespace DataTransferUtils