#pragma once

#include "ThunderbirdRuntime.hpp"
#include "MailboxUtils.hpp"
#include "BatchUtils.hpp"
#include "MessageUtils.hpp"
#include <vector>
#include <string>
#include <cstdint>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdlib>

// Test utilities for common testing operations

namespace TestUtils {

/**
 * @brief Check file permissions for shared memory file
 * @param filepath Path to the shared memory file
 * @return true if file exists and has proper read/write permissions
 */
bool check_file_permissions(const std::string& filepath);

/**
 * @brief Check if QEMU process is running for current user
 * @return true if QEMU process found
 */
bool check_qemu_process();

/**
 * @brief Create and send kernel launch batch
 * @param runtime ThunderbirdHostRuntime instance
 * @param entry_address Device address of kernel entry point
 * @param args_address Device address of kernel arguments
 * @param num_blocks_x Number of blocks in X dimension
 * @param num_blocks_y Number of blocks in Y dimension
 * @param num_blocks_z Number of blocks in Z dimension
 * @param num_threads_x Number of threads per block in X dimension
 * @param num_threads_y Number of threads per block in Y dimension
 * @param num_threads_z Number of threads per block in Z dimension
 * @param out_launch_batch Output vector to store created batch slots
 * @return true if successful
 */
bool create_and_send_launch_batch(ThunderbirdHostRuntime& runtime,
                                  uint64_t entry_address,
                                  uint64_t args_address,
                                  uint32_t num_blocks_x,
                                  uint32_t num_blocks_y,
                                  uint32_t num_blocks_z,
                                  uint32_t num_threads_x,
                                  uint32_t num_threads_y,
                                  uint32_t num_threads_z,
                                  std::vector<std::pair<int, message_slot_t>>& out_launch_batch);

} // namespace TestUtils