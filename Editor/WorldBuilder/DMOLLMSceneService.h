// ======================================================================
//
// DMOLLMSceneService.h
//
// LLM-driven scene manipulation service for the Wicked Engine Editor.
// Provides two access paths:
//
//   1. In-editor GUI panel (WickedLLMAssistantWindow) for local LLM use
//      via Ollama or any OpenAI-compatible endpoint.
//
//   2. MCP (Model Context Protocol) server over stdio so external tools
//      like Codex, Claude, and Kiro can drive the editor directly.
//
// The service layer (DMOLLMSceneService) is shared by both paths.
// It owns the command vocabulary, parameter validation, scene context
// building, undo grouping, and execution dispatch.
//
// ======================================================================

#pragma once

#include "WickedEngine.h"
#include "json.hpp"

#include <functional>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

class EditorComponent;

// -----------------------------------------------------------------------
// Command result returned to callers (GUI panel or MCP)
// -----------------------------------------------------------------------
struct LLMCommandResult
{
    bool        success = false;
    std::string message;
    nlohmann::json data;  // optional structured output (entity IDs, counts, etc.)
};

// -----------------------------------------------------------------------
// Scene context snapshot for LLM system prompts
// -----------------------------------------------------------------------
struct LLMSceneContext
{
    XMFLOAT3    cameraPosition;
    XMFLOAT3    cameraForward;
    int         entityCount;
    int         selectedCount;
    std::string selectedNames;      // comma-separated
    std::string scenePath;
    nlohmann::json toJson() const;
};

// -----------------------------------------------------------------------
// DMOLLMSceneService — stateless command executor
// -----------------------------------------------------------------------
class DMOLLMSceneService
{
public:
    void Initialize(EditorComponent* editor);

    // Build a scene context snapshot for LLM prompting
    LLMSceneContext BuildSceneContext() const;

    // Build the system prompt describing all available tools
    std::string BuildSystemPrompt() const;

    // Build the MCP tools manifest (for initialize response)
    nlohmann::json BuildToolsManifest() const;

    // Execute a named command with JSON parameters
    LLMCommandResult Execute(const std::string& toolName,
                             const nlohmann::json& params);

    // List available tool names
    std::vector<std::string> GetToolNames() const;

    // Get tool schema for a specific tool
    nlohmann::json GetToolSchema(const std::string& toolName) const;

private:
    EditorComponent* m_editor = nullptr;

    // --- Command implementations ---
    LLMCommandResult CmdSpawnEntity(const nlohmann::json& p);
    LLMCommandResult CmdDeleteEntity(const nlohmann::json& p);
    LLMCommandResult CmdMoveEntity(const nlohmann::json& p);
    LLMCommandResult CmdRotateEntity(const nlohmann::json& p);
    LLMCommandResult CmdScaleEntity(const nlohmann::json& p);
    LLMCommandResult CmdDuplicateEntity(const nlohmann::json& p);
    LLMCommandResult CmdSetMaterial(const nlohmann::json& p);
    LLMCommandResult CmdSetLight(const nlohmann::json& p);
    LLMCommandResult CmdSetWeather(const nlohmann::json& p);
    LLMCommandResult CmdGenerateForest(const nlohmann::json& p);
    LLMCommandResult CmdPlaceCave(const nlohmann::json& p);
    LLMCommandResult CmdCreateSpline(const nlohmann::json& p);
    LLMCommandResult CmdApplyLandscape(const nlohmann::json& p);
    LLMCommandResult CmdGetSceneInfo(const nlohmann::json& p);
    LLMCommandResult CmdSelectEntity(const nlohmann::json& p);
    LLMCommandResult CmdSetTransform(const nlohmann::json& p);
    LLMCommandResult CmdSetFog(const nlohmann::json& p);
    LLMCommandResult CmdSetSkyParameters(const nlohmann::json& p);
    LLMCommandResult CmdUndo(const nlohmann::json& p);
    LLMCommandResult CmdRedo(const nlohmann::json& p);

    // Tool registry
    struct ToolDef
    {
        std::string name;
        std::string description;
        nlohmann::json inputSchema;  // JSON Schema for parameters
        std::function<LLMCommandResult(const nlohmann::json&)> handler;
    };
    std::vector<ToolDef> m_tools;

    void RegisterTools();
};

