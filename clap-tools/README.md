# Geraint's CLAP tools

A loose collection of useful things.  Currently Mac-only, but I'll add other OSes as I get round to them.

## CMake

The included `CMakeLists.txt` defines `clap_targets_from_static()`.  This takes a static-library target (with a CLAP interface), and defines other targets for the actual plugins:

```cmake
add_library(staticLibTarget STATIC)
target_sources(staticLibTarget PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/source/foo.cpp
)

clap_targets_from_static(staticLibTarget "com.example.bundleid" "resources/dir/")
```

When built with Emscripten, this builds a WCLAP (including a `.tar.gz` bundle), and also defines the `WCLAP` CMake variable.  Otherwise, this builds VST3 and CLAP plugins (with `.vst3`/`.clap` appended to the bundle ID where needed).

If the resources directory is an empty string (`""`), it's ignored.  Otherwise, everything in that directory is copied appropriately into the plugin bundle.
