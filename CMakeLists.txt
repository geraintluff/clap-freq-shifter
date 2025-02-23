cmake_minimum_required(VERSION 3.28)

project(freq-shifter VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17) # for string_view

if (APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET 10.13)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64")
    enable_language(OBJCXX)
endif ()

# These are included as submodules, and have their own CMakeLists.txt
add_subdirectory(modules/clap)
add_subdirectory(modules/clap-helpers)

# cbor-walker - header-only submodule without a CMakeLists.txt
add_library(cbor-walker INTERFACE)
set_target_properties(cbor-walker PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/modules/cbor-walker)

# hilbert-iir
add_library(hilbert-iir INTERFACE)
set_target_properties(hilbert-iir PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/modules/hilbert-iir)

# The actual module
set(NAME "${PROJECT_NAME}")
add_library(${NAME} STATIC)
target_link_libraries(${NAME} PUBLIC
	clap
	clap-helpers
	cbor-walker
	hilbert-iir
)
target_sources(${NAME} PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/source/freq-shifter.cpp
)

# must be after the "clap" target, otherwise the clap-wrapper helper adds it instead
add_subdirectory(clap-tools)

clap_targets_from_static(${NAME} "uk.co.signalsmith.example.freq-shifter" "${CMAKE_CURRENT_SOURCE_DIR}/resources")
