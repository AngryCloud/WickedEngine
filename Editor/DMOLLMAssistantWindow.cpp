// ======================================================================
//
// DMOLLMAssistantWindow.cpp
//
// In-editor GUI panel for the LLM Scene Assistant.
//
// ======================================================================

#include "stdafx.h"
#include "DMOLLMAssistantWindow.h"
#include "Editor.h"

using json = nlohmann::json;

// -----------------------------------------------------------------------
// Create
// -----------------------------------------------------------------------

void DMOLLMAssistantWindow::Create(EditorComponent* editorIn)
{
    editor = editorIn;

    wi::gui::Window::Create("LLM Scene Assistant", wi::gui::Window::WindowControls::CLOSE_AND_COLLAPSE);
    SetSize(XMFLOAT2(420, 520));

    // Initialize the scene service
    sceneService.Initialize(editor);

    // Prompt input
    promptInput.Create("Command");
    promptInput.SetDescription("Enter JSON tool call or natural language: ");
    promptInput.SetTooltip(
        "Direct JSON: {\"tool\": \"spawn_entity\", \"parameters\": {\"entity_type\": \"point_light\", \"position\": [0, 5, 0]}}\n"
        "Or just type a tool name: get_scene_info"
    );
    AddWidget(&promptInput);

    // Execute button
    executeButton.Create("Execute");
    executeButton.SetTooltip("Execute the command in the input field");
    executeButton.OnClick([this](wi::gui::EventArgs args) {
        std::string input = promptInput.GetCurrentInputValue();
        if (!input.empty())
        {
            ExecuteDirectCommand(input);
            promptInput.SetValue("");
        }
    });
    AddWidget(&executeButton);

    // MCP server toggle
    mcpToggleButton.Create("Start MCP Server");
    mcpToggleButton.SetTooltip(
        "Start/stop the MCP server on localhost:3100.\n"
        "External LLM tools (Codex, Claude, Kiro) can connect\n"
        "to discover and invoke editor tools via JSON-RPC."
    );
    mcpToggleButton.OnClick([this](wi::gui::EventArgs args) {
        if (mcpServer.IsRunning())
        {
            mcpServer.Stop();
            mcpToggleButton.SetText("Start MCP Server");
            mcpStatusLabel.SetText("MCP: Stopped");
        }
        else
        {
            mcpServer.Initialize(editor, &sceneService);
            mcpServer.Start(3100);
            mcpToggleButton.SetText("Stop MCP Server");
            mcpStatusLabel.SetText("MCP: Running on localhost:3100");
        }
    });
    AddWidget(&mcpToggleButton);

    // MCP status label
    mcpStatusLabel.Create("MCP: Stopped");
    AddWidget(&mcpStatusLabel);

    // Tool list label
    toolListLabel.Create("");
    RefreshToolListLabel();
    AddWidget(&toolListLabel);

    // History label
    historyLabel.Create("No commands executed yet.");
    AddWidget(&historyLabel);

    SetVisible(false);
}

// -----------------------------------------------------------------------
// ResizeLayout
// -----------------------------------------------------------------------

void DMOLLMAssistantWindow::ResizeLayout()
{
    wi::gui::Window::ResizeLayout();

    const float padding = 4.0f;
    const float itemH   = 20.0f;
    const float winW    = GetSize().x;
    const float usableW = winW - padding * 2.0f;

    float y = padding;

    // Prompt input (taller)
    promptInput.SetPos(XMFLOAT2(padding, y));
    promptInput.SetSize(XMFLOAT2(usableW, itemH * 3));
    y += itemH * 3 + padding;

    // Execute button
    executeButton.SetPos(XMFLOAT2(padding, y));
    executeButton.SetSize(XMFLOAT2(usableW * 0.48f, itemH));

    // MCP toggle button
    mcpToggleButton.SetPos(XMFLOAT2(padding + usableW * 0.52f, y));
    mcpToggleButton.SetSize(XMFLOAT2(usableW * 0.48f, itemH));
    y += itemH + padding;

    // MCP status
    mcpStatusLabel.SetPos(XMFLOAT2(padding, y));
    mcpStatusLabel.SetSize(XMFLOAT2(usableW, itemH));
    y += itemH + padding * 2;

    // Tool list
    toolListLabel.SetPos(XMFLOAT2(padding, y));
    toolListLabel.SetSize(XMFLOAT2(usableW, itemH * 8));
    y += itemH * 8 + padding;

    // History
    historyLabel.SetPos(XMFLOAT2(padding, y));
    historyLabel.SetSize(XMFLOAT2(usableW, itemH * 8));
}

// -----------------------------------------------------------------------
// Update
// -----------------------------------------------------------------------

void DMOLLMAssistantWindow::Update(const wi::Canvas& canvas, float dt)
{
    wi::gui::Window::Update(canvas, dt);
}

// -----------------------------------------------------------------------
// ExecuteDirectCommand
// -----------------------------------------------------------------------

void DMOLLMAssistantWindow::ExecuteDirectCommand(const std::string& input)
{
    std::string toolName;
    json params = json::object();

    // Try parsing as JSON first
    try
    {
        json parsed = json::parse(input);
        if (parsed.contains("tool"))
        {
            toolName = parsed["tool"].get<std::string>();
            params = parsed.value("parameters", json::object());
        }
        else if (parsed.contains("name"))
        {
            // MCP-style: {"name": "tool_name", "arguments": {...}}
            toolName = parsed["name"].get<std::string>();
            params = parsed.value("arguments", json::object());
        }
    }
    catch (...)
    {
        // Not JSON — treat as a bare tool name (e.g., "get_scene_info")
        // Strip whitespace
        toolName = input;
        size_t start = toolName.find_first_not_of(" \t\n\r");
        size_t end   = toolName.find_last_not_of(" \t\n\r");
        if (start != std::string::npos)
            toolName = toolName.substr(start, end - start + 1);
    }

    if (toolName.empty())
    {
        wi::backlog::post("[DMO-LLM] Empty command");
        return;
    }

    LLMCommandResult result = sceneService.Execute(toolName, params);
    AddHistoryEntry(input, toolName, result);

    if (result.success)
        wi::backlog::post("[DMO-LLM] " + result.message);
    else
        wi::backlog::post("[DMO-LLM] ERROR: " + result.message);
}

// -----------------------------------------------------------------------
// History management
// -----------------------------------------------------------------------

void DMOLLMAssistantWindow::AddHistoryEntry(const std::string& prompt,
                                             const std::string& toolName,
                                             const LLMCommandResult& result)
{
    HistoryEntry entry;
    entry.prompt   = prompt;
    entry.toolName = toolName;
    entry.result   = result.message;
    entry.success  = result.success;

    m_history.push_front(entry);
    if (m_history.size() > MAX_HISTORY)
        m_history.pop_back();

    RefreshHistoryLabel();
}

void DMOLLMAssistantWindow::RefreshHistoryLabel()
{
    std::string text = "Command History:\n";
    int shown = 0;
    for (auto& entry : m_history)
    {
        if (shown >= 10) break;
        text += (entry.success ? "[OK] " : "[ERR] ");
        text += entry.toolName + ": " + entry.result + "\n";
        ++shown;
    }
    historyLabel.SetText(text);
}

void DMOLLMAssistantWindow::RefreshToolListLabel()
{
    std::string text = "Available Tools:\n";
    auto names = sceneService.GetToolNames();
    for (auto& name : names)
    {
        text += "  " + name + "\n";
    }
    toolListLabel.SetText(text);
}
