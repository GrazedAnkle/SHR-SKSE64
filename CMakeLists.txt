cmake_minimum_required(VERSION 3.21 FATAL_ERROR)

########################################################################################################################
## Define project
########################################################################################################################
project(SHR
    VERSION     0.5.3.0
    LANGUAGES   CXX
    DESCRIPTION "Physiological heart-rate simulation with synthesized heartbeat audio."
)

# Namespaced so they cannot collide with an option of the same name in a dependency added via
# add_subdirectory. CommonLibSSE defines its own BUILD_TESTS, which an unnamespaced option here would
# alias.
option(SHR_BUILD_PLUGIN "Build the Skyrim plugin." ON)
option(SHR_BUILD_TESTS "Build unit tests." OFF)
option(SHR_BUILD_PYBIND "Build the offline Python binding." OFF)
message("Options:")
message("    Plugin: ${SHR_BUILD_PLUGIN}")
message("    Tests: ${SHR_BUILD_TESTS}")
message("    Pybind: ${SHR_BUILD_PYBIND}")

string(TIMESTAMP PROJECT_CONFIGURE_YEAR "%Y" UTC)
set(PROJECT_AUTHOR        "GrazedAnkle")
set(PROJECT_FRIENDLY_NAME "Skyrim Heart Rate for SKSE64")
set(PROJECT_COPYRIGHT     "SHR Copyright (C) ${PROJECT_CONFIGURE_YEAR} ${PROJECT_AUTHOR}")

set(CMAKE_CXX_STANDARD          23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(SHR_BUILD_PLUGIN)
    configure_file(
        src/plugin/version.rc.in
        ${CMAKE_CURRENT_BINARY_DIR}/version.rc
        @ONLY
    )

    configure_file(
        src/plugin/fomod/info.xml.in
        ${CMAKE_CURRENT_BINARY_DIR}/Distribution/fomod/info.xml
        @ONLY
    )

    configure_file(
        src/plugin/fomod/ModuleConfig.xml.in
        ${CMAKE_CURRENT_BINARY_DIR}/Distribution/fomod/ModuleConfig.xml
        @ONLY
    )
endif()

# One list per source directory, and each target below lists exactly one of them. A file's directory
# states which target owns it; no file appears in two lists. Layer directories may include downward
# only (plugin -> adapter -> core), which tools/check_layering.py enforces at the include site.

set(core_sources
    src/core/AcousticMapper.cpp
    src/core/AcousticMapper.hpp
    src/core/AudioBuffer.hpp
    src/core/BeatEvent.hpp
    src/core/BeatKind.hpp
    src/core/BeatRenderer.cpp
    src/core/BeatRenderer.hpp
    src/core/Constants.hpp
    src/core/HeartbeatSource.cpp
    src/core/HeartbeatSource.hpp
    src/core/ModelCoefficientRegistry.hpp
    src/core/ModelCoefficients.cpp
    src/core/ModelCoefficients.hpp
    src/core/Pcm16.cpp
    src/core/Pcm16.hpp
    src/core/PhysiologySnapshot.hpp
    src/core/Random.hpp
    src/core/RenderSpec.hpp
    src/core/RhythmEngine.cpp
    src/core/RhythmEngine.hpp
    src/core/RhythmInput.hpp
    src/core/RhythmRandom.hpp
    src/core/Runtime.cpp
    src/core/Runtime.hpp
    src/core/RuntimeEventMailbox.hpp
    src/core/RuntimeSettings.hpp
    src/core/Simulation.cpp
    src/core/Simulation.hpp
    src/core/SimulationSettings.hpp
    src/core/SimulationState.hpp
    src/core/StepInput.hpp
    src/core/StepResult.hpp
)

set(adapter_sources
    src/adapter/Config.cpp
    src/adapter/Config.hpp
    src/adapter/HeartRate.hpp
    src/adapter/HeartRateLevelTracker.hpp
    src/adapter/LoggingConfiguration.cpp
    src/adapter/LoggingConfiguration.hpp
    src/adapter/NotificationPolicy.cpp
    src/adapter/NotificationPolicy.hpp
)

set(plugin_sources
    src/plugin/EventHandler.cpp
    src/plugin/EventHandler.hpp
    src/plugin/HeartbeatVoice.cpp
    src/plugin/HeartbeatVoice.hpp
    src/plugin/InputHandler.cpp
    src/plugin/InputHandler.hpp
    src/plugin/Logging.cpp
    src/plugin/Logging.hpp
    src/plugin/Main.cpp
    src/plugin/SinkBuffer.hpp
    src/plugin/SkyrimHeartRate.cpp
    src/plugin/SkyrimHeartRate.hpp
    src/plugin/ThreadTrace.hpp
    src/plugin/pch.hpp
)

set(test_sources
    tests/AcousticMapperTests.cpp
    tests/AudioBufferTests.cpp
    tests/BeatRendererTests.cpp
    tests/ConfigTests.cpp
    tests/HeartRateLevelTrackerTests.cpp
    tests/HeartbeatSourceTests.cpp
    tests/LoggingTests.cpp
    tests/ModelCoefficientsTests.cpp
    tests/NotificationPolicyTests.cpp
    tests/Pcm16Tests.cpp
    tests/TestWav.hpp
    tests/RhythmEngineTests.cpp
    tests/RuntimeEventMailboxTests.cpp
    tests/RuntimeTests.cpp
    tests/SinkBufferTests.cpp
    tests/SimulationCharacterizationTests.cpp
    tests/SimulationStateTests.cpp
    tests/SimulationTests.cpp
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}
    FILES
        ${core_sources}
        ${adapter_sources}
        ${plugin_sources}
        ${test_sources}
)

########################################################################################################################
## Add CMake features
########################################################################################################################
include(CheckIPOSupported)

########################################################################################################################
## Find dependencies
########################################################################################################################
if(SHR_BUILD_PLUGIN)
    # CommonLibSSE's own BUILD_TESTS defaults to ON; turn it off so it does not configure its test
    # suite. Under CMP0077 its option() honors this normal variable. Nothing needs saving and
    # restoring around the call any more: this project's options are SHR_-prefixed, so the name no
    # longer refers to two different settings.
    set(BUILD_TESTS OFF)
    set(ENABLE_SKYRIM_VR OFF CACHE BOOL "" FORCE)  # TODO(2026-06-06): Add VR support?
    add_subdirectory("extern/commonlibsse-ng" "CommonLibSSE" EXCLUDE_FROM_ALL)
    include("extern/commonlibsse-ng/cmake/CommonLibSSE.cmake")
endif()

if(SHR_BUILD_PLUGIN OR SHR_BUILD_TESTS)
    find_package(spdlog CONFIG REQUIRED)
    find_package(toml11 CONFIG REQUIRED)
endif()

# pocketfft is scipy's FFT backend. It is header-only and ships no CMake config package. It backs
# the core analytic envelope and scipy's offline mirror, so both compute the same transform.
find_path(POCKETFFT_INCLUDE_DIRS "pocketfft_hdronly.h" REQUIRED)

if(SHR_BUILD_TESTS)
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

# Applied per target below rather than globally: the pybind11 extension deliberately opts out.
check_ipo_supported(RESULT USE_IPO OUTPUT IPO_OUTPUT)
if(NOT USE_IPO)
    message(WARNING "Interprocedural optimizations are not supported.")
endif()

########################################################################################################################
## Configure core library
########################################################################################################################
add_library(shr_core STATIC ${core_sources})

add_library("${PROJECT_NAME}::core" ALIAS shr_core)

target_include_directories(shr_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    PRIVATE
        ${POCKETFFT_INCLUDE_DIRS}
)

target_compile_features(shr_core PUBLIC cxx_std_23)

set_target_properties(shr_core PROPERTIES INTERPROCEDURAL_OPTIMIZATION ${USE_IPO})

########################################################################################################################
## Configure adapter policy library
########################################################################################################################
# Skyrim-free adapter policy: configuration parsing, logging configuration, and notification message
# selection. Separated from the plugin so the tests link one built artifact instead of recompiling
# these sources with a second set of compilation settings.
if(SHR_BUILD_PLUGIN OR SHR_BUILD_TESTS)
    add_library(shr_adapter STATIC ${adapter_sources})

    add_library("${PROJECT_NAME}::adapter" ALIAS shr_adapter)

    target_link_libraries(shr_adapter
        PUBLIC
            shr_core
            spdlog::spdlog   # Config.hpp stores spdlog::level::level_enum, so this crosses its API.
        PRIVATE
            toml11::toml11   # Serialization lives entirely in Config.cpp.
    )

    set_target_properties(shr_adapter PROPERTIES INTERPROCEDURAL_OPTIMIZATION ${USE_IPO})
endif()

########################################################################################################################
## Configure target DLL
########################################################################################################################
if(SHR_BUILD_PLUGIN)
    # version.rc is generated into the binary directory, so it stays out of ${plugin_sources}:
    # source_group's TREE root is the source directory, and a binary directory outside it fails to
    # configure.
    add_commonlibsse_plugin(${PROJECT_NAME}
        SOURCES
            ${plugin_sources}
            ${CMAKE_CURRENT_BINARY_DIR}/version.rc
    )

    add_library("${PROJECT_NAME}::${PROJECT_NAME}" ALIAS "${PROJECT_NAME}")

    set_target_properties(${PROJECT_NAME} PROPERTIES INTERPROCEDURAL_OPTIMIZATION ${USE_IPO})

    target_include_directories(${PROJECT_NAME}
        PRIVATE
            $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/src>
    )

    # shr_core and spdlog arrive transitively; the plugin reaches core types through adapter headers
    # and directly, and both are PUBLIC on shr_adapter.
    target_link_libraries(${PROJECT_NAME}
        PRIVATE
            shr_adapter
    )

    target_precompile_headers(${PROJECT_NAME}
        PRIVATE
            src/plugin/pch.hpp
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

    # Automatic deployment to FOMOD directory.
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(BUILD_NAME "Debug")
    else()
        set(BUILD_NAME "Release")
    endif()
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
if(SHR_BUILD_TESTS)
    include(CTest)
    include(Catch)

    add_executable(${PROJECT_NAME}Tests
        ${test_sources}
    )

    # src/ arrives with shr_core's usage requirements; only the suite's own headers need adding.
    target_include_directories(${PROJECT_NAME}Tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/tests
    )

    target_compile_definitions(${PROJECT_NAME}Tests
        PRIVATE
            SHR_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    )

    # shr_adapter carries shr_core and spdlog; the suite compiles no src/ sources of its own.
    target_link_libraries(${PROJECT_NAME}Tests
        PRIVATE
            Catch2::Catch2WithMain
            shr_adapter
    )

    # Tags become CTest labels without their brackets: `ctest -L fatigue`, not `-L [fatigue]`.
    catch_discover_tests(${PROJECT_NAME}Tests ADD_TAGS_AS_LABELS)
endif()

########################################################################################################################
## Configure offline Python binding
########################################################################################################################
if(SHR_BUILD_PYBIND)
    # The extension must build against the interpreter that will import it so its ABI and CRT match.
    # Callers pass -DPython_EXECUTABLE (presets pin the .venv interpreter; build_pybind.py passes whichever
    # interpreter runs it).
    find_package(Python 3 COMPONENTS Interpreter Development.Module REQUIRED)

    # Derive pybind11's CMake package from that interpreter so `cmake --preset` needs no -Dpybind11_DIR.
    # An explicit -Dpybind11_DIR still wins.
    if(NOT pybind11_DIR)
        execute_process(
            COMMAND "${Python_EXECUTABLE}" -m pybind11 --cmakedir
            OUTPUT_VARIABLE  SHR_PYBIND11_CMAKEDIR
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE  SHR_PYBIND11_CMAKEDIR_RESULT
        )
        if(NOT SHR_PYBIND11_CMAKEDIR_RESULT EQUAL 0)
            message(FATAL_ERROR
                "Could not derive pybind11's CMake dir from ${Python_EXECUTABLE}. Install the analysis "
                "requirements into that interpreter (pip install -r requirements.txt) or pass -Dpybind11_DIR."
            )
        endif()
        list(APPEND CMAKE_PREFIX_PATH "${SHR_PYBIND11_CMAKEDIR}")
    endif()
    find_package(pybind11 CONFIG REQUIRED)

    pybind11_add_module(shr_pybind bindings/shr_pybind.cpp)

    # src/ arrives with shr_core's usage requirements.
    target_link_libraries(shr_pybind
        PRIVATE
            shr_core
    )

    # LTO across the pybind11 extension boundary buys little here and risks clang-cl bitcode mismatches;
    # shr_core itself keeps its optimization settings.
    set_target_properties(shr_pybind PROPERTIES INTERPROCEDURAL_OPTIMIZATION OFF)

    # $<TARGET_FILE:> names the built artifact directly, so this does not depend on which output-directory
    # property governs a MODULE library on this platform.
    add_custom_command(TARGET shr_pybind POST_BUILD
        COMMAND "${CMAKE_COMMAND}"
                -DMODULE_FILE=$<TARGET_FILE:shr_pybind>
                -DMODULE_DIR=${CMAKE_SOURCE_DIR}/build/module
                -DPYTHON_EXECUTABLE=${Python_EXECUTABLE}
                -DBUILD_TYPE=${CMAKE_BUILD_TYPE}
                -DBINARY_DIR=${CMAKE_BINARY_DIR}
                -DCOMPILER=${CMAKE_CXX_COMPILER_ID}
                -DCOMPILER_VERSION=${CMAKE_CXX_COMPILER_VERSION}
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/publish_pybind_module.cmake"
        VERBATIM
        COMMENT "Publishing shr_pybind, its stub, and its provenance to build/module"
    )
endif()
