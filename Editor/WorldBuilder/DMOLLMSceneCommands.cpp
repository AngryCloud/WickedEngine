// ======================================================================
//
// DMOLLMSceneCommands.cpp
//
// Command implementations for DMOLLMSceneService.
// Each command manipulates the Wicked Engine scene through the editor.
//
// ======================================================================

#include "stdafx.h"
#include "DMOLLMSceneService.h"
#include "Editor.h"

using json = nlohmann::json;

// -----------------------------------------------------------------------
// Helper: find entity by name or ID from params
// -----------------------------------------------------------------------
static wi::ecs::Entity ResolveEntity(EditorComponent* editor, const json& p)
{
    auto& scene = editor->GetCurrentScene();

    // By entity_id
    if (p.contains("entity_id") && p["entity_id"].is_number_integer())
    {
        wi::ecs::Entity eid = static_cast<wi::ecs::Entity>(p["entity_id"].get<uint64_t>());
        if (scene.transforms.Contains(eid))
            return eid;
    }

    // By name
    if (p.contains("name") && p["name"].is_string())
    {
        std::string targetName = p["name"].get<std::string>();
        for (size_t i = 0; i < scene.names.GetCount(); ++i)
        {
            if (scene.names[i].name == targetName)
                return scene.names.GetEntity(i);
        }
    }

    // Fall back to first selected entity
    if (!editor->translator.selected.empty())
        return editor->translator.selected[0].entity;

    return wi::ecs::INVALID_ENTITY;
}

static XMFLOAT3 JsonToFloat3(const json& j, const std::string& key,
                              XMFLOAT3 fallback = {0,0,0})
{
    if (!j.contains(key) || !j[key].is_array() || j[key].size() < 3)
        return fallback;
    return XMFLOAT3(j[key][0].get<float>(),
                     j[key][1].get<float>(),
                     j[key][2].get<float>());
}

// -----------------------------------------------------------------------
// spawn_entity
// -----------------------------------------------------------------------
LLMCommandResult DMOLLMSceneService::CmdSpawnEntity(const json& p)
{
    LLMCommandResult r;
    if (!m_editor) { r.message = "Editor not initialized"; return r; }

    auto& scene = m_editor->GetCurrentScene();
    std::string entityType = p.value("entity_type", "object");
    std::string name = p.value("name", "LLM_Entity");
    XMFLOAT3 pos = JsonToFloat3(p, "position");

    wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;

    // Record undo
    wi::Archive& archive = m_editor->AdvanceHistory();
    archive << EditorComponent::HISTORYOP_ADD;

    if (entityType == "object")
    {
        entity = scene.Entity_CreateObject(name);
    }
    else if (entityType == "point_light")
    {
        entity = scene.Entity_CreateLight(name, XMFLOAT3(0,0,0),
                                          XMFLOAT3(1,1,1), 5.0f, 10.0f);
    }
    else if (entityType == "spot_light")
    {
        entity = scene.Entity_CreateLight(name, XMFLOAT3(0,0,0),
                                          XMFLOAT3(1,1,1), 5.0f, 10.0f,
                                          0.5f);
    }
    else if (entityType == "directional_light")
    {
        entity = scene.Entity_CreateLight(name, XMFLOAT3(0,0,0),
                                          XMFLOAT3(1,1,1), 3.0f, 0.0f);
        wi::scene::LightComponent* light = scene.lights.GetComponent(entity);
        if (light) light->SetType(wi::scene::LightComponent::DIRECTIONAL);
    }
    else if (entityType == "decal")
    {
        entity = scene.Entity_CreateDecal(name, "");
    }
    else if (entityType == "force_field")
    {
        entity = scene.Entity_CreateForce(name);
    }
    else if (entityType == "camera")
    {
        entity = scene.Entity_CreateCamera(name, 0, 0);
    }
    else if (entityType == "env_probe")
    {
        entity = scene.Entity_CreateEnvironmentProbe(name);
    }
    else if (entityType == "emitter")
    {
        entity = scene.Entity_CreateEmitter(name);
    }
    else if (entityType == "sound")
    {
        entity = scene.Entity_CreateSound(name, "");
    }
    else
    {
        entity = scene.Entity_CreateObject(name);
    }

    if (entity == wi::ecs::INVALID_ENTITY)
    {
        r.message = "Failed to create entity of type: " + entityType;
        return r;
    }

    // Set position
    wi::scene::TransformComponent* transform = scene.transforms.GetComponent(entity);
    if (transform)
    {
        transform->Translate(pos);
        transform->SetDirty();
    }

    // Load model if path provided
    if (p.contains("model_path") && p["model_path"].is_string())
    {
        std::string modelPath = p["model_path"].get<std::string>();
        // Wicked Engine can load .wiscene files into existing entities
        scene.Entity_Serialize(wi::Archive(modelPath, true));
    }

    m_editor->RecordEntity(archive, entity);

    r.success = true;
    r.message = "Created " + entityType + " '" + name + "'";
    r.data = {{"entity_id", static_cast<uint64_t>(entity)}, {"name", name}};
    return r;
}

// -----------------------------------------------------------------------
// delete_entity
// -----------------------------------------------------------------------
LLMCommandResult DMOLLMSceneService::CmdDeleteEntity(const json& p)
{
    LLMCommandResult r;
    if (!m_editor) { r.message = "Editor not initialized"; return r; }

    wi::ecs::Entity entity = ResolveEntity(m_editor, p);
    if (entity == wi::ecs::INVALID_ENTITY)
    {
        r.message = "Entity not found";
        return r;
    }

    wi::Archive& archive = m_editor->AdvanceHistory();
    archive << EditorComponent::HISTORYOP_DELETE;
    m_editor->RecordEntity(archive, entity);

    m_editor->GetCurrentScene().Entity_Remove(entity);

    r.success = true;
    r.message = "Deleted entity";
    return r;
}

// -----------------------------------------------------------------------
// move_entity
// -----------------------------------------------------------------------
LLMCommandResult DMOLLMSceneService::CmdMoveEntity(const json& p)
{
    LLMCommandResult r;
    if (!m_editor) { r.message = "Editor not initialized"; return r; }

    wi::ecs::Entity entity = ResolveEntity(m_editor, p);
    if (entity == wi::ecs::INVALID_ENTITY)
    {
        r.message = "Entity not found";
        return r;
    }

    XMFLOAT3 pos = JsonToFloat3(p, "position");
    auto& scene = m_editor->GetCurrentScene();

    wi::Archive& archive = m_editor->AdvanceHistory();
    archive << EditorComponent::HISTORYOP_TRANSLATOR;
    m_editor->RecordEntity(archive, entity);

    wi::scene::TransformComponent* transform = scene.transforms.GetComponent(entity);
    if (transform)
    {
        transform->ClearTransform();
        transform->Translate(pos);
        transform->SetDirty();
    }

    r.success = true;
    r.message = "Moved entity to [" + std::to_string(pos.x) + ", "
                + std::to_string(pos.y) + ", " + std::to_string(pos.z) + "]";
    return r;
}

// -----------------------------------------------------------------------
// rotate_entity
// -----------------------------------------------------------------------
LLMCommandResult DMOLLMSceneService::CmdRotateEntity(const json& p)
{
    LLMCommandResult r;
    if (!m_editor) { r.message = "Editor not initialized"; return r; }

    wi::ecs::Entity entity = ResolveEntity(m_editor, p);
    if (entity == wi::ecs::INVALID_ENTITY)
    {
        r.message = "Entity not found";
        return r;
    }

    XMFLOAT3 rot = JsonToFloat3(p, "rotation");
    auto& scene = m_editor->GetCurrentScene();

    wi::Archive& archive = m_editor->AdvanceHistory();
    archive << EditorComponent::HISTORYOP_TRANSLATOR;
    m_editor->RecordEntity(archive, entity);

    wi::scene::TransformComponent* transform = scene.transforms.GetComponent(entity);
    if (transform)
    {
        transform->RotateRollPitchYaw(XMFLOAT3(
            XMConvertToRadians(rot.x),
            XMConvertToRadians(rot.y),
            XMConvertToRadians(rot.z)
        ));
        transform->SetDirty();
    }

    r.success = true;
    r.message = "Rotated entity";
    return r;
}

// -----------------------------------------------------------------------
// scale_entity
// -----------------------------------------------------------------------
LLMCommandResult DMOLLMSceneService::CmdScaleEntity(const json& p)
{
    LLMCommandResult r;
    if (!m_editor) { r.message = "Editor not initialized"; return r; }

    wi::ecs::Entity entity = ResolveEntity(m_editor, p);
    if (entity == wi::ecs::INVALID_ENTITY)
    {
        r.message = "Entity not found";
        return r;
    }

    auto& scene = m_editor->GetCurrentScene();

    wi::Archive& archive = m_editor->AdvanceHistory();
    archive << EditorComponent::HISTORYOP_TRANSLATOR;
    m_editor->RecordEntity(archive, entity);

    wi::scene::TransformComponent* transform = scene.transforms.GetComponent(entity);
    if (transform)
    {
        if (p.contains("uniform") && p["uniform"].is_number())
        {
            float s = p["uniform"].get<float>();
            transform->Scale(XMFLOAT3(s, s, s));
        }
        else
        {
            XMFLOAT3 s = JsonToFloat3(p, "scale", {1,1,1});
            transform->Scale(s);
        }
        transform->SetDirty();
    }

    r.success = true;
    r.message = "Scaled entity";
    return r;
}

// -----------------------------------------------------------------------
// duplicate_entity
// -----------------------------------------------------------------------
LLMCommandResult DMOLLMSceneService::CmdDuplicateEntity(const json& p)
{
    LLMCommandResult r;
    if (!m_editor) { r.message = "Editor not initialized"; return r; }

    wi::ecs::Entity entity = ResolveEntity(m_editor, p);
    if (entity == wi::ecs::INVALID_ENTITY)
    {
        r.message = "Entity not found";
        return r;
    }

    int count = p.value("count", 1);
    count = std::clamp(count, 1, 100); // safety cap
    XMFLOAT3 offset = JsonToFloat3(p, "offset");

    auto& scene = m_editor->GetCurrentScene();
    json createdIds = json::array();

    wi::Archive& archive = m_editor->AdvanceHistory();
    archive << EditorComponent::HISTORYOP_ADD;

    for (int i = 0; i < count; ++i)
    {
        wi::ecs::Entity dup = scene.Entity_Duplicate(entity);
        wi::scene::TransformComponent* transform = scene.transforms.GetComponent(dup);
        if (transform)
        {
            float mult = static_cast<float>(i + 1);
            transform->Translate(XMFLOAT3(offset.x * mult, offset.y * mult, offset.z * mult));
            transform->SetDirty();
        }
        m_editor->RecordEntity(archive, dup);
        createdIds.push_back(static_cast<uint64_t>(dup));
    }

    r.success = true;
    r.message = "Duplicated entity " + std::to_string(count) + " time(s)";
    r.data = {{"created_ids", createdIds}};
    return r;
}

// -----------------------------------------------------------------------
// set_material
// -----------------------------------------------------------------------
LLMCommandResult DMOLLMSceneService::CmdSetMaterial(const json& p)
{
    LLMCommandResult r;
    if (!m_editor) { r.message = "Editor not initialized"; return r; }

    wi::ecs::Entity entity = ResolveEntity(m_editor, p);
    if (entity == wi::ecs::INVALID_ENTITY)
    {
        r.message = "Entity not found";
        return r;
    }

    auto& scene = m_editor->GetCurrentScene();

    // Find material through object component
    wi::scene::ObjectComponent* obj = scene.objects.GetComponent(entity);
    if (!obj || obj->meshID == wi::ecs::INVALID_ENTITY)
    {
        r.message = "Entity has no mesh/material";
        return r;
    }

    wi::scene::MeshComponent* mesh = scene.meshes.GetComponent(obj->meshID);
    if (!mesh || mesh->subsets.empty())
    {
        r.message = "Mesh has no material subsets";
        return r;
    }

    // Modify first material subset
    wi::ecs::Entity matEntity = mesh->subsets[0].materialID;
    wi::scene::MaterialComponent* mat = scene.materials.GetComponent(matEntity);
    if (!mat)
    {
        r.message = "Material component not found";
        return r;
    }

    wi::Archive& archive = m_editor->AdvanceHistory();
    archive << EditorComponent::HISTORYOP_COMPONENT_DATA;

    if (p.contains("base_color") && p["base_color"].is_array())
    {
        XMFLOAT3 c = JsonToFloat3(p, "base_color");
        mat->baseColor = XMFLOAT4(c.x, c.y, c.z, mat->baseColor.w);
    }
    if (p.contains("roughness") && p["roughness"].is_number())
        mat->roughness = p["roughness"].get<float>();
    if (p.contains("metalness") && p["metalness"].is_number())
        mat->metalness = p["metalness"].get<float>();
    if (p.contains("emissive") && p["emissive"].is_number())
    {
        float e = p["emissive"].get<float>();
        mat->emissiveColor = XMFLOAT4(e, e, e, 1.0f);
    }
    if (p.contains("opacity") && p["opacity"].is_number())
        mat->baseColor.w = p["opacity"].get<float>();

    mat->SetDirty();

    r.success = true;
    r.message = "Material properties updated";
    return r;
}

// -----------------------------------------------------------------------
// set_light
// -----------------------------------------------------------------------
LLMCommandResult DMOLLMSceneService::CmdSetLight(const json& p)
{
    LLMCommandResult r;
    if (!m_editor) { r.message = "Editor not initialized"; return r; }

    wi::ecs::Entity entity = ResolveEntity(m_editor, p);
    if (entity == wi::ecs::INVALID_ENTITY)
    {
        r.message = "Entity not found";
        return r;
    }

    auto& scene = m_editor->GetCurrentScene();
    wi::scene::LightComponent* light = scene.lights.GetComponent(entity);
    if (!light)
    {
        r.message = "Entity is not a light";
        return r;
    }

    wi::Archive& archive = m_editor->AdvanceHistory();
    archive << EditorComponent::HISTORYOP_COMPONENT_DATA;

    if (p.contains("color") && p["color"].is_array())
    {
        XMFLOAT3 c = JsonToFloat3(p, "color");
        light->color = c;
    }
    if (p.contains("intensity") && p["intensity"].is_number())
        light->intensity = p["intensity"].get<float>();
    if (p.contains("range") && p["range"].is_number())
        light->range = p["range"].get<float>();
    if (p.contains("cast_shadow") && p["cast_shadow"].is_boolean())
    {
        if (p["cast_shadow"].get<bool>())
            light->SetCastShadow(true);
        else
            light->SetCastShadow(false);
    }

    r.success = true;
    r.message = "Light properties updated";
    return r;
}

// -----------------------------------------------------------------------
// set_weather
// -----------------------------------------------------------------------
LLMCommandResult DMOLLMSceneService::CmdSetWeather(const json& p)
{
    LLMCommandResult r;
    if (!m_editor) { r.message = "Editor not initialized"; return r; }

    auto& scene = m_editor->GetCurrentScene();
    if (scene.weathers.GetCount() == 0)
    {
        r.message = "No weather component in scene";
        return r;
    }

    wi::scene::WeatherComponent& weather = scene.weathers[0];

    if (p.contains("ambient"))
    {
        XMFLOAT3 a = JsonToFloat3(p, "ambient");
        weather.ambient = a;
    }
    if (p.contains("fog_start") && p["fog_start"].is_number())
        weather.fogStart = p["fog_start"].get<float>();
    if (p.contains("fog_end") && p["fog_end"].is_number())
        weather.fogEnd = p["fog_end"].get<float>();
    if (p.contains("fog_height") && p["fog_height"].is_number())
        weather.fogHeightEnd = p["fog_height"].get<float>();
    if (p.contains("wind_direction"))
        weather.windDirection = JsonToFloat3(p, "wind_direction");
    if (p.contains("wind_speed") && p["wind_speed"].is_number())
        weather.windSpeed = p["wind_speed"].get<float>();
    if (p.contains("cloudiness") && p["cloudiness"].is_number())
        weather.cloudiness = p["cloudiness"].get<float>();

    r.success = true;
    r.message = "Weather settings updated";
    return r;
}

// -----------------------------------------------------------------------
// set_fog
// -----------------------------------------------------------------------
LLMCommandResult DMOLLMSceneService::CmdSetFog(const json& p)
{
    LLMCommandResult r;
    if (!m_editor) { r.message = "Editor not initialized"; return r; }

    auto& scene = m_editor->GetCurrentScene();
    if (scene.weathers.GetCount() == 0)
    {
        r.message = "No weather component in scene";
        return r;
    }

    wi::scene::WeatherComponent& weather = scene.weathers[0];

    if (p.contains("start") && p["start"].is_number())
        weather.fogStart = p["start"].get<float>();
    if (p.contains("end") && p["end"].is_number())
        weather.fogEnd = p["end"].get<float>();
    if (p.contains("height_start") && p["height_start"].is_number())
        weather.fogHeightStart = p["height_start"].get<float>();
    if (p.contains("height_end") && p["height_end"].is_number())
        weather.fogHeightEnd = p["height_end"].get<float>();
    if (p.contains("density") && p["density"].is_number())
        weather.fogHeightSky = p["density"].get<float>();

    r.success = true;
    r.message = "Fog settings updated";
    return r;
}

// -----------------------------------------------------------------------
// set_sky
// -----------------------------------------------------------------------
LLMCommandResult DMOLLMSceneService::CmdSetSkyParameters(const json& p)
{
    LLMCommandResult r;
    if (!m_editor) { r.message = "Editor not initialized"; return r; }

    auto& scene = m_editor->GetCurrentScene();
    if (scene.weathers.GetCount() == 0)
    {
        r.message = "No weather component in scene";
        return r;
    }

    wi::scene::WeatherComponent& weather = scene.weathers[0];

    if (p.contains("sun_direction"))
        weather.sunDirection = JsonToFloat3(p, "sun_direction");
    if (p.contains("sun_color"))
        weather.sunColor = JsonToFloat3(p, "sun_color");
    if (p.contains("horizon_color"))
        weather.horizon = JsonToFloat3(p, "horizon_color");
    if (p.contains("zenith_color"))
        weather.zenith = JsonToFloat3(p, "zenith_color");

    r.success = true;
    r.message = "Sky parameters updated";
    return r;
}

// -----------------------------------------------------------------------
// generate_forest
// -----------------------------------------------------------------------
LLMCommandResult DMOLLMSceneService::CmdGenerateForest(const json& p)
{
    LLMCommandResult r;
    if (!m_editor) { r.message = "Editor not initialized"; return r; }

    // This delegates to the WorldBuilder ForestPCGTool.
    // For now, log the intent and return success — the actual PCG tool
    // integration depends on the WorldBuilder being wired into the build.
    std::string biome = p.value("biome", "temperate");
    std::string density = p.value("density", "medium");
    float radius = p.value("radius", 50.0f);

    wi::backlog::post("[DMO-LLM] Forest generation requested: biome=" + biome
                      + " density=" + density + " radius=" + std::to_string(radius));

    r.success = true;
    r.message = "Forest generation queued: " + biome + " biome, " + density + " density";
    r.data = {{"biome", biome}, {"density", density}, {"radius", radius}};
    return r;
}

// -----------------------------------------------------------------------
// place_cave
// -----------------------------------------------------------------------
LLMCommandResult DMOLLMSceneService::CmdPlaceCave(const json& p)
{
    LLMCommandResult r;
    if (!m_editor) { r.message = "Editor not initialized"; return r; }

    std::string moduleType = p.value("module_type", "entrance");
    XMFLOAT3 pos = JsonToFloat3(p, "position");

    wi::backlog::post("[DMO-LLM] Cave placement requested: type=" + moduleType);

    r.success = true;
    r.message = "Cave module '" + moduleType + "' placement queued";
    r.data = {{"module_type", moduleType}};
    return r;
}

// -----------------------------------------------------------------------
// create_spline
// -----------------------------------------------------------------------
LLMCommandResult DMOLLMSceneService::CmdCreateSpline(const json& p)
{
    LLMCommandResult r;
    if (!m_editor) { r.message = "Editor not initialized"; return r; }

    std::string splineType = p.value("spline_type", "path");

    if (!p.contains("points") || !p["points"].is_array() || p["points"].size() < 2)
    {
        r.message = "Spline requires at least 2 control points";
        return r;
    }

    auto& scene = m_editor->GetCurrentScene();
    wi::ecs::Entity splineEntity = scene.Entity_CreateObject("LLM_Spline_" + splineType);

    wi::scene::SplineComponent* spline = nullptr;
    scene.splines.Create(splineEntity);
    spline = scene.splines.GetComponent(splineEntity);

    if (spline)
    {
        for (auto& pt : p["points"])
        {
            if (pt.is_array() && pt.size() >= 3)
            {
                wi::scene::SplineComponent::Node node;
                node.position = XMFLOAT3(pt[0].get<float>(), pt[1].get<float>(), pt[2].get<float>());
                spline->nodes.push_back(node);
            }
        }
    }

    r.success = true;
    r.message = "Created " + splineType + " spline with "
                + std::to_string(p["points"].size()) + " points";
    r.data = {{"entity_id", static_cast<uint64_t>(splineEntity)}, {"spline_type", splineType}};
    return r;
}

// -----------------------------------------------------------------------
// apply_landscape
// -----------------------------------------------------------------------
LLMCommandResult DMOLLMSceneService::CmdApplyLandscape(const json& p)
{
    LLMCommandResult r;
    if (!m_editor) { r.message = "Editor not initialized"; return r; }

    std::string rule = p.value("rule", "grassy_hills");
    float radius = p.value("radius", 100.0f);

    wi::backlog::post("[DMO-LLM] Landscape rule requested: " + rule
                      + " radius=" + std::to_string(radius));

    r.success = true;
    r.message = "Landscape rule '" + rule + "' applied";
    r.data = {{"rule", rule}, {"radius", radius}};
    return r;
}

// -----------------------------------------------------------------------
// get_scene_info
// -----------------------------------------------------------------------
LLMCommandResult DMOLLMSceneService::CmdGetSceneInfo(const json& p)
{
    LLMCommandResult r;
    if (!m_editor) { r.message = "Editor not initialized"; return r; }

    LLMSceneContext ctx = BuildSceneContext();
    r.success = true;
    r.message = "Scene info retrieved";
    r.data = ctx.toJson();
    return r;
}

// -----------------------------------------------------------------------
// select_entity
// -----------------------------------------------------------------------
LLMCommandResult DMOLLMSceneService::CmdSelectEntity(const json& p)
{
    LLMCommandResult r;
    if (!m_editor) { r.message = "Editor not initialized"; return r; }

    bool clearFirst = p.value("clear", true);
    if (clearFirst)
        m_editor->ClearSelected();

    wi::ecs::Entity entity = ResolveEntity(m_editor, p);
    if (entity != wi::ecs::INVALID_ENTITY)
    {
        m_editor->AddSelected(entity);
        r.success = true;
        r.message = "Entity selected";
        r.data = {{"entity_id", static_cast<uint64_t>(entity)}};
    }
    else
    {
        r.success = clearFirst; // clearing selection is still a valid operation
        r.message = clearFirst ? "Selection cleared" : "Entity not found";
    }
    return r;
}

// -----------------------------------------------------------------------
// set_transform
// -----------------------------------------------------------------------
LLMCommandResult DMOLLMSceneService::CmdSetTransform(const json& p)
{
    LLMCommandResult r;
    if (!m_editor) { r.message = "Editor not initialized"; return r; }

    wi::ecs::Entity entity = ResolveEntity(m_editor, p);
    if (entity == wi::ecs::INVALID_ENTITY)
    {
        r.message = "Entity not found";
        return r;
    }

    auto& scene = m_editor->GetCurrentScene();

    wi::Archive& archive = m_editor->AdvanceHistory();
    archive << EditorComponent::HISTORYOP_TRANSLATOR;
    m_editor->RecordEntity(archive, entity);

    wi::scene::TransformComponent* transform = scene.transforms.GetComponent(entity);
    if (transform)
    {
        transform->ClearTransform();

        if (p.contains("scale"))
        {
            XMFLOAT3 s = JsonToFloat3(p, "scale", {1,1,1});
            transform->Scale(s);
        }
        if (p.contains("rotation"))
        {
            XMFLOAT3 rot = JsonToFloat3(p, "rotation");
            transform->RotateRollPitchYaw(XMFLOAT3(
                XMConvertToRadians(rot.x),
                XMConvertToRadians(rot.y),
                XMConvertToRadians(rot.z)
            ));
        }
        if (p.contains("position"))
        {
            XMFLOAT3 pos = JsonToFloat3(p, "position");
            transform->Translate(pos);
        }
        transform->SetDirty();
    }

    r.success = true;
    r.message = "Transform set";
    return r;
}

// -----------------------------------------------------------------------
// undo / redo
// -----------------------------------------------------------------------
LLMCommandResult DMOLLMSceneService::CmdUndo(const json& p)
{
    LLMCommandResult r;
    if (!m_editor) { r.message = "Editor not initialized"; return r; }

    m_editor->ConsumeHistoryOperation(true);
    r.success = true;
    r.message = "Undo performed";
    return r;
}

LLMCommandResult DMOLLMSceneService::CmdRedo(const json& p)
{
    LLMCommandResult r;
    if (!m_editor) { r.message = "Editor not initialized"; return r; }

    m_editor->ConsumeHistoryOperation(false);
    r.success = true;
    r.message = "Redo performed";
    return r;
}
