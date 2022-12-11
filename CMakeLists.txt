cmake_minimum_required(VERSION 3.21 FATAL_ERROR)

########################################################################################################################
## Define project
########################################################################################################################
project(SHR
    VERSION     0.3.0.0
    LANGUAGES   CXX
    DESCRIPTION "A mod that simulates changes in heart rate and their effects on the player."
)

string(TIMESTAMP PROJECT_CONFIGURE_YEAR "%Y" UTC)
set(PROJECT_AUTHOR        "GrazedAnkle")
set(PROJECT_FRIENDLY_NAME "Skyrim Heart Rate for SKSE64")
set(PROJECT_COPYRIGHT     "SHR Copyright (C) ${PROJECT_CONFIGURE_YEAR} ${PROJECT_AUTHOR}")

set(CMAKE_CXX_STANDARD          23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

configure_file(
    src/version.rc.in
    ${CMAKE_CURRENT_BINARY_DIR}/version.rc
    @ONLY
)

configure_file(
    src/fomod/info.xml.in
    ${CMAKE_CURRENT_BINARY_DIR}/Distribution/fomod/info.xml
    @ONLY
)

configure_file(
    src/fomod/ModuleConfig.xml.in
    ${CMAKE_CURRENT_BINARY_DIR}/Distribution/fomod/ModuleConfig.xml
    @ONLY
)

set(PUBLIC_HEADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/${PROJECT_NAME}")

set(headers)

set(sources
    src/Main.cpp
    src/Config.cpp
    src/InputHandler.cpp
    src/Logging.cpp
    src/SkyrimHeartRate.cpp
    src/EventHandler.cpp
    src/Sound.cpp
    src/Strings.cpp

    ${CMAKE_CURRENT_BINARY_DIR}/version.rc
)

set(tests)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}
    FILES
        ${headers}
        ${sources}
        ${tests}
)

#########################################################################################################################
### Build options
#########################################################################################################################
option(BUILD_TESTS "Build unit tests." OFF)
message("Options:")
message("    Tests: ${BUILD_TESTS}")

########################################################################################################################
## Add CMake features
########################################################################################################################
include(CheckIPOSupported)
include(GNUInstallDirs)

########################################################################################################################
## Find dependencies
########################################################################################################################
find_package(CommonLibSSE        REQUIRED)
find_package(toml11       CONFIG REQUIRED)

if(BUILD_TESTS)
    find_package(Catch2 CONFIG REQUIRED)
endif()

########################################################################################################################
## Configuration for all targets
########################################################################################################################
if(WIN32)
    add_compile_definitions(
        UNICODE
        _UNICODE
        _AMD64_
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_definitions(
        __cpp_lib_char8_t
        __cpp_consteval
        __cpp_lib_format
    )
endif()

check_ipo_supported(RESULT USE_IPO OUTPUT IPO_OUTPUT)
if(USE_IPO)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
else()
    message(WARNING "Interprocedural optimizations are not supported.")
endif()

########################################################################################################################
## Configure target DLL
########################################################################################################################
add_commonlibsse_plugin(${PROJECT_NAME} SOURCES ${headers} ${sources})

add_library("${PROJECT_NAME}::${PROJECT_NAME}" ALIAS "${PROJECT_NAME}")

target_include_directories(${PROJECT_NAME}
    PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/src>
        $<INSTALL_INTERFACE:src>
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        toml11::toml11
)

target_precompile_headers(${PROJECT_NAME}
    PRIVATE
        src/pch.hpp
)

install(
    DIRECTORY "${PUBLIC_HEADER_DIR}"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

install(
    TARGETS ${PROJECT_NAME}
    DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)

# Automatic deployment to FOMOD directory.
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(BUILD_NAME "Debug")
else()
    set(BUILD_NAME "Release")
endif()
install(DIRECTORY DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/Distribution/Papyrus${BUILD_NAME}/")
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/Distribution/Plugin${BUILD_NAME}/"
)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}> "${CMAKE_CURRENT_BINARY_DIR}/Distribution/Plugin${BUILD_NAME}/"
)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_PDB_FILE:${PROJECT_NAME}> "${CMAKE_CURRENT_BINARY_DIR}/Distribution/Plugin${BUILD_NAME}/"
    )
endif()
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/contrib/Distribution "${CMAKE_CURRENT_BINARY_DIR}/Distribution"
)
file(GLOB_RECURSE OUTPUT_DLLS "${CMAKE_CURRENT_BINARY_DIR}/Distribution/**/*.dll")
file(GLOB_RECURSE OUTPUT_PDBS "${CMAKE_CURRENT_BINARY_DIR}/Distribution/**/*.pdb")
file(GLOB_RECURSE OUTPUT_SCRIPTS "${CMAKE_CURRENT_BINARY_DIR}/Distribution/**/*.pex")
set_property(TARGET ${PROJECT_NAME}
    APPEND PROPERTY ADDITIONAL_CLEAN_FILES "${OUTPUT_DLLS}"
)
set_property(TARGET ${PROJECT_NAME}
    APPEND PROPERTY ADDITIONAL_CLEAN_FILES "${OUTPUT_PDBS}"
)
set_property(TARGET ${PROJECT_NAME}
    APPEND PROPERTY ADDITIONAL_CLEAN_FILES "${OUTPUT_SCRIPTS}"
)

set(DISTRIBUTION_ARCHIVE_NAME "${PROJECT_NAME}-SKSE64-${PROJECT_VERSION}")
add_custom_target(Distribution ALL
    COMMAND ${CMAKE_COMMAND} -E tar "cvf" "${CMAKE_CURRENT_BINARY_DIR}/${DISTRIBUTION_ARCHIVE_NAME}.zip" --format=zip -- .
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Distribution"
)

set_property(TARGET ${PROJECT_NAME}
    APPEND PROPERTY ADDITIONAL_CLEAN_FILES "${CMAKE_CURRENT_BINARY_DIR}/${DISTRIBUTION_ARCHIVE_NAME}.zip"
)

add_dependencies(Distribution
    ${PROJECT_NAME}
)

# Automatic deployment to Mod Organizer 2 mod directory.
foreach(DEPLOY_TARGET $ENV{${PROJECT_NAME}Targets})
    message("Adding deployment target ${DEPLOY_TARGET}.")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}> "${DEPLOY_TARGET}/SKSE/Plugins/"
    )
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_PDB_FILE:${PROJECT_NAME}> "${DEPLOY_TARGET}/SKSE/Plugins/"
    )
endforeach()

########################################################################################################################
## Configure unit tests
########################################################################################################################
if(BUILD_TESTS AND NOT " ${tests}" STREQUAL " ")
    include(CTest)
    include(Catch)

    add_executable(${PROJECT_NAME}Tests
        ${headers}
        ${tests}
    )

    target_link_libraries(${PROJECT_NAME}Tests
        PRIVATE
            ${PROJECT_NAME}
            Catch2::Catch2WithMain
    )

    target_precompile_headers(${PROJECT_NAME}Tests
        PRIVATE
            src/pch.hpp
    )

    catch_discover_tests(${PROJECT_NAME}Tests)
    add_test(NAME ${PROJECT_NAME}Tests COMMAND ${PROJECT_NAME}Tests)
endif()
