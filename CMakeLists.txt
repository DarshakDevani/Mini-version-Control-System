cmake_minimum_required(VERSION 3.13)

project(git-starter-cpp)

set(CMAKE_CXX_STANDARD 23) # Enable the C++23 standard

# Use file(GLOB_RECURSE) to gather all source files
file(GLOB_RECURSE SOURCE_FILES src/*.cpp src/*.hpp)

# Create the executable
add_executable(git ${SOURCE_FILES})

# Find OpenSSL package
find_package(OpenSSL REQUIRED)
if (OpenSSL_FOUND)
    target_include_directories(git PRIVATE ${OpenSSL_INCLUDE_DIR})
    target_link_libraries(git PRIVATE OpenSSL::SSL OpenSSL::Crypto)
else()
    message(FATAL_ERROR "OpenSSL not found!")
endif()

# Find Zlib package
find_package(ZLIB REQUIRED)
if (ZLIB_FOUND)
    target_include_directories(git PRIVATE ${ZLIB_INCLUDE_DIRS})
    target_link_libraries(git PRIVATE ZLIB::ZLIB)
else()
    message(FATAL_ERROR "Zlib not found!")
endif()

# If you want to link to Zlib using the plain signature, you can replace the above line with:
# target_link_libraries(git -lz) 
