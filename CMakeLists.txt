cmake_minimum_required(VERSION 3.21 FATAL_ERROR)

########################################################################################################################
## Define project
########################################################################################################################
project(SHR
    VERSION     0.5.2.0
    LANGUAGES   CXX
    DESCRIPTION "Physiological heart-rate simulation with synthesized heartbeat audio."
)

option(BUILD_PLUGIN "Build the Skyrim plugin." ON)
option(BUILD_TESTS "Build unit tests." OFF)
option(BUILD_PYBIND "Build the offline Python binding." OFF)
message("Options:")
message("    Plugin: ${BUILD_PLUGIN}")
message("    Tests: ${BUILD_TESTS}")
message("    Pybind: ${BUILD_PYBIND}")

string(TIMESTAMP PROJECT_CONFIGURE_YEAR "%Y" UTC)
set(PROJECT_AUTHOR        "GrazedAnkle")
set(PROJECT_FRIENDLY_NAME "Skyrim Heart Rate for SKSE64")
set(PROJECT_COPYRIGHT     "SHR Copyright (C) ${PROJECT_CONFIGURE_YEAR} ${PROJECT_AUTHOR}")

set(CMAKE_CXX_STANDARD          23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(BUILD_PLUGIN)
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
endif()

set(PUBLIC_HEADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/${PROJECT_NAME}")

set(headers
    src/AcousticMapper.hpp
    src/AudioBuffer.hpp
    src/BeatRenderer.hpp
    src/BeatEvent.hpp
    src/BeatKind.hpp
    src/Config.hpp
    src/Constants.hpp
    src/EventHandler.hpp
    src/HeartbeatVoice.hpp
    src/HeartbeatSource.hpp
    src/HeartRate.hpp
    src/InputHandler.hpp
    src/Logging.hpp
    src/NotificationPolicy.hpp
    src/Pcm16.hpp
    src/pch.hpp
    src/PhysiologySnapshot.hpp
    src/Random.hpp
    src/RhythmEngine.hpp
    src/RhythmInput.hpp
    src/RhythmRandom.hpp
    src/RenderSpec.hpp
    src/Runtime.hpp
    src/RuntimeSettings.hpp
    src/Simulation.hpp
    src/SimulationSettings.hpp
    src/SimulationState.hpp
    src/SkyrimHeartRate.hpp
    src/StepInput.hpp
    src/StepResult.hpp
)

set(core_sources
    src/AcousticMapper.cpp
    src/BeatRenderer.cpp
    src/HeartbeatSource.cpp
    src/Pcm16.cpp
    src/RhythmEngine.cpp
    src/Runtime.cpp
    src/Simulation.cpp
)

set(sources
    src/Main.cpp
    src/Config.cpp
    src/HeartbeatVoice.cpp
    src/InputHandler.cpp
    src/Logging.cpp
    src/LoggingConfiguration.cpp
    src/NotificationPolicy.cpp
    src/SkyrimHeartRate.cpp
    src/EventHandler.cpp

    ${CMAKE_CURRENT_BINARY_DIR}/version.rc
)

set(tests
    src/Config.cpp
    src/LoggingConfiguration.cpp
    src/NotificationPolicy.cpp
    tests/AcousticMapperTests.cpp
    tests/AudioBufferTests.cpp
    tests/BeatRendererTests.cpp
    tests/BeatRenderFixtures.hpp
    tests/ConfigTests.cpp
    tests/HeartbeatSourceTests.cpp
    tests/LoggingTests.cpp
    tests/NotificationPolicyTests.cpp
    tests/Pcm16Tests.cpp
    tests/TestWav.hpp
    tests/RhythmEngineTests.cpp
    tests/RuntimeTests.cpp
    tests/SimulationCharacterizationTests.cpp
    tests/SimulationStateTests.cpp
    tests/SimulationTests.cpp
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}
    FILES
        ${headers}
        ${core_sources}
        ${sources}
        ${tests}
)

########################################################################################################################
## Add CMake features
########################################################################################################################
include(CheckIPOSupported)
include(GNUInstallDirs)

########################################################################################################################
## Find dependencies
########################################################################################################################
if(BUILD_PLUGIN)
    set(SHR_BUILD_TESTS ${BUILD_TESTS})
    set(BUILD_TESTS OFF)  # Prevent CommonLibSSE from building its own test suite
    set(ENABLE_SKYRIM_VR OFF CACHE BOOL "" FORCE)  # TODO(2026-06-06): Add VR support?
    add_subdirectory("extern/commonlibsse-ng" "CommonLibSSE" EXCLUDE_FROM_ALL)
    include("extern/commonlibsse-ng/cmake/CommonLibSSE.cmake")
    set(BUILD_TESTS ${SHR_BUILD_TESTS})  # Restore after CommonLibSSE subdirectory
endif()

if(BUILD_PLUGIN OR BUILD_TESTS)
    find_package(spdlog CONFIG REQUIRED)
    find_package(toml11 CONFIG REQUIRED)
endif()

# pocketfft is scipy's FFT backend. It is header-only and ships no CMake config package. It backs
# the core analytic envelope and scipy's offline mirror, so both compute the same transform.
find_path(POCKETFFT_INCLUDE_DIRS "pocketfft_hdronly.h" REQUIRED)

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
## Configure core library
########################################################################################################################
add_library(shr_core STATIC
    src/AcousticMapper.hpp
    src/AudioBuffer.hpp
    src/BeatRenderer.hpp
    src/BeatEvent.hpp
    src/BeatKind.hpp
    src/Constants.hpp
    src/HeartRate.hpp
    src/HeartbeatSource.hpp
    src/PhysiologySnapshot.hpp
    src/Pcm16.hpp
    src/Random.hpp
    src/RhythmEngine.hpp
    src/RhythmInput.hpp
    src/RhythmRandom.hpp
    src/RenderSpec.hpp
    src/Runtime.hpp
    src/RuntimeSettings.hpp
    src/Simulation.hpp
    src/SimulationSettings.hpp
    src/SimulationState.hpp
    src/StepInput.hpp
    src/StepResult.hpp
    ${core_sources}
)

add_library("${PROJECT_NAME}::core" ALIAS shr_core)

target_include_directories(shr_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    PRIVATE
        ${POCKETFFT_INCLUDE_DIRS}
)

target_compile_features(shr_core PUBLIC cxx_std_23)

########################################################################################################################
## Configure target DLL
########################################################################################################################
if(BUILD_PLUGIN)
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
        shr_core
        toml11::toml11
)

target_precompile_headers(${PROJECT_NAME}
    PRIVATE
        src/pch.hpp
)

# CMake flags for Release builds are suboptimal.
# See: https://gitlab.kitware.com/cmake/cmake/-/issues/20812.
# See: https://stackoverflow.com/questions/28178978/how-to-generate-pdb-files-for-release-build-with-cmake-flags.
if(MSVC)
    target_compile_options(${PROJECT_NAME}
        PRIVATE
            $<$<CONFIG:Release>:/Zc:inline>
            $<$<CONFIG:Release>:/Zi>
            $<$<CONFIG:Release>:/Gy>
    )
    target_link_options(${PROJECT_NAME}
        PRIVATE
            $<$<CONFIG:Release>:/DEBUG>
            $<$<CONFIG:Release>:/INCREMENTAL:NO>
            $<$<CONFIG:Release>:/OPT:REF>
            $<$<CONFIG:Release>:/OPT:ICF>
    )
elseif((CMAKE_CXX_COMPILER_ID STREQUAL "GNU") OR (CMAKE_CXX_COMPILER_ID MATCHES ".*Clang"))
    target_compile_options(${PROJECT_NAME}
        PRIVATE
            $<$<CONFIG:Release>:-g>
    )
    target_link_options(${PROJECT_NAME}
        PRIVATE
            $<$<CONFIG:Release>:-g>
    )
    if(WIN32)
        target_compile_options(${PROJECT_NAME}
            PRIVATE
                $<$<CONFIG:Release>:-gcodeview>
        )
    endif()
endif()

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
endif()

########################################################################################################################
## Configure unit tests
########################################################################################################################
if(BUILD_TESTS)
    include(CTest)
    include(Catch)

    add_executable(${PROJECT_NAME}Tests
        ${tests}
    )

    target_include_directories(${PROJECT_NAME}Tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/tests
    )

    target_compile_definitions(${PROJECT_NAME}Tests
        PRIVATE
            SHR_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    )

    target_link_libraries(${PROJECT_NAME}Tests
        PRIVATE
            Catch2::Catch2WithMain
            shr_core
            spdlog::spdlog
            toml11::toml11
    )

    catch_discover_tests(${PROJECT_NAME}Tests)

    add_executable(${PROJECT_NAME}SourceConditioningFixture
        tests/SourceConditioningFixture.cpp
        tests/TestWav.hpp
    )

    target_include_directories(${PROJECT_NAME}SourceConditioningFixture
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/tests
    )

    target_link_libraries(${PROJECT_NAME}SourceConditioningFixture
        PRIVATE
            shr_core
    )

    add_executable(${PROJECT_NAME}BeatRendererFixture
        tests/BeatRendererFixture.cpp
        tests/BeatRenderFixtures.hpp
        tests/TestWav.hpp
    )

    target_include_directories(${PROJECT_NAME}BeatRendererFixture
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/tests
    )

    target_link_libraries(${PROJECT_NAME}BeatRendererFixture
        PRIVATE
            shr_core
    )
endif()

########################################################################################################################
## Configure offline Python binding
########################################################################################################################
if(BUILD_PYBIND)
    # Build against the interpreter that will import the module (the analysis venv), so the extension
    # ABI and CRT match. Pass -DPython_EXECUTABLE=<venv python> and -Dpybind11_DIR=<venv pybind11 cmakedir>.
    find_package(Python 3 COMPONENTS Interpreter Development.Module REQUIRED)
    find_package(pybind11 CONFIG REQUIRED)

    pybind11_add_module(shr_pybind bindings/shr_pybind.cpp)

    target_include_directories(shr_pybind
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(shr_pybind
        PRIVATE
            shr_core
    )

    # LTO across the pybind11 extension boundary buys little here and risks clang-cl bitcode mismatches;
    # shr_core itself keeps its optimization settings.
    set_target_properties(shr_pybind PROPERTIES INTERPROCEDURAL_OPTIMIZATION OFF)
endif()
