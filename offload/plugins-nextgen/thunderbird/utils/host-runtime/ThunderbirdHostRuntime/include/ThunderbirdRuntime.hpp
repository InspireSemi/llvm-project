#pragma once

#include "DataTransferEngineReadBase.hpp"
#include "DataTransferEngineWriteBase.hpp"
#include <memory>

class ThunderbirdHostRuntime {
public:
    ThunderbirdHostRuntime(std::unique_ptr<DataTransferEngineReadBase> reader,
                          std::unique_ptr<DataTransferEngineWriteBase> writer);
    
    ~ThunderbirdHostRuntime();

    // Disable copy constructor and copy assignment
    ThunderbirdHostRuntime(const ThunderbirdHostRuntime&) = delete;
    ThunderbirdHostRuntime& operator=(const ThunderbirdHostRuntime&) = delete;

    // Enable move constructor and move assignment
    ThunderbirdHostRuntime(ThunderbirdHostRuntime&&) = default;
    ThunderbirdHostRuntime& operator=(ThunderbirdHostRuntime&&) = default;

    // Accessors
    DataTransferEngineReadBase* getReader() const { return m_reader.get(); }
    DataTransferEngineWriteBase* getWriter() const { return m_writer.get(); }

private:
    std::unique_ptr<DataTransferEngineReadBase> m_reader;
    std::unique_ptr<DataTransferEngineWriteBase> m_writer;
};
