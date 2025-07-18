/**
 * @file DataTransferEngineReadXilinx.cpp
 * @author Jacob Longwell (jlongwell@inspiresemi.com)
 * @brief 
 * @version 0.1
 * @date 2025-07-15
 * 
 * @copyright Copyright (c) 2025 InspireSemi
 * 
 */
#include "DataTransferEngineReadXilinx.hpp"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <stdexcept>
#include <system_error>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

inline void chkR(int rc, const char *what)
{
    if (rc < 0)
        throw std::system_error(errno, std::generic_category(), what);
}

inline std::size_t align4096R(std::size_t v) { return (v + 4095UL) & ~4095UL; }

constexpr std::size_t CHUNK_BYTES_R = 1UL << 20; // 1 MiB

/* ────────────────────────────────────────────────────────────────────────── */

DataTransferEngineReadXilinx::DataTransferEngineReadXilinx(std::string node)
    : devNode_(std::move(node)) {}

    DataTransferEngineReadXilinx::~DataTransferEngineReadXilinx()
{
    if (fd_ >= 0)
        ::close(fd_);
}

void DataTransferEngineReadXilinx::openIfNeeded()
{
    if (fd_ >= 0)
        return;

    fd_ = ::open(devNode_.c_str(), O_RDWR | O_SYNC | O_DIRECT);
    if (fd_ < 0 && errno == EINVAL)
        fd_ = ::open(devNode_.c_str(), O_RDWR | O_SYNC);

    chkR(fd_, "open(xdma c2h)");
}

std::size_t DataTransferEngineReadXilinx::transfer(uint64_t addr, void *buf, std::size_t len)
{
    openIfNeeded();

    chkR(::lseek64(fd_, static_cast<off64_t>(addr), SEEK_SET), "lseek64");

    uint8_t *     p    = static_cast<uint8_t *>(buf);
    std::size_t   left = len;

    while (left > 0) {
        ssize_t rd = ::read(fd_, p, left);
        if (rd < 0)
            chkR(rd, "read(device)");
        if (rd == 0)
            throw std::runtime_error("short read (0 bytes)");
        p += rd;
        left -= static_cast<std::size_t>(rd);
    }
    return len;
}

std::size_t DataTransferEngineReadXilinx::transferFile(uint64_t    addr,
                                               const std::string & path,
                                               std::size_t         len)
{
    // Prepare aligned staging buffer.
    const std::size_t bufBytes = align4096R(std::min(CHUNK_BYTES_R, len));
    void *            raw      = nullptr;
    if (::posix_memalign(&raw, 4096, bufBytes))
        throw std::bad_alloc();

    std::unique_ptr<uint8_t, decltype(&::free)> buf(reinterpret_cast<uint8_t *>(raw), &::free);

    int out = ::open(path.c_str(), O_CREAT | O_TRUNC | O_WRONLY | O_SYNC | O_DIRECT, 0666);
    if (out < 0 && errno == EINVAL)
        out = ::open(path.c_str(), O_CREAT | O_TRUNC | O_WRONLY | O_SYNC, 0666);
    chkR(out, "open(output)");

    std::size_t received = 0;
    while (received < len) {
        const std::size_t todo = std::min(bufBytes, len - received);

        // Read chunk from device.
        transfer(addr + received, buf.get(), todo);

        // Write chunk to file.
        std::size_t wrTotal = 0;
        while (wrTotal < todo) {
            ssize_t wr = ::write(out, buf.get() + wrTotal, todo - wrTotal);
            if (wr < 0)
                chkR(wr, "write(output)");
            if (wr == 0)
                throw std::runtime_error("short write (0 bytes)");
            wrTotal += static_cast<std::size_t>(wr);
        }

        received += todo;
    }

    ::close(out);
    return received;
}
