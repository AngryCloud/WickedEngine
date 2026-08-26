cmake_minimum_required(VERSION 3.19)
project(DmoClientMac)

# MACRENDER-01 — DMO WICKED client macOS window bring-up.
# Links the in-tree WickedEngine target (transitive Metal/SDL2/dxc/LUA deps),
# mirroring the Template_* samples. macOS-only (AppKit + Carbon).

# DMO repo root. The engine (CMAKE_SOURCE_DIR) is repo/external/WickedEngine, so
# the DMO client repo root is two levels up.
get_filename_component(DMO_CLIENT_ROOT "${CMAKE_SOURCE_DIR}/../.." ABSOLUTE)

# --- dmo_wicked_ui_min: the minimal substrate+projector subset, compiled against
# the DMO SHIM in its OWN target with its OWN include dirs. This keeps the shim
# TUs isolated from the app's real-engine TUs (no XMFLOAT2 collision) and avoids
# dragging the full dmo_wicked_core (OpenSSL/SQLite/audio/...). The app links this
# and talks to it only through the engine-neutral POD sink interface.
add_library(dmo_wicked_ui_min STATIC
    ${DMO_CLIENT_ROOT}/Source/UI/Substrate/WickedUIWidget.cpp
    ${DMO_CLIENT_ROOT}/Source/UI/Substrate/WickedUIAnimation.cpp
    ${DMO_CLIENT_ROOT}/Source/UI/Substrate/WickedUIInputFocus.cpp
    ${DMO_CLIENT_ROOT}/Source/UI/Substrate/WickedUIScreenRegistry.cpp
    ${DMO_CLIENT_ROOT}/Source/UI/Substrate/WickedUIScreenHost.cpp
    ${DMO_CLIENT_ROOT}/Source/UI/Controllers/WickedUIScreenController.cpp
    ${DMO_CLIENT_ROOT}/Source/UI/Controllers/WickedFrontDoorControllers.cpp
    ${DMO_CLIENT_ROOT}/Source/UI/Localization/WickedLocalizationRuntime.cpp
    ${DMO_CLIENT_ROOT}/Source/UI/Localization/WickedUiPreferenceStore.cpp
    ${DMO_CLIENT_ROOT}/Source/UI/Theme/WickedUIThemePack.cpp
    ${DMO_CLIENT_ROOT}/Source/UI/Theme/WickedUIThemeSystem.cpp
    ${DMO_CLIENT_ROOT}/Source/UI/Projection/WickedGuiProjector.cpp
    ${DMO_CLIENT_ROOT}/Source/UI/Projection/WickedDemoScreen.cpp
    ${DMO_CLIENT_ROOT}/Source/UI/Screens/WickedAuthScreenController.cpp
    ${DMO_CLIENT_ROOT}/Source/UI/Screens/WickedCharacterSelectController.cpp
    ${DMO_CLIENT_ROOT}/Source/UI/Screens/WickedCharacterCreateController.cpp
    ${DMO_CLIENT_ROOT}/Source/UI/Screens/WickedLoadingController.cpp
    ${DMO_CLIENT_ROOT}/Source/UI/Screens/WickedWorldBackdropController.cpp
    ${DMO_CLIENT_ROOT}/Source/UI/Screens/WickedHudController.cpp
    ${DMO_CLIENT_ROOT}/Source/UI/Screens/WickedMenuWindowController.cpp
    ${DMO_CLIENT_ROOT}/Source/UI/Screens/WickedInventoryWindowController.cpp
    ${DMO_CLIENT_ROOT}/Source/UI/Screens/WickedJournalWindowController.cpp
    ${DMO_CLIENT_ROOT}/Source/UI/Screens/WickedCharacterSheetController.cpp
    ${DMO_CLIENT_ROOT}/Source/UI/Screens/WickedConversationAmbientBarkController.cpp
    ${DMO_CLIENT_ROOT}/Source/UI/Screens/WickedConversationPanelController.cpp
    ${DMO_CLIENT_ROOT}/Source/UI/Screens/WickedGodClientShellController.cpp
    ${DMO_CLIENT_ROOT}/Source/UI/Screens/WickedChatWindowController.cpp
    ${DMO_CLIENT_ROOT}/Source/UI/Screens/WickedServicePointController.cpp
    ${DMO_CLIENT_ROOT}/Source/UI/Screens/WickedSettingsWindowController.cpp
    ${DMO_CLIENT_ROOT}/Source/Stats/WickedStatReadModel.cpp
    ${DMO_CLIENT_ROOT}/Source/Stats/WickedStatServerSnapshotAdapter.cpp
    ${DMO_CLIENT_ROOT}/Source/Stats/WickedCharacterCreationPresenter.cpp
    ${DMO_CLIENT_ROOT}/Source/CharacterCreation/WickedRaceClassCatalog.cpp
    ${DMO_CLIENT_ROOT}/Source/CharacterCreation/WickedCharacterNameRules.cpp
    ${DMO_CLIENT_ROOT}/Source/CharacterCreation/WickedAppearanceCustomizationCatalog.cpp
    ${DMO_CLIENT_ROOT}/Source/CharacterCreation/WickedMorphSession.cpp
    ${DMO_CLIENT_ROOT}/Source/CharacterCreation/WickedAppearancePreviewAssembler.cpp
    ${DMO_CLIENT_ROOT}/Source/CharacterCreation/WickedCreationPreviewService.cpp
    ${DMO_CLIENT_ROOT}/Source/CharacterCreation/WickedCreationPreviewMeshBinder.cpp
    ${DMO_CLIENT_ROOT}/Source/CharacterCreation/WickedCreationPreviewGpuAssembler.cpp
    ${DMO_CLIENT_ROOT}/Source/CharacterCreation/WickedAppearanceConfirmCodec.cpp
    ${DMO_CLIENT_ROOT}/Source/CharacterCreation/WickedCharacterCreateRuntime.cpp
    ${DMO_CLIENT_ROOT}/Source/Gear/WickedGearTypes.cpp
    ${DMO_CLIENT_ROOT}/Source/Gear/WickedGearEquipRules.cpp
    ${DMO_CLIENT_ROOT}/Source/Inventory/WickedInventoryFootprintCatalog.cpp
    ${DMO_CLIENT_ROOT}/Source/Inventory/WickedInventoryGridReadModel.cpp
    ${DMO_CLIENT_ROOT}/Source/Inventory/WickedInventoryGridIntentClient.cpp
    ${DMO_CLIENT_ROOT}/Source/Inventory/WickedInventoryIconBinding.cpp
    ${DMO_CLIENT_ROOT}/Source/Inventory/WickedInventoryPanelRuntime.cpp
    ${DMO_CLIENT_ROOT}/Source/Inventory/WickedInventoryRuntimeServices.cpp
    ${DMO_CLIENT_ROOT}/Source/Inventory/WickedInventoryViewModel.cpp
    ${DMO_CLIENT_ROOT}/Source/Inventory/WickedInventoryWorldDropIntentClient.cpp
    ${DMO_CLIENT_ROOT}/Source/Inventory/WickedItemIconCache.cpp
    ${DMO_CLIENT_ROOT}/Source/Inventory/WickedItemFramingCatalog.cpp
    ${DMO_CLIENT_ROOT}/Source/Inventory/WickedItemTooltipModel.cpp
    ${DMO_CLIENT_ROOT}/Source/Inventory/WickedItemTooltipRecordAdapter.cpp
    ${DMO_CLIENT_ROOT}/Source/Inventory/WickedIconCacheSidecar.cpp
    # Right-click context menu model + its split-policy dependency (Phase E).
    ${DMO_CLIENT_ROOT}/Source/Inventory/WickedInventoryActionPolicy.cpp
    ${DMO_CLIENT_ROOT}/Source/Inventory/WickedInventoryContextMenu.cpp
    # Client read-model store — the seam a server-snapshot transport writes to.
    ${DMO_CLIENT_ROOT}/Source/UI/ReadModel/WickedClientReadModelStore.cpp
    # Demo snapshot feed — stands in for the live transport, drives the store off dt.
    ${DMO_CLIENT_ROOT}/Source/UI/ReadModel/WickedDemoSnapshotFeed.cpp
    # Integrated pre-existing portable Journal presenter layer (Source/Presenters).
    ${DMO_CLIENT_ROOT}/Source/Presenters/WickedJournalBookShell.cpp
    ${DMO_CLIENT_ROOT}/Source/Presenters/WickedJournalTabPresenter.cpp
    ${DMO_CLIENT_ROOT}/Source/Presenters/WickedLedgerPresenter.cpp
    ${DMO_CLIENT_ROOT}/Source/UI/Substrate/WickedUISubstrate.cpp
    ${DMO_CLIENT_ROOT}/Source/UI/WickedFrontDoorAuthScreen.cpp
    ${DMO_CLIENT_ROOT}/Source/UI/WickedFrontDoorAvatarSelectionScreen.cpp
    ${DMO_CLIENT_ROOT}/Source/UI/WickedFrontDoorEnterWorldScreen.cpp
    ${DMO_CLIENT_ROOT}/Source/UI/Projection/WickedFrontDoorHost.cpp
    ${DMO_CLIENT_ROOT}/Source/Foundation/WickedEventKernel.cpp
    ${DMO_CLIENT_ROOT}/Source/Chat/WickedChatRuntime.cpp
    ${DMO_CLIENT_ROOT}/Source/Application/WickedClientServiceDirectory.cpp
    ${DMO_CLIENT_ROOT}/Source/Application/WickedAdmittedSessionRuntime.cpp
    ${DMO_CLIENT_ROOT}/Source/Application/WickedAuthoritativeSpawnSnapshot.cpp
    ${DMO_CLIENT_ROOT}/Source/Application/WickedPlayerEntrySubstrate.cpp
    ${DMO_CLIENT_ROOT}/Source/Application/WickedPlayerLoopEdgeWireCodec.cpp
    ${DMO_CLIENT_ROOT}/Source/Application/WickedFrontDoorService.cpp
    ${DMO_CLIENT_ROOT}/Source/Application/WickedStartupFlowShell.cpp
    ${DMO_CLIENT_ROOT}/Source/Application/WickedFrontDoorAuthUiService.cpp
    ${DMO_CLIENT_ROOT}/Source/Application/WickedAccountExchangeProvider.cpp
    ${DMO_CLIENT_ROOT}/Source/Application/WickedGameplayUiInteractionRuntime.cpp
    ${DMO_CLIENT_ROOT}/Source/Application/WickedGameplayUiProjector.cpp
    ${DMO_CLIENT_ROOT}/Source/Application/WickedInventorySessionRuntime.cpp
    ${DMO_CLIENT_ROOT}/Source/Application/WickedUiRuntimeService.cpp
    ${DMO_CLIENT_ROOT}/Source/Application/WickedUiPrototypeHarness.cpp
    ${DMO_CLIENT_ROOT}/Source/Application/WickedSessionTargetingRuntime.cpp
    ${DMO_CLIENT_ROOT}/Source/Weather/WickedWeatherPacketIngress.cpp
    ${DMO_CLIENT_ROOT}/Source/Weather/WickedWeatherRuntimeHub.cpp
    ${DMO_CLIENT_ROOT}/Source/UI/Debug/WickedFrontDoorDebugPanel.cpp
    # WICKED-MOVE-06A/06B2: input action map + locomotion policy. Engine-free by
    # construction (std headers only), so they compile cleanly on the shim side
    # and the real-engine app reaches them through POD seams -- exactly the
    # boundary this target exists to keep.
    ${DMO_CLIENT_ROOT}/Source/Input/WickedInputActionMap.cpp
    ${DMO_CLIENT_ROOT}/Source/Input/WickedInputDefaultBindings.cpp
    ${DMO_CLIENT_ROOT}/Source/Locomotion/WickedCharacterPhysicsController.cpp
    ${DMO_CLIENT_ROOT}/Source/Locomotion/WickedLocomotionController.cpp
    ${DMO_CLIENT_ROOT}/Source/Locomotion/WickedFollowCamera.cpp
    # MOVE-06E: the traversal lane, now with a caller.
    ${DMO_CLIENT_ROOT}/Source/Traversal/WickedTraversalTypes.cpp
    ${DMO_CLIENT_ROOT}/Source/Traversal/WickedTraversalLockSeam.cpp
    ${DMO_CLIENT_ROOT}/Source/Traversal/WickedTraversalProfileCatalog.cpp
    ${DMO_CLIENT_ROOT}/Source/Traversal/WickedTraversalStateMachine.cpp
    ${DMO_CLIENT_ROOT}/Source/Traversal/WickedTraversalAffordanceQueryService.cpp
    ${DMO_CLIENT_ROOT}/Source/Traversal/WickedTraversalAnimationPresenter.cpp
    ${DMO_CLIENT_ROOT}/Source/Traversal/WickedTraversalIKPresenter.cpp
    ${DMO_CLIENT_ROOT}/Source/Traversal/WickedTraversalPredictionClient.cpp
    ${DMO_CLIENT_ROOT}/Source/Traversal/WickedTraversalIntegrationRuntime.cpp
    ${DMO_CLIENT_ROOT}/Source/Traversal/WickedTraversalLocomotionBridge.cpp
)
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
    DmoClientRenderPath.cpp
    # WICKED-MOVE-06B2: the REAL wi::physics character adapter. Compiled HERE,
    # in the app target, because this target's include path omits
    # Source/Support -- so its `WickedEngine.h` is the real engine, not the
    # shim. Compiling it on the shim side (as MOVE-06B first did) produced code
    # that called stubs and could never move a character.
    ${DMO_CLIENT_ROOT}/Source/Locomotion/WickedEngineCharacterPhysicsBackend.cpp
    # WICKED-MOVE-06C: the composition that REFERENCES the engine-free modules,
    # so the linker actually pulls them out of libdmo_wicked_ui_min.a. Without a
    # referencing TU they sit in the archive and never reach the binary.
    ${DMO_CLIENT_ROOT}/Source/Collision/WickedCollisionLayerPolicy.cpp
    ${DMO_CLIENT_ROOT}/Source/Collision/WickedEngineCollisionQuery.cpp
    ${DMO_CLIENT_ROOT}/Source/Locomotion/WickedZoneWalkComposition.cpp
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
