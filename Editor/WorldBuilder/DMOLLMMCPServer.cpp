// ======================================================================
//
// DMOLLMMCPServer.cpp
//
// MCP server implementation — TCP listener + JSON-RPC 2.0 handler.
//
// ======================================================================

#include "stdafx.h"
#include "DMOLLMMCPServer.h"
#include "Editor.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef int socklen_t;
#define CLOSE_SOCKET closesocket
#define SOCKET_VALID(s) ((s) != INVALID_SOCKET)
#define SOCKET_INVALID INVALID_SOCKET
typedef SOCKET socket_t;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#define CLOSE_SOCKET close
#define SOCKET_VALID(s) ((s) >= 0)
#define SOCKET_INVALID (-1)
typedef int socket_t;
#endif

using json = nlohmann::json;

// -----------------------------------------------------------------------
// Initialize
// -----------------------------------------------------------------------

void DMOLLMMCPServer::Initialize(EditorComponent* editor, DMOLLMSceneService* service)
{
    m_editor  = editor;
    m_service = service;
}

// -----------------------------------------------------------------------
// Start / Stop
// -----------------------------------------------------------------------

void DMOLLMMCPServer::Start(int port)
{
    if (m_running.load()) return;

    m_port = port;

#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    m_listenSocket = static_cast<int>(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    if (!SOCKET_VALID(static_cast<socket_t>(m_listenSocket)))
    {
        wi::backlog::post("[DMO-LLM-MCP] Failed to create socket");
        return;
    }

    // Allow port reuse
    int opt = 1;
    setsockopt(static_cast<socket_t>(m_listenSocket), SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&opt), sizeof(opt));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // localhost only
    addr.sin_port = htons(static_cast<uint16_t>(port));

    if (bind(static_cast<socket_t>(m_listenSocket),
             reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0)
    {
        wi::backlog::post("[DMO-LLM-MCP] Failed to bind port " + std::to_string(port));
        CLOSE_SOCKET(static_cast<socket_t>(m_listenSocket));
        m_listenSocket = static_cast<int>(SOCKET_INVALID);
        return;
    }

    if (listen(static_cast<socket_t>(m_listenSocket), 4) != 0)
    {
        wi::backlog::post("[DMO-LLM-MCP] Failed to listen");
        CLOSE_SOCKET(static_cast<socket_t>(m_listenSocket));
        m_listenSocket = static_cast<int>(SOCKET_INVALID);
        return;
    }

    m_running.store(true);
    m_listenThread = std::thread(&DMOLLMMCPServer::ListenLoop, this);

    wi::backlog::post("[DMO-LLM-MCP] Server started on localhost:" + std::to_string(port));
}

void DMOLLMMCPServer::Stop()
{
    m_running.store(false);

    if (SOCKET_VALID(static_cast<socket_t>(m_listenSocket)))
    {
        CLOSE_SOCKET(static_cast<socket_t>(m_listenSocket));
        m_listenSocket = static_cast<int>(SOCKET_INVALID);
    }

    if (m_listenThread.joinable())
        m_listenThread.join();

    wi::backlog::post("[DMO-LLM-MCP] Server stopped");

#ifdef _WIN32
    WSACleanup();
#endif
}

// -----------------------------------------------------------------------
// Listen loop (runs on background thread)
// -----------------------------------------------------------------------

void DMOLLMMCPServer::ListenLoop()
{
    while (m_running.load())
    {
        struct sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);

        // Use select() with timeout so we can check m_running periodically
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(static_cast<socket_t>(m_listenSocket), &readSet);

        struct timeval timeout{};
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int sel = select(m_listenSocket + 1, &readSet, nullptr, nullptr, &timeout);
        if (sel <= 0) continue;

        socket_t clientSocket = accept(static_cast<socket_t>(m_listenSocket),
                                       reinterpret_cast<struct sockaddr*>(&clientAddr),
                                       &clientLen);
        if (!SOCKET_VALID(clientSocket)) continue;

        // Handle client in a detached thread (simple model for editor use)
        std::thread clientThread([this, clientSocket]() {
            HandleClient(static_cast<int>(clientSocket));
        });
        clientThread.detach();
    }
}

// -----------------------------------------------------------------------
// Handle a single client connection
// -----------------------------------------------------------------------

void DMOLLMMCPServer::HandleClient(int clientSocket)
{
    socket_t sock = static_cast<socket_t>(clientSocket);
    std::string buffer;
    char chunk[4096];

    while (m_running.load())
    {
        int bytesRead = recv(sock, chunk, sizeof(chunk) - 1, 0);
        if (bytesRead <= 0) break;

        chunk[bytesRead] = '\0';
        buffer += chunk;

        // Process complete lines (newline-delimited JSON)
        size_t pos;
        while ((pos = buffer.find('\n')) != std::string::npos)
        {
            std::string line = buffer.substr(0, pos);
            buffer.erase(0, pos + 1);

            if (line.empty()) continue;

            std::string response = HandleRequest(line);
            response += "\n";

            send(sock, response.c_str(), static_cast<int>(response.size()), 0);
        }
    }

    CLOSE_SOCKET(sock);
}

// -----------------------------------------------------------------------
// HandleRequest — main JSON-RPC dispatcher
// -----------------------------------------------------------------------

std::string DMOLLMMCPServer::HandleRequest(const std::string& jsonRequest)
{
    json request;
    try
    {
        request = json::parse(jsonRequest);
    }
    catch (...)
    {
        return MakeErrorResponse(nullptr, -32700, "Parse error");
    }

    std::string method = request.value("method", "");
    json id = request.contains("id") ? request["id"] : json(nullptr);

    if (method == "initialize")
        return HandleInitialize(request);
    else if (method == "tools/list")
        return HandleToolsList(request);
    else if (method == "tools/call")
        return HandleToolsCall(request);
    else if (method == "ping")
        return HandlePing(request);
    else
        return MakeErrorResponse(id, -32601, "Method not found: " + method);
}

// -----------------------------------------------------------------------
// MCP: initialize
// -----------------------------------------------------------------------

std::string DMOLLMMCPServer::HandleInitialize(const json& request)
{
    json id = request.value("id", json(nullptr));

    json result = {
        {"protocolVersion", "2024-11-05"},
        {"capabilities", {
            {"tools", {{"listChanged", false}}}
        }},
        {"serverInfo", {
            {"name", "dmo-wicked-editor"},
            {"version", "1.0.0"}
        }}
    };

    return MakeSuccessResponse(id, result);
}

// -----------------------------------------------------------------------
// MCP: tools/list
// -----------------------------------------------------------------------

std::string DMOLLMMCPServer::HandleToolsList(const json& request)
{
    json id = request.value("id", json(nullptr));

    json tools = json::array();
    if (m_service)
    {
        tools = m_service->BuildToolsManifest();
    }

    json result = {{"tools", tools}};
    return MakeSuccessResponse(id, result);
}

// -----------------------------------------------------------------------
// MCP: tools/call
// -----------------------------------------------------------------------

std::string DMOLLMMCPServer::HandleToolsCall(const json& request)
{
    json id = request.value("id", json(nullptr));
    json params = request.value("params", json::object());

    std::string toolName = params.value("name", "");
    json arguments = params.value("arguments", json::object());

    if (toolName.empty())
        return MakeErrorResponse(id, -32602, "Missing tool name in params.name");

    if (!m_service)
        return MakeErrorResponse(id, -32603, "Scene service not initialized");

    // Execute on the main thread context
    // NOTE: In a production build, this should be queued to the main thread
    // via a thread-safe command queue. For the editor prototype, we execute
    // directly since the editor is single-user and commands are serialized.
    LLMCommandResult cmdResult = m_service->Execute(toolName, arguments);

    json content = json::array();
    content.push_back({
        {"type", "text"},
        {"text", cmdResult.message}
    });

    if (!cmdResult.data.is_null() && !cmdResult.data.empty())
    {
        content.push_back({
            {"type", "text"},
            {"text", cmdResult.data.dump(2)}
        });
    }

    json result = {
        {"content", content},
        {"isError", !cmdResult.success}
    };

    return MakeSuccessResponse(id, result);
}

// -----------------------------------------------------------------------
// MCP: ping
// -----------------------------------------------------------------------

std::string DMOLLMMCPServer::HandlePing(const json& request)
{
    json id = request.value("id", json(nullptr));
    return MakeSuccessResponse(id, json::object());
}

// -----------------------------------------------------------------------
// JSON-RPC response builders
// -----------------------------------------------------------------------

std::string DMOLLMMCPServer::MakeErrorResponse(const json& id, int code, const std::string& message)
{
    json response = {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"error", {
            {"code", code},
            {"message", message}
        }}
    };
    return response.dump();
}

std::string DMOLLMMCPServer::MakeSuccessResponse(const json& id, const json& result)
{
    json response = {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"result", result}
    };
    return response.dump();
}
