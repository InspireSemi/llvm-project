/**
 * @file XILINX_XDMA_DEVICES.hpp
 * @author Jacob Longwell (jlongwell@inspiresemi.com)
 * @brief 
 * @version 0.1
 * @date 2025-07-15
 * 
 * @copyright Copyright (c) 2025 InspireSemi
 * 
 */
#pragma once

class XILINXXdmaDevices {
public:
    /* Host → Card (write) */
    static constexpr const char* H2C0 = "/dev/xdma0_h2c_0";
    static constexpr const char* H2C1 = "/dev/xdma0_h2c_1";
    static constexpr const char* H2C2 = "/dev/xdma0_h2c_2";
    static constexpr const char* H2C3 = "/dev/xdma0_h2c_3";
    /* Card → Host (read)  */
    static constexpr const char* C2H0 = "/dev/xdma0_c2h_0";
    static constexpr const char* C2H1 = "/dev/xdma0_c2h_1";
    static constexpr const char* C2H2 = "/dev/xdma0_c2h_2";
    static constexpr const char* C2H3 = "/dev/xdma0_c2h_3";
};

