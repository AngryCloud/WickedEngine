// ======================================================================
//
// DMOLLMMCPServer.h
//
// MCP (Model Context Protocol) server for the Wicked Engine Editor.
// Listens on a local TCP port and speaks JSON-RPC 2.0 over newline-
// delimited JSON, following the MCP specification.
//
// This allows external LLM tools (Codex, Claude, Kiro, etc.) to
// discover and invoke editor scene manipulation tools.
//
// Protocol: JSON-RPC 2.0 over TCP (one JSON object per line)
// Default port: 3100 (configurable)
//
// MCP lifecycle:
//   1. Client connects and sends "initialize" request
//   2. Server responds with capabilities and tool list
//   3. Client sends "tools/call" requests to invoke tools
//   4. Server executes and returns results
//
// ======================================================================

#pragma once

#include "DMOLLMSceneService.h"
#include <atomic>
#include <string>
#include <thread>
#include <functional>

class EditorComponent;

class DMOLLMMCPServer
{
public:
    void Initialize(EditorComponent* editor, DMOLLMSceneService* service);
    void Start(int port = 3100);
    void Stop();
    bool IsRunning() const { return m_running.load(); }
    int  GetPort() const { return m_port; }

    // Process a single JSON-RPC request and return the response.
    // This is also used by the in-editor panel for local tool calls.
    std::string HandleRequest(const std::string& jsonRequest);

private:
    EditorComponent*    m_editor  = nullptr;
    DMOLLMSceneService* m_service = nullptr;
    std::atomic<bool>   m_running{false};
    int                 m_port = 3100;
    std::thread         m_listenThread;

    // Platform socket handle
    int m_listenSocket = -1;

    void ListenLoop();
    void HandleClient(int clientSocket);

    // JSON-RPC handlers
    std::string HandleInitialize(const nlohmann::json& request);
    std::string HandleToolsList(const nlohmann::json& request);
    std::string HandleToolsCall(const nlohmann::json& request);
    std::string HandlePing(const nlohmann::json& request);

    // JSON-RPC error response builder
    std::string MakeErrorResponse(const nlohmann::json& id, int code, const std::string& message);
    std::string MakeSuccessResponse(const nlohmann::json& id, const nlohmann::json& result);
};

