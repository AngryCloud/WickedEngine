// ======================================================================
//
// DMOLLMSceneService.cpp
//
// LLM scene manipulation service — command registry and execution.
//
// ======================================================================

#include "stdafx.h"
#include "DMOLLMSceneService.h"
#include "Editor.h"

// WorldBuilder tools
#include "WickedForestPCGTool.h"
#include "WickedModularCaveTool.h"
#include "WickedDitchStreamTool.h"
#include "WickedAutoLandscapeTool.h"
#include "WickedSplineTool.h"

using json = nlohmann::json;

// -----------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------

static XMFLOAT3 JsonToFloat3(const json& j, const std::string& key,
                              XMFLOAT3 fallback = {0,0,0})
{
    if (!j.contains(key) || !j[key].is_array() || j[key].size() < 3)
        return fallback;
    return XMFLOAT3(j[key][0].get<float>(),
                     j[key][1].get<float>(),
                     j[key][2].get<float>());
}

static json Float3ToJson(const XMFLOAT3& v)
{
    return json::array({v.x, v.y, v.z});
}

// -----------------------------------------------------------------------
// LLMSceneContext
// -----------------------------------------------------------------------

json LLMSceneContext::toJson() const
{
    return {
        {"camera_position", json::array({cameraPosition.x, cameraPosition.y, cameraPosition.z})},
        {"camera_forward",  json::array({cameraForward.x, cameraForward.y, cameraForward.z})},
        {"entity_count",    entityCount},
        {"selected_count",  selectedCount},
        {"selected_names",  selectedNames},
        {"scene_path",      scenePath}
    };
}

// -----------------------------------------------------------------------
// Initialize
// -----------------------------------------------------------------------

void DMOLLMSceneService::Initialize(EditorComponent* editor)
{
    m_editor = editor;
    RegisterTools();
    wi::backlog::post("[DMO-LLM] Scene service initialized with "
                      + std::to_string(m_tools.size()) + " tools");
}

// -----------------------------------------------------------------------
// Scene context builder
// -----------------------------------------------------------------------

LLMSceneContext DMOLLMSceneService::BuildSceneContext() const
{
    LLMSceneContext ctx{};
    if (!m_editor) return ctx;

    auto& editorScene = m_editor->GetCurrentEditorScene();
    auto& scene = m_editor->GetCurrentScene();

    // Camera
    XMMATRIX camWorld = XMLoadFloat4x4(&editorScene.camera.View);
    camWorld = XMMatrixInverse(nullptr, camWorld);
    XMFLOAT4X4 camWorldF;
    XMStoreFloat4x4(&camWorldF, camWorld);
    ctx.cameraPosition = XMFLOAT3(camWorldF._41, camWorldF._42, camWorldF._43);
    ctx.cameraForward  = XMFLOAT3(camWorldF._31, camWorldF._32, camWorldF._33);

    // Entity count
    ctx.entityCount = static_cast<int>(scene.transforms.GetCount());

    // Selected entities
    ctx.selectedCount = 0;
    std::string names;
    for (auto& picked : m_editor->translator.selected)
    {
        ctx.selectedCount++;
        wi::scene::NameComponent* nameComp = scene.names.GetComponent(picked.entity);
        if (nameComp)
        {
            if (!names.empty()) names += ", ";
            names += nameComp->name;
        }
    }
    ctx.selectedNames = names;
    ctx.scenePath = editorScene.path;

    return ctx;
}

// -----------------------------------------------------------------------
// System prompt for LLM providers
// -----------------------------------------------------------------------

std::string DMOLLMSceneService::BuildSystemPrompt() const
{
    std::string prompt =
        "You are a 3D level design assistant for DMO (a large-scale MMO built on Wicked Engine).\n"
        "You control the scene editor using ONLY the tools listed below.\n"
        "Return ONLY valid JSON matching this schema:\n"
        "{\n"
        "  \"tool\": \"tool_name\",\n"
        "  \"parameters\": { ... tool-specific params ... },\n"
        "  \"description\": \"short human-readable summary\"\n"
        "}\n\n"
        "Available tools:\n";

    for (auto& tool : m_tools)
    {
        prompt += "- " + tool.name + ": " + tool.description + "\n";
    }

    return prompt;
}

// -----------------------------------------------------------------------
// MCP tools manifest
// -----------------------------------------------------------------------

json DMOLLMSceneService::BuildToolsManifest() const
{
    json tools = json::array();
    for (auto& tool : m_tools)
    {
        tools.push_back({
            {"name", tool.name},
            {"description", tool.description},
            {"inputSchema", tool.inputSchema}
        });
    }
    return tools;
}

// -----------------------------------------------------------------------
// Execute
// -----------------------------------------------------------------------

LLMCommandResult DMOLLMSceneService::Execute(const std::string& toolName,
                                              const json& params)
{
    for (auto& tool : m_tools)
    {
        if (tool.name == toolName)
        {
            wi::backlog::post("[DMO-LLM] Executing: " + toolName);
            return tool.handler(params);
        }
    }

    LLMCommandResult r;
    r.success = false;
    r.message = "Unknown tool: " + toolName;
    return r;
}

// -----------------------------------------------------------------------
// Tool listing
// -----------------------------------------------------------------------

std::vector<std::string> DMOLLMSceneService::GetToolNames() const
{
    std::vector<std::string> names;
    names.reserve(m_tools.size());
    for (auto& t : m_tools)
        names.push_back(t.name);
    return names;
}

json DMOLLMSceneService::GetToolSchema(const std::string& toolName) const
{
    for (auto& t : m_tools)
    {
        if (t.name == toolName)
            return t.inputSchema;
    }
    return json::object();
}

// -----------------------------------------------------------------------
// Tool registration
// -----------------------------------------------------------------------

void DMOLLMSceneService::RegisterTools()
{
    m_tools.clear();

    // Helper to build JSON Schema objects concisely
    auto prop = [](const std::string& type, const std::string& desc) -> json {
        return {{"type", type}, {"description", desc}};
    };
    auto arrayProp = [](const std::string& desc) -> json {
        return {{"type", "array"}, {"items", {{"type", "number"}}},
                {"minItems", 3}, {"maxItems", 3}, {"description", desc}};
    };

    // --- spawn_entity ---
    m_tools.push_back({
        "spawn_entity",
        "Create a new entity (object, light, decal, etc.) at a world position",
        {{"type", "object"},
         {"properties", {
             {"entity_type", {{"type", "string"}, {"enum", {"object","point_light","spot_light","directional_light","decal","force_field","emitter","camera","sound","env_probe"}},
                              {"description", "Type of entity to create"}}},
             {"name",        prop("string", "Display name for the entity")},
             {"position",    arrayProp("World position [x, y, z]")},
             {"model_path",  prop("string", "Path to .wiscene or model file (for object type)")}
         }},
         {"required", json::array({"entity_type"})}},
        [this](const json& p) { return CmdSpawnEntity(p); }
    });

    // --- delete_entity ---
    m_tools.push_back({
        "delete_entity",
        "Delete an entity by name or entity ID",
        {{"type", "object"},
         {"properties", {
             {"name",      prop("string", "Name of the entity to delete")},
             {"entity_id", prop("integer", "Entity ID (alternative to name)")}
         }}},
        [this](const json& p) { return CmdDeleteEntity(p); }
    });

    // --- move_entity ---
    m_tools.push_back({
        "move_entity",
        "Move an entity to a new world position",
        {{"type", "object"},
         {"properties", {
             {"name",      prop("string", "Entity name")},
             {"entity_id", prop("integer", "Entity ID")},
             {"position",  arrayProp("Target world position [x, y, z]")}
         }},
         {"required", json::array({"position"})}},
        [this](const json& p) { return CmdMoveEntity(p); }
    });

    // --- rotate_entity ---
    m_tools.push_back({
        "rotate_entity",
        "Rotate an entity by Euler angles in degrees",
        {{"type", "object"},
         {"properties", {
             {"name",      prop("string", "Entity name")},
             {"entity_id", prop("integer", "Entity ID")},
             {"rotation",  arrayProp("Euler angles [pitch, yaw, roll] in degrees")}
         }},
         {"required", json::array({"rotation"})}},
        [this](const json& p) { return CmdRotateEntity(p); }
    });

    // --- scale_entity ---
    m_tools.push_back({
        "scale_entity",
        "Scale an entity uniformly or per-axis",
        {{"type", "object"},
         {"properties", {
             {"name",      prop("string", "Entity name")},
             {"entity_id", prop("integer", "Entity ID")},
             {"scale",     arrayProp("Scale factors [x, y, z]")},
             {"uniform",   prop("number", "Uniform scale factor (alternative to per-axis)")}
         }}},
        [this](const json& p) { return CmdScaleEntity(p); }
    });

    // --- duplicate_entity ---
    m_tools.push_back({
        "duplicate_entity",
        "Duplicate an entity with optional offset",
        {{"type", "object"},
         {"properties", {
             {"name",      prop("string", "Entity name to duplicate")},
             {"entity_id", prop("integer", "Entity ID")},
             {"offset",    arrayProp("Position offset from original [x, y, z]")},
             {"count",     prop("integer", "Number of copies (default 1)")}
         }}},
        [this](const json& p) { return CmdDuplicateEntity(p); }
    });

    // --- set_material ---
    m_tools.push_back({
        "set_material",
        "Modify material properties on an entity (color, roughness, metalness, emissive)",
        {{"type", "object"},
         {"properties", {
             {"name",       prop("string", "Entity name")},
             {"entity_id",  prop("integer", "Entity ID")},
             {"base_color", arrayProp("Base color [r, g, b] in 0-1 range")},
             {"roughness",  prop("number", "Roughness 0-1")},
             {"metalness",  prop("number", "Metalness 0-1")},
             {"emissive",   prop("number", "Emissive strength")},
             {"opacity",    prop("number", "Opacity 0-1")}
         }}},
        [this](const json& p) { return CmdSetMaterial(p); }
    });

    // --- set_light ---
    m_tools.push_back({
        "set_light",
        "Modify light properties (color, intensity, range, type)",
        {{"type", "object"},
         {"properties", {
             {"name",       prop("string", "Light entity name")},
             {"entity_id",  prop("integer", "Entity ID")},
             {"color",      arrayProp("Light color [r, g, b] in 0-1 range")},
             {"intensity",  prop("number", "Light intensity")},
             {"range",      prop("number", "Light range in meters")},
             {"cast_shadow",prop("boolean", "Whether the light casts shadows")}
         }}},
        [this](const json& p) { return CmdSetLight(p); }
    });

    // --- set_weather ---
    m_tools.push_back({
        "set_weather",
        "Adjust weather/atmosphere settings (fog, sky, ambient)",
        {{"type", "object"},
         {"properties", {
             {"ambient",    arrayProp("Ambient color [r, g, b]")},
             {"fog_start",  prop("number", "Fog start distance")},
             {"fog_end",    prop("number", "Fog end distance")},
             {"fog_height", prop("number", "Fog height")},
             {"wind_direction", arrayProp("Wind direction [x, y, z]")},
             {"wind_speed", prop("number", "Wind speed")},
             {"cloudiness", prop("number", "Cloud coverage 0-1")}
         }}},
        [this](const json& p) { return CmdSetWeather(p); }
    });

    // --- set_fog ---
    m_tools.push_back({
        "set_fog",
        "Configure volumetric fog parameters",
        {{"type", "object"},
         {"properties", {
             {"color",   arrayProp("Fog color [r, g, b]")},
             {"density", prop("number", "Fog density")},
             {"start",   prop("number", "Fog start distance")},
             {"end",     prop("number", "Fog end distance")},
             {"height_start", prop("number", "Height fog start")},
             {"height_end",   prop("number", "Height fog end")}
         }}},
        [this](const json& p) { return CmdSetFog(p); }
    });

    // --- set_sky ---
    m_tools.push_back({
        "set_sky",
        "Adjust sky and sun parameters",
        {{"type", "object"},
         {"properties", {
             {"sun_direction", arrayProp("Sun direction vector [x, y, z]")},
             {"sun_color",     arrayProp("Sun color [r, g, b]")},
             {"sun_intensity", prop("number", "Sun intensity")},
             {"horizon_color", arrayProp("Horizon color [r, g, b]")},
             {"zenith_color",  arrayProp("Zenith color [r, g, b]")}
         }}},
        [this](const json& p) { return CmdSetSkyParameters(p); }
    });

    // --- generate_forest ---
    m_tools.push_back({
        "generate_forest",
        "Generate a procedural forest using the PCG forest tool",
        {{"type", "object"},
         {"properties", {
             {"biome",    {{"type", "string"}, {"enum", {"pine","temperate","ancient"}},
                           {"description", "Forest biome type"}}},
             {"density",  {{"type", "string"}, {"enum", {"low","medium","high"}},
                           {"description", "Tree density"}}},
             {"center",   arrayProp("Center position [x, y, z]")},
             {"radius",   prop("number", "Generation radius in meters")}
         }}},
        [this](const json& p) { return CmdGenerateForest(p); }
    });

    // --- place_cave ---
    m_tools.push_back({
        "place_cave",
        "Place a modular cave entrance or section",
        {{"type", "object"},
         {"properties", {
             {"module_type", {{"type", "string"}, {"enum", {"entrance","corridor","chamber","exit"}},
                              {"description", "Cave module type"}}},
             {"position",    arrayProp("World position [x, y, z]")},
             {"rotation",    arrayProp("Euler rotation [pitch, yaw, roll] degrees")}
         }},
         {"required", json::array({"module_type", "position"})}},
        [this](const json& p) { return CmdPlaceCave(p); }
    });

    // --- create_spline ---
    m_tools.push_back({
        "create_spline",
        "Create a spline path (for roads, fences, walls, rivers)",
        {{"type", "object"},
         {"properties", {
             {"spline_type", {{"type", "string"}, {"enum", {"road","fence","wall","river","path"}},
                              {"description", "Spline usage type"}}},
             {"points",      {{"type", "array"}, {"items", {{"type", "array"}, {"items", {{"type","number"}}}, {"minItems",3}, {"maxItems",3}}},
                              {"description", "Array of [x,y,z] control points"}}}
         }},
         {"required", json::array({"spline_type", "points"})}},
        [this](const json& p) { return CmdCreateSpline(p); }
    });

    // --- apply_landscape ---
    m_tools.push_back({
        "apply_landscape",
        "Apply a landscape rule to the current terrain area",
        {{"type", "object"},
         {"properties", {
             {"rule", {{"type", "string"}, {"enum", {"grassy_hills","rocky_cliffs","desert","snow","swamp"}},
                       {"description", "Landscape rule to apply"}}},
             {"center", arrayProp("Center position [x, y, z]")},
             {"radius", prop("number", "Affected radius in meters")}
         }},
         {"required", json::array({"rule"})}},
        [this](const json& p) { return CmdApplyLandscape(p); }
    });

    // --- get_scene_info ---
    m_tools.push_back({
        "get_scene_info",
        "Get information about the current scene (entity count, selected objects, camera position)",
        {{"type", "object"}, {"properties", json::object()}},
        [this](const json& p) { return CmdGetSceneInfo(p); }
    });

    // --- select_entity ---
    m_tools.push_back({
        "select_entity",
        "Select an entity by name or ID (or clear selection)",
        {{"type", "object"},
         {"properties", {
             {"name",      prop("string", "Entity name to select")},
             {"entity_id", prop("integer", "Entity ID to select")},
             {"clear",     prop("boolean", "If true, clear current selection first")}
         }}},
        [this](const json& p) { return CmdSelectEntity(p); }
    });

    // --- set_transform ---
    m_tools.push_back({
        "set_transform",
        "Set the full transform (position, rotation, scale) of an entity at once",
        {{"type", "object"},
         {"properties", {
             {"name",      prop("string", "Entity name")},
             {"entity_id", prop("integer", "Entity ID")},
             {"position",  arrayProp("World position [x, y, z]")},
             {"rotation",  arrayProp("Euler angles [pitch, yaw, roll] degrees")},
             {"scale",     arrayProp("Scale [x, y, z]")}
         }}},
        [this](const json& p) { return CmdSetTransform(p); }
    });

    // --- undo ---
    m_tools.push_back({
        "undo",
        "Undo the last editor operation",
        {{"type", "object"}, {"properties", json::object()}},
        [this](const json& p) { return CmdUndo(p); }
    });

    // --- redo ---
    m_tools.push_back({
        "redo",
        "Redo the last undone editor operation",
        {{"type", "object"}, {"properties", json::object()}},
        [this](const json& p) { return CmdRedo(p); }
    });
}
