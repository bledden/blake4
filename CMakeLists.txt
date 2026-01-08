cmake_minimum_required(VERSION 3.14)
project(blake4 VERSION 2.0.0 LANGUAGES C)

# Options
option(BLAKE4_BUILD_TESTS "Build test suite" ON)
option(BLAKE4_BUILD_SHARED "Build shared library" ON)
option(BLAKE4_BUILD_STATIC "Build static library" ON)
option(BLAKE4_ENABLE_SIMD "Enable SIMD optimizations" ON)

# C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Compiler warnings
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -pedantic)
endif()

# Source files
set(BLAKE4_SOURCES
    src/blake4.c
)

set(BLAKE4_STREAM_SOURCES
    src/blake4_stream.c
)

# SIMD sources (optional)
set(BLAKE4_SIMD_SOURCES
    src/blake4_simd.c
)

# Header files
set(BLAKE4_HEADERS
    include/blake4.h
)

set(BLAKE4_STREAM_HEADERS
    include/blake4_stream.h
)

set(BLAKE4_SIMD_HEADERS
    include/blake4_simd.h
)

# SIMD configuration
if(BLAKE4_ENABLE_SIMD)
    # Detect platform and set appropriate flags
    include(CheckCCompilerFlag)

    # x86-64 SIMD flags
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64")
        check_c_compiler_flag("-mavx2" HAS_AVX2)
        check_c_compiler_flag("-mavx512f -mavx512vl" HAS_AVX512)

        if(HAS_AVX512)
            message(STATUS "BLAKE4: AVX-512 support detected")
            set(BLAKE4_SIMD_FLAGS "-mavx512f" "-mavx512vl")
        elseif(HAS_AVX2)
            message(STATUS "BLAKE4: AVX2 support detected")
            set(BLAKE4_SIMD_FLAGS "-mavx2")
        else()
            message(STATUS "BLAKE4: No x86 SIMD support detected")
        endif()
    # ARM SIMD flags
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
        message(STATUS "BLAKE4: ARM64 detected, NEON enabled by default")
        # NEON is typically enabled by default on ARM64
    else()
        message(STATUS "BLAKE4: Unknown architecture, SIMD disabled")
    endif()

    # Add SIMD sources to the main library
    list(APPEND BLAKE4_SOURCES ${BLAKE4_SIMD_SOURCES})
    list(APPEND BLAKE4_HEADERS ${BLAKE4_SIMD_HEADERS})
endif()

# Static library (core)
if(BLAKE4_BUILD_STATIC)
    add_library(blake4_static STATIC ${BLAKE4_SOURCES})
    target_include_directories(blake4_static PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )
    if(BLAKE4_SIMD_FLAGS)
        target_compile_options(blake4_static PRIVATE ${BLAKE4_SIMD_FLAGS})
    endif()
    set_target_properties(blake4_static PROPERTIES
        OUTPUT_NAME blake4
        PUBLIC_HEADER "${BLAKE4_HEADERS}"
    )
    add_library(blake4::blake4_static ALIAS blake4_static)

    # Static library (stream)
    add_library(blake4_stream_static STATIC ${BLAKE4_STREAM_SOURCES})
    target_include_directories(blake4_stream_static PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )
    target_link_libraries(blake4_stream_static PUBLIC blake4_static)
    set_target_properties(blake4_stream_static PROPERTIES
        OUTPUT_NAME blake4_stream
        PUBLIC_HEADER "${BLAKE4_STREAM_HEADERS}"
    )
    add_library(blake4::blake4_stream_static ALIAS blake4_stream_static)
endif()

# Shared library (core)
if(BLAKE4_BUILD_SHARED)
    add_library(blake4_shared SHARED ${BLAKE4_SOURCES})
    target_include_directories(blake4_shared PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )
    if(BLAKE4_SIMD_FLAGS)
        target_compile_options(blake4_shared PRIVATE ${BLAKE4_SIMD_FLAGS})
    endif()
    set_target_properties(blake4_shared PROPERTIES
        OUTPUT_NAME blake4
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
        PUBLIC_HEADER "${BLAKE4_HEADERS}"
    )
    add_library(blake4::blake4_shared ALIAS blake4_shared)

    # Shared library (stream)
    add_library(blake4_stream_shared SHARED ${BLAKE4_STREAM_SOURCES})
    target_include_directories(blake4_stream_shared PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )
    target_link_libraries(blake4_stream_shared PUBLIC blake4_shared)
    set_target_properties(blake4_stream_shared PROPERTIES
        OUTPUT_NAME blake4_stream
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
        PUBLIC_HEADER "${BLAKE4_STREAM_HEADERS}"
    )
    add_library(blake4::blake4_stream_shared ALIAS blake4_stream_shared)
endif()

# Default target alias
if(BLAKE4_BUILD_STATIC)
    add_library(blake4::blake4 ALIAS blake4_static)
elseif(BLAKE4_BUILD_SHARED)
    add_library(blake4::blake4 ALIAS blake4_shared)
endif()

# Tests
if(BLAKE4_BUILD_TESTS)
    enable_testing()

    # Core tests
    add_executable(test_blake4 test/test_blake4.c)
    if(BLAKE4_BUILD_STATIC)
        target_link_libraries(test_blake4 PRIVATE blake4_static)
    else()
        target_link_libraries(test_blake4 PRIVATE blake4_shared)
    endif()
    add_test(NAME blake4 COMMAND test_blake4)

    # Stream tests
    add_executable(test_stream test/test_stream.c)
    if(BLAKE4_BUILD_STATIC)
        target_link_libraries(test_stream PRIVATE blake4_stream_static)
    else()
        target_link_libraries(test_stream PRIVATE blake4_stream_shared)
    endif()
    add_test(NAME stream COMMAND test_stream)

    # Vector generator tool
    add_executable(gen_vectors tools/gen_vectors.c)
    if(BLAKE4_BUILD_STATIC)
        target_link_libraries(gen_vectors PRIVATE blake4_stream_static)
    else()
        target_link_libraries(gen_vectors PRIVATE blake4_stream_shared)
    endif()
endif()

# Install
include(GNUInstallDirs)

if(BLAKE4_BUILD_STATIC)
    install(TARGETS blake4_static blake4_stream_static
        EXPORT blake4-targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
endif()

if(BLAKE4_BUILD_SHARED)
    install(TARGETS blake4_shared blake4_stream_shared
        EXPORT blake4-targets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
endif()

# CMake package config
install(EXPORT blake4-targets
    FILE blake4-targets.cmake
    NAMESPACE blake4::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/blake4
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/blake4-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/blake4-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/blake4-config.cmake
    @ONLY
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/blake4-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/blake4-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/blake4
)

# pkg-config
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/blake4.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/blake4.pc
    @ONLY
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/blake4.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)
