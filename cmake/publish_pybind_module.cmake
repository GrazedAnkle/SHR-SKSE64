# Publishes a built shr_pybind extension, its type stub, and its provenance into one module directory.
# README.md (Building) owns what that directory means and why the trees stay separate.
#
# Run as a POST_BUILD step via `cmake -P`, not included.
# Required: MODULE_FILE MODULE_DIR PYTHON_EXECUTABLE BUILD_TYPE BINARY_DIR COMPILER COMPILER_VERSION

foreach(required MODULE_FILE MODULE_DIR PYTHON_EXECUTABLE BINARY_DIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "publish_pybind_module.cmake: -D${required} is required")
    endif()
endforeach()

get_filename_component(module_name "${MODULE_FILE}" NAME)

file(MAKE_DIRECTORY "${MODULE_DIR}")

# Drop extensions from an earlier build with a different ABI tag. They cannot shadow the current one - the
# interpreter only accepts its own tag - but they would outlive the stamp below and misdescribe the
# directory.
file(GLOB stale "${MODULE_DIR}/shr_pybind*.pyd" "${MODULE_DIR}/shr_pybind*.pyi")
foreach(path IN LISTS stale)
    get_filename_component(name "${path}" NAME)
    if(NOT name STREQUAL module_name)
        file(REMOVE "${path}")
    endif()
endforeach()

file(COPY "${MODULE_FILE}" DESTINATION "${MODULE_DIR}")

# Best-effort: a missing pybind11-stubgen costs IntelliSense, not a usable module, so it must not fail an
# otherwise successful build.
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env "PYTHONPATH=${MODULE_DIR}"
            "${PYTHON_EXECUTABLE}" -m pybind11_stubgen shr_pybind -o "${MODULE_DIR}" --exit-code
    RESULT_VARIABLE stub_result
    OUTPUT_VARIABLE stub_output
    ERROR_VARIABLE  stub_output
)
if(NOT stub_result EQUAL 0)
    message(WARNING
        "could not generate ${module_name}'s type stub; install pybind11-stubgen (in requirements.txt) "
        "into ${PYTHON_EXECUTABLE} for binding IntelliSense. The compiled module is unaffected.\n"
        "${stub_output}"
    )
endif()

# Read back by tools/core_offline.py. Normalize to forward slashes: a native Windows path would emit
# invalid JSON escapes.
file(TO_CMAKE_PATH "${BINARY_DIR}" binary_dir)
file(TO_CMAKE_PATH "${PYTHON_EXECUTABLE}" python_executable)
string(TIMESTAMP published "%Y-%m-%dT%H:%M:%SZ" UTC)
file(WRITE "${MODULE_DIR}/binding.json"
"{\n"
"  \"module\": \"${module_name}\",\n"
"  \"build_type\": \"${BUILD_TYPE}\",\n"
"  \"binary_dir\": \"${binary_dir}\",\n"
"  \"compiler\": \"${COMPILER} ${COMPILER_VERSION}\",\n"
"  \"python\": \"${python_executable}\",\n"
"  \"published_utc\": \"${published}\"\n"
"}\n"
)
