// ======================================================================
//
// DMOLLMAssistantWindow.h
//
// In-editor GUI panel for the LLM Scene Assistant.
// Provides a natural language input field, command preview/confirmation,
// command history, and MCP server status/controls.
//
// Follows the wi::gui::Window pattern of DMOAssetPlacerWindow et al.
//
// ======================================================================

#pragma once

#include "wiGUI.h"
#include "wiScene.h"
#include "WorldBuilder/DMOLLMSceneService.h"
#include "WorldBuilder/DMOLLMMCPServer.h"

#include <string>
#include <vector>
#include <deque>

class EditorComponent;

class DMOLLMAssistantWindow : public wi::gui::Window
{
public:
    void Create(EditorComponent* editor);
    void ResizeLayout() override;
    void Update(const wi::Canvas& canvas, float dt) override;

    EditorComponent* editor = nullptr;

    // Service and MCP server (owned here, shared with WorldBuilder)
    DMOLLMSceneService sceneService;
    DMOLLMMCPServer    mcpServer;

private:
    // --- UI widgets ---
    wi::gui::TextInputField promptInput;
    wi::gui::Button         executeButton;
    wi::gui::Button         mcpToggleButton;
    wi::gui::Label          mcpStatusLabel;
    wi::gui::Label          historyLabel;
    wi::gui::Label          toolListLabel;

    // --- Command history ---
    struct HistoryEntry
    {
        std::string prompt;
        std::string toolName;
        std::string result;
        bool        success;
    };
    std::deque<HistoryEntry> m_history;
    static constexpr size_t MAX_HISTORY = 50;

    // --- Direct tool execution (no LLM needed) ---
    // Parses JSON tool calls directly from the input field.
    // Format: {"tool": "spawn_entity", "parameters": {...}}
    void ExecuteDirectCommand(const std::string& input);

    // --- History management ---
    void AddHistoryEntry(const std::string& prompt, const std::string& toolName,
                         const LLMCommandResult& result);
    void RefreshHistoryLabel();
    void RefreshToolListLabel();
};

