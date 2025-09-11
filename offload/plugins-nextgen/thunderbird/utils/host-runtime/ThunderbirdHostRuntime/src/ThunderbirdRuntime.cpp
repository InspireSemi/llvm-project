#include "ThunderbirdRuntime.hpp"

ThunderbirdHostRuntime::ThunderbirdHostRuntime(std::unique_ptr<DataTransferEngineReadBase> reader,
                                             std::unique_ptr<DataTransferEngineWriteBase> writer)
    : m_reader(std::move(reader)), m_writer(std::move(writer))
{
    // Constructor implementation
}

ThunderbirdHostRuntime::~ThunderbirdHostRuntime() = default;
