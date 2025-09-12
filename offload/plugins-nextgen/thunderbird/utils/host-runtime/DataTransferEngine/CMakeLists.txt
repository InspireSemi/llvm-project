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

# Use PUBLIC if your public headers expose std::filesystem types
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang"
   OR (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9))
  target_link_libraries(DataTransferEngine PUBLIC stdc++fs)
endif()
 
install(TARGETS ${TARGET} DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/"
    DESTINATION ${CMAKE_INSTALL_PREFIX}/include
    FILES_MATCHING
    PATTERN "*.hpp"
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
