set(TARGET DataTransferEngine)

add_library(${TARGET}
    src/DataTransferEngineWriteXilinx.cpp
    src/DataTransferEngineReadXilinx.cpp
    src/DataTransferEngineWriteThunderbird.cpp
    src/DataTransferEngineReadThunderbird.cpp
    src/DataTransferEngineWriteThunderbirdQEMU.cpp
    src/DataTransferEngineReadThunderbirdQEMU.cpp
    src/DataTransferEngineFactory.cpp
)

target_include_directories(${TARGET} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
