// Implementations for BatchUtils

#include "BatchUtils.hpp"
#include <iostream>
#include <cstring>
#include <thread>
#include <chrono>

namespace {
    // Helper function to extract slot information from batch payload
    bool extract_batch_slots(const message_slot_t& slot, std::set<uint8_t>& slots_set, uint32_t& slot_count) {
        if (slot.msg_id == MSG_CMD_BEGIN_BATCH || slot.msg_id == MSG_RSP_BEGIN_BATCH) {
            if (slot.length >= sizeof(batch_begin_cmd_t)) {
                const batch_begin_cmd_t* payload = reinterpret_cast<const batch_begin_cmd_t*>(slot.data);
                slot_count = payload->slot_count;
                for (uint32_t i = 0; i < payload->slot_count && i < MAX_BATCH_SLOTS; ++i) {
                    slots_set.insert(payload->batch_slots[i]);
                }
                return true;
            }
        } else if (slot.msg_id == MSG_CMD_END_BATCH || slot.msg_id == MSG_RSP_END_BATCH) {
            if (slot.length >= sizeof(batch_end_cmd_t)) {
                const batch_end_cmd_t* payload = reinterpret_cast<const batch_end_cmd_t*>(slot.data);
                slot_count = payload->slot_count;
                for (uint32_t i = 0; i < payload->slot_count && i < MAX_BATCH_SLOTS; ++i) {
                    slots_set.insert(payload->batch_slots[i]);
                }
                return true;
            }
        }
        return false;
    }

    // Helper function to validate batch consistency
    bool validate_batch_consistency(bool found_cmd_begin, bool found_cmd_end,
                                   bool found_rsp_begin, bool found_rsp_end,
                                   bool& is_cmd, bool& is_rsp) {
        if (found_cmd_begin && found_cmd_end && !found_rsp_begin && !found_rsp_end) {
            is_cmd = true;
            return true;
        } else if (found_rsp_begin && found_rsp_end && !found_cmd_begin && !found_cmd_end) {
            is_rsp = true;
            return true;
        }
        return false;
    }

    // Helper function to verify slot sets match
    bool verify_slot_sets_match(const std::set<uint8_t>& begin_slots,
                                const std::set<uint8_t>& end_slots,
                                uint32_t begin_count, uint32_t end_count) {
        return (begin_count == end_count) && (begin_slots == end_slots);
    }

    // Helper function to validate actual slots against advertised slots
    bool validate_actual_slots(const std::vector<std::pair<int, message_slot_t>>& slots,
                              const std::set<uint8_t>& advertised_slots,
                              uint32_t advertised_count) {
        // Check slot count
        if (advertised_count != slots.size()) {
            return false;
        }

        // Check that all actual slots are in advertised set
        std::set<uint8_t> actual_slots;
        for (const auto& [index, slot] : slots) {
            actual_slots.insert(static_cast<uint8_t>(index));
        }

        return advertised_slots == actual_slots;
    }



}

// Public API functions for batch creation and validation

namespace BatchUtils {

bool confirm_batch_integrity(const std::vector<std::pair<int, message_slot_t>>& slots,
                           int& begin_slot, int& end_slot, bool& is_cmd, bool& is_rsp,
                           std::vector<std::pair<int, message_slot_t>>& body_slots) {
    begin_slot = -1;
    end_slot = -1;
    is_cmd = false;
    is_rsp = false;
    body_slots.clear();
    
    bool found_cmd_begin = false;
    bool found_rsp_begin = false;
    bool found_cmd_end = false;
    bool found_rsp_end = false;
    
    std::set<uint8_t> begin_advertised_slots;
    std::set<uint8_t> end_advertised_slots;
    uint32_t begin_slot_count = 0;
    uint32_t end_slot_count = 0;

    for (const auto& [index, slot] : slots) {
        if (slot.msg_id == MSG_CMD_BEGIN_BATCH) {
            if (begin_slot != -1) return false; // More than one begin command found
            begin_slot = index;
            found_cmd_begin = true;
            if (!extract_batch_slots(slot, begin_advertised_slots, begin_slot_count)) {
                return false;
            }
            
        } else if (slot.msg_id == MSG_RSP_BEGIN_BATCH) {
            if (begin_slot != -1) return false; // More than one begin command found
            begin_slot = index;
            found_rsp_begin = true;
            if (!extract_batch_slots(slot, begin_advertised_slots, begin_slot_count)) {
                return false;
            }
            
        } else if (slot.msg_id == MSG_CMD_END_BATCH) {
            if (end_slot != -1) return false; // More than one end command found
            end_slot = index;
            found_cmd_end = true;
            if (!extract_batch_slots(slot, end_advertised_slots, end_slot_count)) {
                return false;
            }
            
        } else if (slot.msg_id == MSG_RSP_END_BATCH) {
            if (end_slot != -1) return false; // More than one end command found
            end_slot = index;
            found_rsp_end = true;
            if (!extract_batch_slots(slot, end_advertised_slots, end_slot_count)) {
                return false;
            }
        }
    }

    // Check if begin and end slots are found and properly ordered
    if (begin_slot == -1 || end_slot == -1 || begin_slot >= end_slot) {
        return false;
    }

    // Validate batch consistency (cmd vs rsp)
    if (!validate_batch_consistency(found_cmd_begin, found_cmd_end,
                                   found_rsp_begin, found_rsp_end, is_cmd, is_rsp)) {
        return false;
    }

    // Verify begin and end slot sets match
    if (!verify_slot_sets_match(begin_advertised_slots, end_advertised_slots,
                               begin_slot_count, end_slot_count)) {
        return false;
    }

    // Validate actual slots against advertised slots
    if (!validate_actual_slots(slots, begin_advertised_slots, begin_slot_count)) {
        return false;
    }

    // Collect body slots (excluding begin and end) in index order
    for (const auto& [index, slot] : slots) {
        if (index != begin_slot && index != end_slot) {
            body_slots.push_back({index, slot});
        }
    }

    // Sort body slots by index to ensure proper order
    std::sort(body_slots.begin(), body_slots.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    return true;
}

// Helper function for creating batches
bool create_batch_helper(const std::vector<message_slot_t>& body_messages,
							uint8_t start_slot_index,
							std::vector<std::pair<int, message_slot_t>>& batch_slots,
							Batch::Type type) {
    batch_slots.clear();
    
    if (body_messages.empty()) {
        return false; // Empty batches not supported
    }
    
    // Create a Batch instance
    Batch batch(start_slot_index, 0, MAX_BATCH_SLOTS - 1, type);  // begin_slot, end_slot (calculated), max_end_slot, type
    
    // Add all body messages
    for (const auto& msg : body_messages) {
        batch.append_body_message(msg);
    }
    
    // Finalize the batch
    if (!batch.finalize()) {
        return false;
    }
    
    // Get the finalized batch
    batch_slots = batch.finalized_batch();
    return true;
}   

bool create_command_batch(const std::vector<message_slot_t>& body_messages,
                         uint8_t start_slot_index,
                         std::vector<std::pair<int, message_slot_t>>& batch_slots) {
    return create_batch_helper(body_messages, start_slot_index, batch_slots, Batch::Type::COMMAND);
}

bool create_response_batch(const std::vector<message_slot_t>& body_messages,
                          uint8_t start_slot_index,
                          std::vector<std::pair<int, message_slot_t>>& batch_slots) {
    return create_batch_helper(body_messages, start_slot_index, batch_slots, Batch::Type::RESPONSE);
}

bool run_batch_integrity_tests() {
    std::cout << "Running BatchUtils tests..." << std::endl;
    int tests_passed = 0;
    int total_tests = 0;

    // Test 1: Valid command batch using public API
    {
        total_tests++;
        std::cout << "Testing command batch creation..." << std::endl;
        
        std::vector<message_slot_t> body_messages(3);
        MessageUtils::createMallocCmd(&body_messages[0], 1024);
        MessageUtils::createFreeCmd(&body_messages[1], 0x12345678);
        MessageUtils::createQueryDeviceCmd(&body_messages[2]);
        
        std::vector<std::pair<int, message_slot_t>> batch_slots;
        bool result = create_command_batch(body_messages, 10, batch_slots);
        
        if (result && batch_slots.size() == 5) { // begin + 3 body + end
            std::cout << "✓ Command batch creation test passed" << std::endl;
            tests_passed++;
        } else {
            std::cout << "✗ Command batch creation test failed" << std::endl;
        }
    }

    // Test 2: Valid response batch using public API
    {
        total_tests++;
        std::cout << "Testing response batch creation..." << std::endl;
        
        std::vector<message_slot_t> body_messages(1);
        MessageUtils::createPing(&body_messages[0], 12345);
        
        std::vector<std::pair<int, message_slot_t>> batch_slots;
        bool result = create_response_batch(body_messages, 20, batch_slots);
        
        if (result && batch_slots.size() == 3) { // begin + 1 body + end
            std::cout << "✓ Response batch creation test passed" << std::endl;
            tests_passed++;
        } else {
            std::cout << "✗ Response batch creation test failed" << std::endl;
        }
    }

    // Test 3: Empty body messages should fail
    {
        total_tests++;
        std::cout << "Testing empty batch creation..." << std::endl;
        
        std::vector<message_slot_t> body_messages;
        std::vector<std::pair<int, message_slot_t>> batch_slots;
        bool result = create_command_batch(body_messages, 0, batch_slots);
        
        if (!result) {
            std::cout << "✓ Empty batch creation test passed" << std::endl;
            tests_passed++;
        } else {
            std::cout << "✗ Empty batch creation test failed" << std::endl;
        }
    }

    std::cout << "\nTest Results: " << tests_passed << "/" << total_tests << " tests passed" << std::endl;
    
    if (tests_passed == total_tests) {
        std::cout << "✅ All tests passed!" << std::endl;
        return true;
    } else {
        std::cout << "❌ Some tests failed!" << std::endl;
        return false;
    }
}


// Batch class implementation
Batch::Batch(int begin_slot, int end_slot, int max_end_slot, Batch::Type type)
    : begin_slot_(begin_slot), end_slot_(end_slot), max_end_slot_(max_end_slot), type_(type) {
}

void Batch::append_body_message(const message_slot_t& message) {
    messages_.push_back(message);
}

std::vector<message_slot_t>& Batch::body_messages() {
    return messages_;
}

bool Batch::finalize() {
    // Validate slot values
    if (begin_slot_ < 0 || end_slot_ < 0 || max_end_slot_ < 0) {
        std::cerr << "Error: Invalid slot values (negative)" << std::endl;
        return false;
    }

    if (begin_slot_ > max_end_slot_) {
        std::cerr << "Error: begin_slot (" << begin_slot_ << ") exceeds max_end_slot (" << max_end_slot_ << ")" << std::endl;
        return false;
    }

    // Calculate required slots: begin + body + end
    int required_slots = 1 + messages_.size() + 1;  // begin + body + end
    int last_slot_needed = begin_slot_ + required_slots - 1;

    if (last_slot_needed > max_end_slot_) {
        std::cerr << "Error: Batch requires slots " << begin_slot_ << " to " << last_slot_needed
                  << " but max_end_slot is " << max_end_slot_ << std::endl;
        return false;
    }

    // If end_slot was set by user, validate it matches our calculation
    if (end_slot_ != 0 && end_slot_ != last_slot_needed) {
        std::cerr << "Error: Specified end_slot (" << end_slot_ << ") doesn't match calculated end slot (" << last_slot_needed << ")" << std::endl;
        return false;
    }

    // Create the finalized batch
    finalized_batch_.clear();
    finalized_batch_.reserve(required_slots);

    // Create slot indices vector for the entire batch
    std::vector<uint8_t> slot_indices;
    for (int i = 0; i < required_slots; ++i) {
        slot_indices.push_back(static_cast<uint8_t>(begin_slot_ + i));
    }

    // Add begin slot
    message_slot_t begin_slot_msg;
    bool begin_success = (type_ == Type::COMMAND) ?
        MessageUtils::createBatchBeginCmd(&begin_slot_msg, slot_indices) :
        MessageUtils::createBatchBeginRsp(&begin_slot_msg, slot_indices);
    if (!begin_success) {
        std::cerr << "Error: Failed to create begin batch message" << std::endl;
        return false;
    }
    finalized_batch_.emplace_back(begin_slot_, begin_slot_msg);

    // Add body messages
    for (size_t i = 0; i < messages_.size(); ++i) {
        finalized_batch_.emplace_back(begin_slot_ + 1 + i, messages_[i]);
    }

    // Add end slot
    message_slot_t end_slot_msg;
    bool end_success = (type_ == Type::COMMAND) ?
        MessageUtils::createBatchEndCmd(&end_slot_msg, slot_indices) :
        MessageUtils::createBatchEndRsp(&end_slot_msg, slot_indices);
    if (!end_success) {
        std::cerr << "Error: Failed to create end batch message" << std::endl;
        return false;
    }
    finalized_batch_.emplace_back(last_slot_needed, end_slot_msg);

    // Update end_slot_ to the calculated value
    end_slot_ = last_slot_needed;

    return true;
}

const std::vector<std::pair<int, message_slot_t>>& Batch::finalized_batch() const {
    return finalized_batch_;
}

int Batch::begin_slot() const {
    return begin_slot_;
}

int Batch::end_slot() const {
    return end_slot_;
}

int Batch::max_end_slot() const {
    return max_end_slot_;
}

} // namespace BatchUtils