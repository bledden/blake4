cmake_minimum_required(VERSION 3.14)
project(blake4 VERSION 2.0.0 LANGUAGES C ASM)

# Options
option(BLAKE4_BUILD_TESTS "Build test suite" ON)
option(BLAKE4_BUILD_SHARED "Build shared library" ON)
option(BLAKE4_BUILD_STATIC "Build static library" ON)
option(BLAKE4_ENABLE_SIMD "Enable SIMD optimizations" ON)
option(BLAKE4_SANITIZE_ADDRESS "Enable AddressSanitizer" OFF)
option(BLAKE4_SANITIZE_UNDEFINED "Enable UndefinedBehaviorSanitizer" OFF)
option(BLAKE4_SANITIZE_MEMORY "Enable MemorySanitizer (Clang only)" OFF)
option(BLAKE4_SANITIZE_THREAD "Enable ThreadSanitizer" OFF)

# C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Compiler warnings
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -pedantic)
endif()

# Sanitizer configuration
set(BLAKE4_SANITIZER_FLAGS "")

if(BLAKE4_SANITIZE_ADDRESS)
    message(STATUS "BLAKE4: AddressSanitizer enabled")
    list(APPEND BLAKE4_SANITIZER_FLAGS "-fsanitize=address")
    # Disable SIMD with ASan for better error detection
    set(BLAKE4_ENABLE_SIMD OFF CACHE BOOL "Disabled due to ASan" FORCE)
endif()

if(BLAKE4_SANITIZE_UNDEFINED)
    message(STATUS "BLAKE4: UndefinedBehaviorSanitizer enabled")
    list(APPEND BLAKE4_SANITIZER_FLAGS "-fsanitize=undefined")
endif()

if(BLAKE4_SANITIZE_MEMORY)
    if(CMAKE_C_COMPILER_ID MATCHES "Clang")
        message(STATUS "BLAKE4: MemorySanitizer enabled")
        list(APPEND BLAKE4_SANITIZER_FLAGS "-fsanitize=memory")
        # MSan requires -fno-omit-frame-pointer for better stack traces
        list(APPEND BLAKE4_SANITIZER_FLAGS "-fno-omit-frame-pointer")
    else()
        message(WARNING "BLAKE4: MemorySanitizer is only supported by Clang")
    endif()
endif()

if(BLAKE4_SANITIZE_THREAD)
    message(STATUS "BLAKE4: ThreadSanitizer enabled")
    list(APPEND BLAKE4_SANITIZER_FLAGS "-fsanitize=thread")
endif()

if(BLAKE4_SANITIZER_FLAGS)
    add_compile_options(${BLAKE4_SANITIZER_FLAGS})
    add_link_options(${BLAKE4_SANITIZER_FLAGS})
endif()

# Source files
set(BLAKE4_SOURCES
    src/blake4.c
    src/blake4_dispatch.c
    src/blake4_parallel.c
    src/blake4_ct.c
)

# Thread library detection
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads)

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
    include/blake4_dispatch.h
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
            # AVX-512 intrinsics and assembly
            list(APPEND BLAKE4_SOURCES src/blake4_avx512.c)
            list(APPEND BLAKE4_SOURCES src/blake4_avx512_x86-64.S)
            list(APPEND BLAKE4_SOURCES src/blake4_avx512_asm.c)
            # Also include AVX2 for broader compatibility testing
            list(APPEND BLAKE4_SOURCES src/blake4_avx2.c)
            list(APPEND BLAKE4_SOURCES src/blake4_avx2_x86-64.S)
            list(APPEND BLAKE4_SOURCES src/blake4_avx2_asm.c)
        elseif(HAS_AVX2)
            message(STATUS "BLAKE4: AVX2 support detected")
            set(BLAKE4_SIMD_FLAGS "-mavx2")
            # AVX2 intrinsics and assembly
            list(APPEND BLAKE4_SOURCES src/blake4_avx2.c)
            list(APPEND BLAKE4_SOURCES src/blake4_avx2_x86-64.S)
            list(APPEND BLAKE4_SOURCES src/blake4_avx2_asm.c)
        else()
            message(STATUS "BLAKE4: No x86 SIMD support detected")
        endif()
    # ARM SIMD flags
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
        message(STATUS "BLAKE4: ARM64 detected, NEON enabled by default")
        # NEON is typically enabled by default on ARM64
        # Add NEON intrinsics source
        list(APPEND BLAKE4_SOURCES src/blake4_neon.c)
        # Add NEON assembly source
        list(APPEND BLAKE4_SOURCES src/blake4_neon_arm64.S)
        list(APPEND BLAKE4_SOURCES src/blake4_neon_asm.c)
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
    # Link thread library for parallel hashing
    if(Threads_FOUND)
        target_link_libraries(blake4_static PRIVATE Threads::Threads)
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
    # Link thread library for parallel hashing
    if(Threads_FOUND)
        target_link_libraries(blake4_shared PRIVATE Threads::Threads)
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

    # Benchmark tool
    add_executable(benchmark tools/benchmark.c)
    if(BLAKE4_BUILD_STATIC)
        target_link_libraries(benchmark PRIVATE blake4_static)
    else()
        target_link_libraries(benchmark PRIVATE blake4_shared)
    endif()
endif()

# Fuzzing targets (libFuzzer)
# Note: Requires LLVM Clang with libFuzzer (not Apple Clang).
# Install via: brew install llvm
# Build with: cmake .. -DBLAKE4_BUILD_FUZZ=ON -DCMAKE_C_COMPILER=/opt/homebrew/opt/llvm/bin/clang
option(BLAKE4_BUILD_FUZZ "Build fuzzing harnesses (requires LLVM Clang)" OFF)

if(BLAKE4_BUILD_FUZZ)
    if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "Fuzzing requires Clang compiler with libFuzzer")
    endif()

    # Check if this is Apple Clang (which lacks libFuzzer)
    if(CMAKE_C_COMPILER_ID STREQUAL "AppleClang")
        message(WARNING "Apple Clang may not include libFuzzer. Use LLVM Clang instead:\n  brew install llvm\n  cmake .. -DCMAKE_C_COMPILER=/opt/homebrew/opt/llvm/bin/clang")
    endif()

    message(STATUS "BLAKE4: Building fuzz targets")

    # Common fuzzer flags
    set(FUZZER_FLAGS "-fsanitize=fuzzer,address,undefined" "-fno-omit-frame-pointer")

    # Fuzz one-shot hashing
    add_executable(fuzz_hash fuzz/fuzz_hash.c)
    target_compile_options(fuzz_hash PRIVATE ${FUZZER_FLAGS})
    target_link_options(fuzz_hash PRIVATE ${FUZZER_FLAGS})
    if(BLAKE4_BUILD_STATIC)
        target_link_libraries(fuzz_hash PRIVATE blake4_static)
    else()
        target_link_libraries(fuzz_hash PRIVATE blake4_shared)
    endif()

    # Fuzz incremental API
    add_executable(fuzz_incremental fuzz/fuzz_incremental.c)
    target_compile_options(fuzz_incremental PRIVATE ${FUZZER_FLAGS})
    target_link_options(fuzz_incremental PRIVATE ${FUZZER_FLAGS})
    if(BLAKE4_BUILD_STATIC)
        target_link_libraries(fuzz_incremental PRIVATE blake4_static)
    else()
        target_link_libraries(fuzz_incremental PRIVATE blake4_shared)
    endif()

    # Fuzz deserialization
    add_executable(fuzz_deserialize fuzz/fuzz_deserialize.c)
    target_compile_options(fuzz_deserialize PRIVATE ${FUZZER_FLAGS})
    target_link_options(fuzz_deserialize PRIVATE ${FUZZER_FLAGS})
    if(BLAKE4_BUILD_STATIC)
        target_link_libraries(fuzz_deserialize PRIVATE blake4_static)
    else()
        target_link_libraries(fuzz_deserialize PRIVATE blake4_shared)
    endif()

    # Fuzz keyed hashing
    add_executable(fuzz_keyed fuzz/fuzz_keyed.c)
    target_compile_options(fuzz_keyed PRIVATE ${FUZZER_FLAGS})
    target_link_options(fuzz_keyed PRIVATE ${FUZZER_FLAGS})
    if(BLAKE4_BUILD_STATIC)
        target_link_libraries(fuzz_keyed PRIVATE blake4_static)
    else()
        target_link_libraries(fuzz_keyed PRIVATE blake4_shared)
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
