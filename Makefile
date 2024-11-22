wclap: emsdk
	@# based on https://stunlock.gg/posts/emscripten_with_cmake/
	@$(EMSDK_ENV) emcmake cmake . -B build-emscripten
	@$(EMSDK_ENV) cmake --build build-emscripten --target freq-shifter --config Release --verbose

	cd build-emscripten/artefacts/freq-shifter/freq-shifter.wclap; tar --exclude=".*" -vczf ../freq-shifter.wclap.tar.gz *

# ------
# Emscripten SDK setup

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