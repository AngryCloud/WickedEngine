#include "stdafx.h"
#include "RiverSplineWindow.h"

void RiverSplineWindow::Create(EditorComponent* editorIn)
{
    editor = editorIn;

    wi::gui::Window::Create("River Spline Tool", wi::gui::Window::WindowControls::CLOSE_AND_COLLAPSE);
    SetSize(XMFLOAT2(300, 200));

    float y = 10;
    float padding = 5;

    loadButton.Create("Load Spline JSON");
    loadButton.SetPos(XMFLOAT2(10, y));
    loadButton.SetSize(XMFLOAT2(200, 25));
    loadButton.OnClick([this](wi::gui::EventArgs) {
        wi::helper::FileDialogParams params;
        params.type = wi::helper::FileDialogParams::OPEN;
        params.description = "JSON Spline Data";
        params.extensions = { "json" };
        wi::helper::FileDialog(params, [this](const std::string& fileName) {
            wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                [this, fileName](uint64_t) {
                    // Logic to parse USplineComponent points goes here.
                    // Emulating success:
                    SetStatus("Loaded spline points. Generating mesh...");
                    BuildRiverMesh();
                });
        });
    });
    AddWidget(&loadButton);
    
    y += 30 + padding;
    
    clearButton.Create("Clear River");
    clearButton.SetPos(XMFLOAT2(10, y));
    clearButton.SetSize(XMFLOAT2(200, 25));
    clearButton.OnClick([this](wi::gui::EventArgs) {
        if (m_riverEntity != wi::ecs::INVALID_ENTITY)
        {
            wi::scene::GetScene().Entity_Remove(m_riverEntity);
            m_riverEntity = wi::ecs::INVALID_ENTITY;
            m_points.clear();
            SetStatus("River cleared.");
        }
    });
    AddWidget(&clearButton);

    y += 30 + padding;

    widthSlider.Create(1.0f, 20.0f, 5.0f, 100, "Width");
    widthSlider.SetPos(XMFLOAT2(60, y));
    widthSlider.SetSize(XMFLOAT2(150, 25));
    widthSlider.OnSlide([this](wi::gui::EventArgs) {
        if (!m_points.empty())
        {
            BuildRiverMesh();
        }
    });
    AddWidget(&widthSlider);

    y += 30 + padding;

    statusLabel.Create("Status");
    statusLabel.SetText("Ready.");
    statusLabel.SetPos(XMFLOAT2(10, y));
    statusLabel.SetSize(XMFLOAT2(280, 20));
    AddWidget(&statusLabel);
    
    SetVisible(false);
}

void RiverSplineWindow::SetStatus(const std::string& msg)
{
    statusLabel.SetText(msg);
}

void RiverSplineWindow::ResizeLayout()
{
    wi::gui::Window::ResizeLayout();
}

void RiverSplineWindow::Update(const wi::Canvas& canvas, float dt)
{
    wi::gui::Window::Update(canvas, dt);
}

void RiverSplineWindow::BuildRiverMesh()
{
    if (m_points.empty())
    {
        // Mock points for testing when nothing is loaded
        m_points.push_back({ XMFLOAT3(0, 0, 0), XMFLOAT3(1, 0, 0), XMFLOAT3(1, 0, 0) });
        m_points.push_back({ XMFLOAT3(10, 0, 10), XMFLOAT3(1, 0, 1), XMFLOAT3(1, 0, 1) });
        m_points.push_back({ XMFLOAT3(20, 0, 0), XMFLOAT3(1, 0, -1), XMFLOAT3(1, 0, -1) });
    }

    wi::scene::Scene& scene = wi::scene::GetScene();

    if (m_riverEntity == wi::ecs::INVALID_ENTITY)
    {
        m_riverEntity = scene.Entity_CreateObject("ProceduralRiver");
    }

    wi::scene::ObjectComponent* object = scene.objects.GetComponent(m_riverEntity);
    
    // Create new mesh
    wi::scene::MeshComponent mesh;
    float halfWidth = widthSlider.GetValue() * 0.5f;

    // Simple flat spline extrusion
    for (size_t i = 0; i < m_points.size(); ++i)
    {
        XMFLOAT3 p = m_points[i].position;
        // Direction to next point (mock tangent for perpendicular)
        XMFLOAT3 dir = (i < m_points.size() - 1) ? 
                        XMFLOAT3(m_points[i+1].position.x - p.x, 0.0f, m_points[i+1].position.z - p.z) : 
                        XMFLOAT3(p.x - m_points[i-1].position.x, 0.0f, p.z - m_points[i-1].position.z);
        
        XMVECTOR vDir = XMVector3Normalize(XMLoadFloat3(&dir));
        XMVECTOR vUp = XMVectorSet(0, 1, 0, 0);
        XMVECTOR vRight = XMVector3Cross(vUp, vDir);
        
        XMFLOAT3 right;
        XMStoreFloat3(&right, vRight);

        // Vertices
        mesh.vertex_positions.push_back(XMFLOAT3(p.x - right.x * halfWidth, p.y, p.z - right.z * halfWidth));
        mesh.vertex_positions.push_back(XMFLOAT3(p.x + right.x * halfWidth, p.y, p.z + right.z * halfWidth));
        
        // UVs
        mesh.vertex_uvset_0.push_back(XMFLOAT2(0.0f, static_cast<float>(i)));
        mesh.vertex_uvset_0.push_back(XMFLOAT2(1.0f, static_cast<float>(i)));
        
        // Normals
        mesh.vertex_normals.push_back(XMFLOAT3(0, 1, 0));
        mesh.vertex_normals.push_back(XMFLOAT3(0, 1, 0));
    }

    // Indices
    for (size_t i = 0; i < m_points.size() - 1; ++i)
    {
        uint32_t i0 = static_cast<uint32_t>(i * 2);
        mesh.indices.push_back(i0);
        mesh.indices.push_back(i0 + 1);
        mesh.indices.push_back(i0 + 2);
        
        mesh.indices.push_back(i0 + 2);
        mesh.indices.push_back(i0 + 1);
        mesh.indices.push_back(i0 + 3);
    }

    mesh.CreateRenderData();
    wi::ecs::Entity meshEntity = scene.Entity_CreateMesh("RiverMesh");
    *scene.meshes.GetComponent(meshEntity) = std::move(mesh);
    
    // Assign to Object
    object->meshID = meshEntity;
    scene.Component_Attach(meshEntity, m_riverEntity, true);
}
