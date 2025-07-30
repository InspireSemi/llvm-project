set(TARGET DataTransferEngine)

add_library(${TARGET} SHARED
    src/DataTransferEngineWriteXilinx.cpp
    src/DataTransferEngineReadXilinx.cpp
    src/DataTransferEngineWriteThunderbird.cpp
    src/DataTransferEngineReadThunderbird.cpp
    src/DataTransferEngineWriteThunderbirdQEMU.cpp
    src/DataTransferEngineReadThunderbirdQEMU.cpp
    src/DataTransferEngineThunderbirdQEMUBase.cpp
    src/DataTransferEngineFactory.cpp
)

target_include_directories(${TARGET} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

install(TARGETS ${TARGET} DESTINATION ${CMAKE_INSTALL_PREFIX/lib})

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/"
            DESTINATION ${CMAKE_INSTALL_PREFIX}/include
            FILES_MATCHING
            PATTERN "*.hpp")

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
