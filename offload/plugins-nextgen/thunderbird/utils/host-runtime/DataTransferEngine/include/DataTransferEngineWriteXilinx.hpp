/**
 * @file DataTransferEngineWriteXilinx.hpp
 * @author Jacob Longwell (jlongwell@inspiresemi.com)
 * @brief 
 * @version 0.1
 * @date 2025-07-15
 * 
 * @copyright Copyright (c) 2025 InspireSemi
 * 
 */
#pragma once
#include "DataTransferEngineWriteBase.hpp"
#include <string>

class DataTransferEngineWriteXilinx final : public DataTransferEngineWriteBase {
public:
    explicit DataTransferEngineWriteXilinx(std::string devNode);
    ~DataTransferEngineWriteXilinx() override;

    std::size_t transfer(uint64_t, const void*, std::size_t) override;
    std::size_t transferFile(uint64_t, const std::string&, std::size_t) override;

private:
    void openIfNeeded();
    int         fd_{-1};
    std::string devNode_;
};

