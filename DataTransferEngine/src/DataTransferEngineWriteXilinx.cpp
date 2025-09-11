/**
 * @file DataTransferEngineWriteXilinx.cpp
 * @author Jacob Longwell (jlongwell@inspiresemi.com)
 * @brief 
 * @version 0.1
 * @date 2025-07-15
 * 
 * @copyright Copyright (c) 2025 InspireSemi
 * 
 */
#include "DataTransferEngineWriteXilinx.hpp"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <stdexcept>
#include <system_error>
#include <sys/mman.h>
#include <unistd.h>

static inline void chk(int rc, const char* what)
{
    if (rc < 0) throw std::system_error(errno, std::generic_category(), what);
}

DataTransferEngineWriteXilinx::DataTransferEngineWriteXilinx(std::string node)
    : devNode_(std::move(node)) {}

    DataTransferEngineWriteXilinx::~DataTransferEngineWriteXilinx()
{
    if (fd_ >= 0) ::close(fd_);
}

void DataTransferEngineWriteXilinx::openIfNeeded()
{
    if (fd_ >= 0) return;
    fd_ = ::open(devNode_.c_str(), O_RDWR | O_SYNC);
    chk(fd_, "open(xdma h2c)");
}

std::size_t DataTransferEngineWriteXilinx::transfer(
        uint64_t addr, const void* buf, std::size_t len)
{
    openIfNeeded();
    chk(::lseek64(fd_, static_cast<off64_t>(addr), SEEK_SET), "lseek64");
    const uint8_t* p = static_cast<const uint8_t*>(buf);
    for (std::size_t left = len; left;) {
        ssize_t wr = ::write(fd_, p, left);
        chk(static_cast<int>(wr), "write");
        left -= wr; p += wr;
    }
    return len;
}

std::size_t DataTransferEngineWriteXilinx::transferFile(
        uint64_t addr, const std::string& path, std::size_t len)
{
    int in = ::open(path.c_str(), O_RDONLY);
    chk(in, "open(input)");
    void* raw = nullptr;
    if (::posix_memalign(&raw, 4096, len)) throw std::bad_alloc();
    std::unique_ptr<uint8_t, decltype(&::free)>
        buf(reinterpret_cast<uint8_t*>(raw), &::free);

    std::size_t filled = 0;
    while (filled < len) {
        ssize_t rd = ::read(in, buf.get() + filled, len - filled);
        chk(static_cast<int>(rd), "read");
        if (rd == 0) throw std::runtime_error("input file too small");
        filled += rd;
    }
    ::close(in);
    return transfer(addr, buf.get(), len);
}

