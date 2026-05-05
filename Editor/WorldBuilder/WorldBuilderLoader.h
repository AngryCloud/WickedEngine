#pragma once
#include "WickedEngine.h"

// Includes all WorldBuilder subsystems
#include "WickedWorldEditorExtension.h"
#include "WickedWorldBuilderMasterTool.h"
#include "WickedAutoLandscapeTool.h"
#include "WickedCavePCGTool.h"
#include "WickedModularCaveTool.h"
#include "WickedForestPCGTool.h"
#include "WickedDitchStreamTool.h"
#include "WickedSplineTool.h"
#include "DMOLLMSceneService.h"
#include "DMOLLMMCPServer.h"

class EditorComponent;

// Container structure for hooking into EditorComponent seamlessly
struct WorldBuilderExtensions
{
    WickedWorldEditorHubWindow  hubWindow;
    WickedWorldBuilderMasterWindow masterWindow;
    WickedAutoLandscapeWindow   autoLandscapeWindow;
    WickedCavePCGWindow         cavePCGWindow;
    WickedModularCaveWindow     modularCaveWindow;
    WickedForestPCGWindow       forestPCGWindow;
    WickedDitchStreamWindow     ditchStreamWindow;
    WickedSplineWindow          splineWindow;

    void Initialize(EditorComponent* editor)
    {
        hubWindow.Create(editor);
        masterWindow.Create(editor);
        autoLandscapeWindow.Create(editor);
        cavePCGWindow.Create(editor);
        modularCaveWindow.Create(editor);
        forestPCGWindow.Create(editor);
        ditchStreamWindow.Create(editor);
        splineWindow.Create(editor);

        // Bind main hub toggles
        hubWindow.openMasterControlsButton.OnClick([this](wi::gui::EventArgs) {
            masterWindow.SetVisible(!masterWindow.IsVisible());
        });
        
        hubWindow.openLandscapeButton.OnClick([this](wi::gui::EventArgs) {
            autoLandscapeWindow.SetVisible(!autoLandscapeWindow.IsVisible());
        });
        
        hubWindow.openCaveToolButton.OnClick([this](wi::gui::EventArgs) {
            cavePCGWindow.SetVisible(!cavePCGWindow.IsVisible());
        });
        
        hubWindow.openForestToolButton.OnClick([this](wi::gui::EventArgs) {
            forestPCGWindow.SetVisible(!forestPCGWindow.IsVisible());
        });
        
        hubWindow.openDitchStreamButton.OnClick([this](wi::gui::EventArgs) {
            ditchStreamWindow.SetVisible(!ditchStreamWindow.IsVisible());
        });
        
        hubWindow.openSplineToolButton.OnClick([this](wi::gui::EventArgs) {
            splineWindow.SetVisible(!splineWindow.IsVisible());
        });
    }

    void Update(const wi::Canvas& canvas, float dt)
    {
        hubWindow.Update(canvas, dt);
        masterWindow.Update(canvas, dt);
        autoLandscapeWindow.Update(canvas, dt);
        cavePCGWindow.Update(canvas, dt);
        modularCaveWindow.Update(canvas, dt);
        forestPCGWindow.Update(canvas, dt);
        ditchStreamWindow.Update(canvas, dt);
        splineWindow.Update(canvas, dt);
    }
};
