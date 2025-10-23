// Buffer management utilities implementation

#include "BufferUtils.hpp"

namespace BufferUtils {

AllocatedBuffer* find_buffer_by_purpose(std::vector<AllocatedBuffer>& buffers, const std::string& purpose) {
    for (auto& buffer : buffers) {
        if (buffer.purpose == purpose) {
            return &buffer;
        }
    }
    return nullptr;
}

bool find_required_buffers(const std::unordered_map<std::string, AllocatedBuffer>& buffers_map,
                          const std::vector<std::string>& purposes,
                          std::unordered_map<std::string, AllocatedBuffer*>& out_found_buffers) {
    out_found_buffers.clear();
    
    // Find each required buffer using the map
    for (const auto& purpose : purposes) {
        auto it = buffers_map.find(purpose);
        if (it == buffers_map.end()) {
            std::cerr << "Error: Required buffer with purpose '" << purpose << "' not found" << std::endl;
            out_found_buffers.clear(); // Clear partial results
            return false;
        }
        out_found_buffers[purpose] = const_cast<AllocatedBuffer*>(&it->second);
    }
    
    return true;
}

} // namespace BufferUtils