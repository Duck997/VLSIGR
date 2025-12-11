cmake_minimum_required(VERSION 3.18)
project(vlsigr_python LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(pybind11 CONFIG REQUIRED)

set(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")

set(VLSIGR_SOURCES
    "${REPO_ROOT}/src/router/cost_model.cpp"
    "${REPO_ROOT}/src/router/hum.cpp"
    "${REPO_ROOT}/src/router/ispd_data.cpp"
    "${REPO_ROOT}/src/router/layer_assignment.cpp"
    "${REPO_ROOT}/src/router/patterns.cpp"
    "${REPO_ROOT}/src/router/routing_core.cpp"
    "${REPO_ROOT}/src/router/utils.cpp"
    "${REPO_ROOT}/src/api/vlsigr.cpp"
    "${REPO_ROOT}/src/tools/draw_lib.cpp"
    "${REPO_ROOT}/third_party/LayerAssignment.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/bindings.cpp"
)

pybind11_add_module(vlsigr ${VLSIGR_SOURCES})
target_include_directories(vlsigr PRIVATE
    "${REPO_ROOT}/src"
    "${REPO_ROOT}/third_party"
)
target_link_libraries(vlsigr PRIVATE pthread)

# Install the extension into the Python package directory: vlsigr/vlsigr*.so
install(TARGETS vlsigr
        LIBRARY DESTINATION vlsigr
        ARCHIVE DESTINATION vlsigr
        RUNTIME DESTINATION vlsigr)


