cmake_minimum_required(VERSION 3.19)
project(DmoClientMac)

# MACRENDER-01 — DMO WICKED client macOS window bring-up.
# Links the in-tree WickedEngine target (transitive Metal/SDL2/dxc/LUA deps),
# mirroring the Template_* samples. macOS-only (AppKit + Carbon).

# DMO repo root. The engine (CMAKE_SOURCE_DIR) is repo/external/WickedEngine, so
# the DMO client repo root is two levels up.
get_filename_component(DMO_CLIENT_ROOT "${CMAKE_SOURCE_DIR}/../.." ABSOLUTE)

# WICKED-CLIENT-LAYERS-01: the DMO source lists live in the CLIENT repo, next to
# the code, not here in the engine submodule. Both the macOS and Windows app
# targets include this same file, so a new client module is added ONCE.
include("${DMO_CLIENT_ROOT}/cmake/DmoClientLayers.cmake")

# --- dmo_wicked_ui_min: the minimal substrate+projector subset, compiled against
# the DMO SHIM in its OWN target with its OWN include dirs. This keeps the shim
# TUs isolated from the app's real-engine TUs (no XMFLOAT2 collision) and avoids
# dragging the full dmo_wicked_core (OpenSSL/SQLite/audio/...). The app links this
# and talks to it only through the engine-neutral POD sink interface.
add_library(dmo_wicked_ui_min STATIC ${DMO_CLIENT_UI_MIN_SOURCES})

target_include_directories(dmo_wicked_ui_min PRIVATE
    ${DMO_CLIENT_ROOT}/Source
    ${DMO_CLIENT_ROOT}/Source/Support   # the shim's WickedEngine.h (XMFLOAT2/4)
)
target_compile_definitions(dmo_wicked_ui_min PRIVATE DMO_WICKED_SHIM_INCLUDED=1)
set_target_properties(dmo_wicked_ui_min PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

set(SOURCE_FILES
    main.mm
    ${DMO_CLIENT_RENDER_PATH_SOURCES}
    ${DMO_CLIENT_APP_LAYER_SOURCES}
)


# macOS runtime shader compiler is the arm64 .dylib (the .so is Linux x86-64).
# wiShaderCompiler dlopens "./libdxcompiler.dylib" relative to the run cwd.
set(LIB_DXCOMPILER "libdxcompiler.dylib")

add_executable(DmoClientMac ${SOURCE_FILES})

add_executable(DmoClientMacGodClientPanelProbe
    ${DMO_CLIENT_ROOT}/Tests/WickedGodClientPanelWindowFactorySmoke.cpp
)

# ARC + Obj-C++ for the AppKit window shell.
set_source_files_properties(main.mm PROPERTIES
    COMPILE_FLAGS "-fobjc-arc"
)

target_link_libraries(DmoClientMac PUBLIC
    WickedEngine
    dmo_wicked_ui_min
    "-framework AppKit"
    "-framework Carbon"
)

target_link_libraries(DmoClientMacGodClientPanelProbe PUBLIC
    WickedEngine
)

# Only the ENGINE-NEUTRAL POD headers (WickedUIVisualSink.h / WickedDemoScreen.h)
# are included by the app; NOT Source/Support (shim), so the app's own
# "WickedEngine.h" resolves to the real engine, not the shim.
target_include_directories(DmoClientMac PRIVATE
    ${DMO_CLIENT_ROOT}/Source
    # The shared render path still lives in this directory (see the debt note in
    # cmake/DmoClientLayers.cmake). App-layer sources compiled from Source/Client
    # include "DmoClientRenderPath.h", so the dir must be a real -I, not just the
    # target's own source dir. The Windows target already did this.
    "${DMO_CLIENT_RENDER_PATH_DIR}"
)

target_include_directories(DmoClientMacGodClientPanelProbe PRIVATE
    ${DMO_CLIENT_ROOT}/Source
)

if(WICKED_ENABLE_IPO)
    set_target_properties(DmoClientMac PROPERTIES
        INTERPROCEDURAL_OPTIMIZATION ON
        INTERPROCEDURAL_OPTIMIZATION_DEBUG OFF
    )
endif()

# Bring the runtime shader toolchain dylibs next to the executable so wiLoadLibrary
# (bare-name dlopen) resolves them: the HLSL compiler (dxc) AND Apple's Metal
# Shader Converter (metal_irconverter), which turns DXIL into a valid .metallib.
# Without the converter, Release builds skip conversion (assert compiled out) and
# Metal rejects raw DXIL with "Invalid library file".
set(LIBDXCOMPILER_PATH "${WICKED_ROOT_DIR}/WickedEngine/${LIB_DXCOMPILER}")
set(LIBMETALIR_PATH "${WICKED_ROOT_DIR}/WickedEngine/libmetalirconverter.dylib")
foreach(RUNTIME_DYLIB "${LIBDXCOMPILER_PATH}" "${LIBMETALIR_PATH}")
    if(EXISTS ${RUNTIME_DYLIB})
        add_custom_command(
            TARGET DmoClientMac POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${RUNTIME_DYLIB} ${CMAKE_CURRENT_BINARY_DIR}
        )
    endif()
endforeach()
