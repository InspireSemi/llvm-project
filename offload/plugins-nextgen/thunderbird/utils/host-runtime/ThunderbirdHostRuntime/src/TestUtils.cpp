#include "TestUtils.hpp"

namespace TestUtils {

bool check_file_permissions(const std::string& filepath) {
    struct stat file_stat;
    if (stat(filepath.c_str(), &file_stat) != 0) {
        std::cerr << "Error: Shared memory file " << filepath << " does not exist" << std::endl;
        return false;
    }

    // Check read and write permissions
    if (access(filepath.c_str(), R_OK | W_OK) != 0) {
        std::cerr << "Error: Current user does not have read/write permissions for " << filepath << std::endl;
        return false;
    }

    std::cout << "✓ Shared memory file exists and has proper permissions" << std::endl;
    return true;
}

bool check_qemu_process() {
    // Use pgrep to check for QEMU processes owned by current user (partial match)
    int result = system("pgrep -u $(id -u) -f qemu > /dev/null 2>&1");
    if (result != 0) {
        std::cerr << "Error: No QEMU process found running for current user" << std::endl;
        std::cerr << "Please start QEMU with ivshmem support before running this test" << std::endl;
        return false;
    }

    std::cout << "✓ QEMU process found running for current user" << std::endl;
    return true;
}

bool create_and_send_launch_batch(ThunderbirdHostRuntime& runtime,
                                  uint64_t entry_address,
                                  uint64_t args_address,
                                  uint32_t args_size,
                                  uint32_t num_blocks_x,
                                  uint32_t num_blocks_y,
                                  uint32_t num_blocks_z,
                                  uint32_t num_threads_x,
                                  uint32_t num_threads_y,
                                  uint32_t num_threads_z,
                                  std::vector<std::pair<int, message_slot_t>>& out_launch_batch) {
    std::cout << "Creating kernel launch batch..." << std::endl;
    std::cout << "Entry address: 0x" << std::hex << entry_address << std::dec << std::endl;
    std::cout << "Args address: 0x" << std::hex << args_address << std::dec << std::endl;
    std::cout << "Args size: " << args_size << " bytes" << std::endl;

    // Create batch with only launch command
    std::vector<message_slot_t> launch_batch_body(1);

    if (!MessageUtils::createLaunchCmd(&launch_batch_body[0], entry_address,
                                      num_blocks_x, num_blocks_y, num_blocks_z,
                                      num_threads_x, num_threads_y, num_threads_z,
                                      args_address, args_size)) {
        std::cerr << "Error: Failed to create launch command" << std::endl;
        return false;
    }

    if (!BatchUtils::create_command_batch(launch_batch_body, 0, out_launch_batch)) {
        std::cerr << "Error: Failed to create launch batch" << std::endl;
        return false;
    }

    std::cout << "\n=== LAUNCH BATCH ===" << std::endl;
    std::cout << "Batch contains " << out_launch_batch.size() << " slots:" << std::endl;
    for (const auto& [slot_index, slot] : out_launch_batch) {
        std::cout << "\n--- Slot " << slot_index << " ---" << std::endl;
        MessageUtils::printMessageSlot(&slot);
    }
    std::cout << "==================\n" << std::endl;

    // Send the launch batch
    auto writer = runtime.getWriter();
    if (!BatchUtils::sendBatch(*writer, out_launch_batch, true)) {
        std::cerr << "Error: Failed to send launch batch" << std::endl;
        return false;
    }

    return true;
}

} // namespace TestUtils