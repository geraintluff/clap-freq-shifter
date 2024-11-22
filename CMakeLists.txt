cmake_minimum_required(VERSION 3.28)

project(freq-shifter VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 11)
set(OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/artefacts/${PROJECT_NAME}")

# CLAP (headers as empty "library")
add_library(clap INTERFACE)
set_target_properties(clap PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/modules/clap/include)

# CLAP helpers
add_library(clap-helpers INTERFACE)
set_target_properties(clap-helpers PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/modules/clap-helpers/include)

# cbor-walker
add_library(cbor-walker INTERFACE)
set_target_properties(cbor-walker PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/modules/cbor-walker)

# hilbert-iir
add_library(hilbert-iir INTERFACE)
set_target_properties(hilbert-iir PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/modules/hilbert-iir)

# The actual module
add_executable(${PROJECT_NAME})
target_link_libraries(${PROJECT_NAME} PUBLIC
	clap
	clap-helpers
	cbor-walker
	hilbert-iir
)
target_sources(${PROJECT_NAME} PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/source/freq-shifter.cpp
)

# Emscripten flags
set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_FLAGS "-O3 -ffast-math -fno-exceptions -fno-rtti")
set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS    "-O3 -ffast-math -fno-exceptions -fno-rtti -sSTANDALONE_WASM --no-entry -sEXPORTED_FUNCTIONS=_clap_entry,_malloc -s INITIAL_MEMORY=512kb -sALLOW_MEMORY_GROWTH -sALLOW_TABLE_GROWTH -sPURE_WASI --export-table")

set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}/${PROJECT_NAME}.wclap/")
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME module) # call it module.wasm
set(CMAKE_EXECUTABLE_SUFFIX ".wasm") # No accompanying JS

# Find everything in resources/, except dot-files
file(GLOB PROJECT_RESOURCES "${CMAKE_CURRENT_SOURCE_DIR}/resources/*")
list(FILTER PROJECT_RESOURCES EXCLUDE REGEX "^\\..+")
# Copy them all into the .wclap/
file(COPY ${PROJECT_RESOURCES} DESTINATION ${OUTPUT_DIR}/${PROJECT_NAME}.wclap/)
	
