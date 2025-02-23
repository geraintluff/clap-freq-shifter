cmake_minimum_required(VERSION 3.28)

if (NOT DEFINED CMAKE_CXX_STANDARD)
	message(FATAL_ERROR "CMAKE_CXX_STANDARD needs to be set before including clap-tools")
endif()

if ((DEFINED EMSCRIPTEN) OR CMAKE_TOOLCHAIN_FILE MATCHES "/Emscripten\.cmake$" OR (DEFINED EMSCRIPTEN_SYSTEM_PROCESSOR) OR CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
	set(WCLAP 1)
	set(WCLAP 1 PARENT_SCOPE)
	unset(APPLE)
	unset(APPLE PARENT_SCOPE)
endif()

if (APPLE)
    enable_language(OBJC)
endif ()

#### Fetch CLAP wrappers

if (NOT DEFINED WCLAP)
	# The wrapper will download the vst3sdk, unless CMake is called with -DVST3_SDK_ROOT=/path/to/sdk/root
	if (NOT DEFINED VST3_SDK_ROOT)
		set(CLAP_WRAPPER_DOWNLOAD_DEPENDENCIES TRUE)
	endif()

	include(FetchContent)
	FetchContent_Declare(
		clap-wrapper
		GIT_REPOSITORY https://github.com/free-audio/clap-wrapper
		GIT_TAG v0.11.0
		GIT_SHALLOW ON
	)
	set(CLAP_WRAPPER_OUTPUT_NAME clap-wrapper-vst3)
	FetchContent_MakeAvailable(clap-wrapper)
endif()

#### Wraps a CLAP-based static library into various plugin formats

function (clap_targets_from_static NAME BUNDLE_ID RESOURCES_DIR)
	if(NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY)
		set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
	endif()
#		set(STFX_OUTPUT_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugin-$<CONFIG>/${NAME}")
#	elseif(CMAKE_LIBRARY_OUTPUT_DIRECTORY)
#		set(STFX_OUTPUT_DIR "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/plugin-$<CONFIG>/${NAME}")
#	else()
#		set(STFX_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/plugin-$<CONFIG>/${NAME}")
#	endif()
#	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${STFX_OUTPUT_DIR})
#	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${STFX_OUTPUT_DIR})

	if (APPLE)
		target_link_libraries(${NAME} PRIVATE "-framework Cocoa")
		target_link_libraries(${NAME} PRIVATE "-framework WebKit")
	endif()

	if (WCLAP)
		# =========================== WCLAP ===========================
		add_executable(${NAME}.wclap)
		get_target_property(PLUGIN_DIR ${NAME}.wclap LIBRARY_OUTPUT_DIRECTORY)

		set_target_properties(${NAME}.wclap PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PLUGIN_DIR}/${NAME}.wclap/") # the WASM should end up inside the directory, located as if it's a library
		set_target_properties(${NAME}.wclap PROPERTIES OUTPUT_NAME module) # call it module.wasm
		set_target_properties(${NAME}.wclap PROPERTIES SUFFIX ".wasm") # no accompanying JS
		target_link_libraries(${NAME}.wclap PUBLIC ${NAME}) # link to our shared code

		set_target_properties(${NAME}.wclap PROPERTIES COMPILE_FLAGS "-O3 -ffast-math -msimd128")
		set_target_properties(${NAME}.wclap PROPERTIES LINK_FLAGS    "-O3 -ffast-math -msimd128 -sSTANDALONE_WASM --no-entry -s EXPORTED_FUNCTIONS=_clap_entry,_malloc -s INITIAL_MEMORY=512kb -s ALLOW_MEMORY_GROWTH=1 -s ALLOW_TABLE_GROWTH=1 -s PURE_WASI --export-table")

		if (NOT "${RESOURCES_DIR}" STREQUAL "")
			add_custom_command(
				TARGET ${NAME}.wclap POST_BUILD
				COMMAND ${CMAKE_COMMAND} -E copy_directory
					"${RESOURCES_DIR}"
					"${PLUGIN_DIR}/${NAME}.wclap/"
			)
		endif()

		add_custom_command(
			TARGET ${NAME}.wclap POST_BUILD
			WORKING_DIRECTORY "${PLUGIN_DIR}/${NAME}.wclap/"
			COMMAND rm -f "../${NAME}.wclap.tar.gz"
			COMMAND tar --exclude=".*" -vczf "../${NAME}.wclap.tar.gz" *
		)

	else() # not WCLAP

		# =========================== CLAP ===========================

		add_library(${NAME}.clap MODULE)

		target_link_libraries(${NAME}.clap PUBLIC ${NAME}) # link to our shared code (publicly, since we want the CLAP stuff)
		get_target_property(PLUGIN_DIR ${NAME}.clap LIBRARY_OUTPUT_DIRECTORY)

		if (APPLE)
			set_target_properties(${NAME}.clap PROPERTIES
				OUTPUT_NAME ${NAME}
				LIBRARY_OUTPUT_NAME ${NAME}
				BUNDLE True
				BUNDLE_EXTENSION clap
				XCODE_ATTRIBUTE_GENERATE_PKGINFO_FILE YES
				MACOSX_BUNDLE_GUI_IDENTIFIER "${BUNDLE_ID}.clap"
				MACOSX_BUNDLE_BUNDLE_NAME ${NAME}
				MACOSX_BUNDLE_BUNDLE_VERSION "${CMAKE_PROJECT_VERSION}"
				MACOSX_BUNDLE_SHORT_VERSION_STRING "${CMAKE_PROJECT_VERSION}"
#				MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_LIST_DIR}/CLAP_Info.plist.in
			)

			if (NOT "${RESOURCES_DIR}" STREQUAL "")
				add_custom_command(
					TARGET ${NAME}.clap POST_BUILD
					COMMAND ${CMAKE_COMMAND} -E copy_directory
					"${RESOURCES_DIR}"
					"$<TARGET_FILE_DIR:${NAME}.clap>/../Contents/Resources/"
				)
			endif()

		else()
			message(FATAL_ERROR "CLAP is Apple-only for now")
		endif()

		# =========================== VST3 ===========================

		add_library(${NAME}.vst3 MODULE)
		target_add_vst3_wrapper(TARGET ${NAME}.vst3 OUTPUT_NAME ${NAME})
		target_link_libraries(${NAME}.vst3 PRIVATE ${NAME}) # Link to our shared code
		get_target_property(PLUGIN_DIR ${NAME}.clap LIBRARY_OUTPUT_DIRECTORY)

		if(APPLE)
			set_target_properties(${NAME}.vst3 PROPERTIES
				OUTPUT_NAME ${NAME}
				LIBRARY_OUTPUT_NAME ${NAME}
				BUNDLE True
				BUNDLE_EXTENSION vst3
				MACOSX_BUNDLE_GUI_IDENTIFIER "${BUNDLE_ID}.vst3"
				MACOSX_BUNDLE_BUNDLE_NAME ${NAME}
				MACOSX_BUNDLE_BUNDLE_VERSION "${CMAKE_PROJECT_VERSION}"
				MACOSX_BUNDLE_SHORT_VERSION_STRING "${CMAKE_PROJECT_VERSION}"
#				MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_LIST_DIR}/VST3_Info.plist.in
			)

			if (NOT "${RESOURCES_DIR}" STREQUAL "")
				add_custom_command(
					TARGET ${NAME}.vst3 POST_BUILD
					COMMAND ${CMAKE_COMMAND} -E copy_directory
					"${RESOURCES_DIR}"
					"$<TARGET_FILE_DIR:${NAME}.vst3>/../Contents/Resources/"
				)
			endif()
		else()
			message(FATAL_ERROR "VST3 build is Apple-only for now")
		endif()
		
	endif()

endfunction()
