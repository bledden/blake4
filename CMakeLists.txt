cmake_minimum_required(VERSION 3.14)
project(blake4 VERSION 1.0.0 LANGUAGES C)

# Options
option(BLAKE4_BUILD_TESTS "Build test suite" ON)
option(BLAKE4_BUILD_SHARED "Build shared library" ON)
option(BLAKE4_BUILD_STATIC "Build static library" ON)

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

# Header files
set(BLAKE4_HEADERS
    include/blake4.h
)

# Static library
if(BLAKE4_BUILD_STATIC)
    add_library(blake4_static STATIC ${BLAKE4_SOURCES})
    target_include_directories(blake4_static PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )
    set_target_properties(blake4_static PROPERTIES
        OUTPUT_NAME blake4
        PUBLIC_HEADER "${BLAKE4_HEADERS}"
    )
    add_library(blake4::blake4_static ALIAS blake4_static)
endif()

# Shared library
if(BLAKE4_BUILD_SHARED)
    add_library(blake4_shared SHARED ${BLAKE4_SOURCES})
    target_include_directories(blake4_shared PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )
    set_target_properties(blake4_shared PROPERTIES
        OUTPUT_NAME blake4
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
        PUBLIC_HEADER "${BLAKE4_HEADERS}"
    )
    add_library(blake4::blake4_shared ALIAS blake4_shared)
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

    add_executable(test_basic test/test_basic.c)
    if(BLAKE4_BUILD_STATIC)
        target_link_libraries(test_basic PRIVATE blake4_static)
    else()
        target_link_libraries(test_basic PRIVATE blake4_shared)
    endif()

    add_test(NAME basic COMMAND test_basic)
endif()

# Install
include(GNUInstallDirs)

if(BLAKE4_BUILD_STATIC)
    install(TARGETS blake4_static
        EXPORT blake4-targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
endif()

if(BLAKE4_BUILD_SHARED)
    install(TARGETS blake4_shared
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
