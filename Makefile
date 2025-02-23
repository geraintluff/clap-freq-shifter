all: clap vst3 wclap

CMAKE_PARAMS := -DCMAKE_BUILD_TYPE=Release -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=.. -DCMAKE_LIBRARY_OUTPUT_DIRECTORY=..

out/build: Makefile
	cmake -B out/build $(CMAKE_PARAMS)

clap: out/build
	cmake --build out/build --target freq-shifter.clap --config Release

vst3: out/build
	cmake --build out/build --target freq-shifter.vst3 --config Release

wclap: emsdk
	@# based on https://stunlock.gg/posts/emscripten_with_cmake/
	@$(EMSDK_ENV) emcmake cmake . -B out/build-emscripten $(CMAKE_PARAMS)
	@$(EMSDK_ENV) cmake --build out/build-emscripten --target freq-shifter.wclap --config Release

clean:
	rm -rf out

#----- Emscripten SDK setup -----#

CURRENT_DIR := $(shell pwd)
EMSDK ?= $(CURRENT_DIR)/emsdk
EMSDK_ENV = EMSDK_QUIET=1 . "$(EMSDK)/emsdk_env.sh";

emsdk:
	@ if ! test -d "$(EMSDK)" ;\
	then \
		echo "SDK not found - cloning from Github" ;\
		git clone https://github.com/emscripten-core/emsdk.git "$(EMSDK)" ;\
		cd "$(EMSDK)" && git pull && ./emsdk install latest && ./emsdk activate latest ;\
		$(EMSDK_ENV) emcc --check && python3 --version && cmake --version ;\
	fi