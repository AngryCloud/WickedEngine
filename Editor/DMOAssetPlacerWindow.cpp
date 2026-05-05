// Editor/DMOAssetPlacerWindow.cpp
// DMO Asset Placer panel — placement modes and ghost entity (Task 12).

#include "stdafx.h"
#include "DMOAssetPlacerWindow.h"
#include "Editor.h"

#include "wiRandom.h"
#include "wiInput.h"

#include <cmath>
#include <algorithm>

// ---------------------------------------------------------------------------
// Create
// ---------------------------------------------------------------------------

void DMOAssetPlacerWindow::Create(EditorComponent* editorIn)
{
    editor = editorIn;

    wi::gui::Window::Create("Asset Placer", wi::gui::Window::WindowControls::CLOSE_AND_COLLAPSE);
    SetSize(XMFLOAT2(320, 300));

    // Mode combo: Free / Grid / Surface-snap / Brush
    modeCombo.Create("Mode: ");
    modeCombo.AddItem("Free");
    modeCombo.AddItem("Grid");
    modeCombo.AddItem("Surface-snap");
    modeCombo.AddItem("Brush");
    modeCombo.SetSelected(0);
    modeCombo.OnSelect([this](wi::gui::EventArgs args) {
        m_mode = static_cast<PlacementMode>(args.iValue);
        if (m_mode == PlacementMode::Brush)
            RemoveGhostEntity();
    });
    AddWidget(&modeCombo);

    // Grid size slider [0.1, 100.0]
    gridSizeSlider.Create(0.1f, 100.0f, 1.0f, 999, "Grid Size: ");
    gridSizeSlider.SetTooltip("Snap interval in meters (Grid mode)");
    AddWidget(&gridSizeSlider);

    // Brush radius slider [0.5, 50.0]
    brushRadiusSlider.Create(0.5f, 50.0f, 5.0f, 495, "Radius: ");
    brushRadiusSlider.SetTooltip("Brush radius in meters (Brush mode)");
    AddWidget(&brushRadiusSlider);

    // Brush density slider [0.1, 10.0]
    brushDensitySlider.Create(0.1f, 10.0f, 1.0f, 99, "Density: ");
    brushDensitySlider.SetTooltip("Instances per square meter (Brush mode)");
    AddWidget(&brushDensitySlider);

    // Scale min/max sliders [0.01, 10.0]
    scaleMinSlider.Create(0.01f, 10.0f, 0.8f, 999, "Scale Min: ");
    scaleMinSlider.SetTooltip("Minimum random scale");
    AddWidget(&scaleMinSlider);

    scaleMaxSlider.Create(0.01f, 10.0f, 1.2f, 999, "Scale Max: ");
    scaleMaxSlider.SetTooltip("Maximum random scale");
    AddWidget(&scaleMaxSlider);

    // Rotation Y range slider [0.0, 360.0]
    rotYRangeSlider.Create(0.0f, 360.0f, 360.0f, 360, "Rotation Y Range: ");
    rotYRangeSlider.SetTooltip("Random Y-axis rotation range in degrees");
    AddWidget(&rotYRangeSlider);

    // Tilt range slider [0.0, 45.0]
    tiltRangeSlider.Create(0.0f, 45.0f, 0.0f, 450, "Tilt Range: ");
    tiltRangeSlider.SetTooltip("Random tilt range in degrees");
    AddWidget(&tiltRangeSlider);

    SetVisible(false);
}

// ---------------------------------------------------------------------------
// ResizeLayout
// ---------------------------------------------------------------------------

void DMOAssetPlacerWindow::ResizeLayout()
{
    wi::gui::Window::ResizeLayout();

    const float padding  = 4.0f;
    const float itemH    = 18.0f;
    const float winW     = GetSize().x;

    float y = padding;

    modeCombo.SetPos(XMFLOAT2(padding, y));
    modeCombo.SetSize(XMFLOAT2(winW - padding * 2.0f, itemH));
    y += itemH + padding;

    gridSizeSlider.SetPos(XMFLOAT2(padding, y));
    gridSizeSlider.SetSize(XMFLOAT2(winW - padding * 2.0f, itemH));
    y += itemH + padding;

    brushRadiusSlider.SetPos(XMFLOAT2(padding, y));
    brushRadiusSlider.SetSize(XMFLOAT2(winW - padding * 2.0f, itemH));
    y += itemH + padding;

    brushDensitySlider.SetPos(XMFLOAT2(padding, y));
    brushDensitySlider.SetSize(XMFLOAT2(winW - padding * 2.0f, itemH));
    y += itemH + padding;

    scaleMinSlider.SetPos(XMFLOAT2(padding, y));
    scaleMinSlider.SetSize(XMFLOAT2(winW - padding * 2.0f, itemH));
    y += itemH + padding;

    scaleMaxSlider.SetPos(XMFLOAT2(padding, y));
    scaleMaxSlider.SetSize(XMFLOAT2(winW - padding * 2.0f, itemH));
    y += itemH + padding;

    rotYRangeSlider.SetPos(XMFLOAT2(padding, y));
    rotYRangeSlider.SetSize(XMFLOAT2(winW - padding * 2.0f, itemH));
    y += itemH + padding;

    tiltRangeSlider.SetPos(XMFLOAT2(padding, y));
    tiltRangeSlider.SetSize(XMFLOAT2(winW - padding * 2.0f, itemH));
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------

void DMOAssetPlacerWindow::Update(const wi::Canvas& canvas, float dt)
{
    wi::gui::Window::Update(canvas, dt);

    if (!IsVisible())
        return;

    // Surface-snap mode: pick each frame and update ghost entity
    if (m_mode == PlacementMode::SurfaceSnap)
    {
        wi::primitive::Ray pickRay = editor->pickRay;
        wi::scene::Scene& scene = editor->GetCurrentScene();
        wi::scene::PickResult hit = wi::scene::Pick(pickRay, wi::enums::FILTER_OPAQUE, ~0u, scene);
        if (hit.entity != wi::ecs::INVALID_ENTITY)
        {
            UpdateGhostEntity(hit.position, hit.normal);
        }
        else
        {
            HideGhostEntity();
        }
    }
    // Grid mode: snap cursor world position to grid and update ghost entity
    else if (m_mode == PlacementMode::Grid)
    {
        wi::primitive::Ray pickRay = editor->pickRay;
        wi::scene::Scene& scene = editor->GetCurrentScene();
        wi::scene::PickResult hit = wi::scene::Pick(pickRay, wi::enums::FILTER_OPAQUE, ~0u, scene);
        if (hit.entity != wi::ecs::INVALID_ENTITY)
        {
            XMFLOAT3 snapped = SnapToGrid(hit.position, gridSizeSlider.GetValue());
            UpdateGhostEntity(snapped, XMFLOAT3(0, 1, 0));
        }
        else
        {
            HideGhostEntity();
        }
    }

    // ---------------------------------------------------------------------------
    // Scroll wheel overrides (13.3)
    // ---------------------------------------------------------------------------
    {
        float scrollDelta = wi::input::GetPointer().z;  // z = scroll wheel delta
        if (scrollDelta != 0.0f)
        {
            bool shiftHeld = wi::input::Down(wi::input::KEYBOARD_BUTTON_LSHIFT) ||
                             wi::input::Down(wi::input::KEYBOARD_BUTTON_RSHIFT);
            bool ctrlHeld  = wi::input::Down(wi::input::KEYBOARD_BUTTON_LCONTROL) ||
                             wi::input::Down(wi::input::KEYBOARD_BUTTON_RCONTROL);

            if (ctrlHeld)
            {
                m_scrollScale *= std::pow(1.1f, scrollDelta);
                m_scrollScale = std::clamp(m_scrollScale, 0.01f, 10.0f);
            }
            else if (shiftHeld)
            {
                m_scrollHeightOfs += scrollDelta * 0.1f;
            }
            else
            {
                m_scrollYRotation += scrollDelta * 15.0f;
            }
        }
    }

    // ---------------------------------------------------------------------------
    // Brush mode placement (13.6)
    // ---------------------------------------------------------------------------
    if (m_mode == PlacementMode::Brush && IsVisible())
    {
        float radius  = brushRadiusSlider.GetValue();
        float density = brushDensitySlider.GetValue();
        int   count   = ComputeBrushCount(density, radius);

        if (wi::input::Down(wi::input::MOUSE_BUTTON_LEFT))
        {
            wi::scene::Scene& scene = editor->GetCurrentScene();
            for (int i = 0; i < count; ++i)
            {
                float angle = wi::random::GetRandom(0.0f, 2.0f * 3.14159265f);
                float dist  = wi::random::GetRandom(0.0f, radius);
                XMFLOAT3 offset(std::cos(angle) * dist, 0.0f, std::sin(angle) * dist);

                // Offset the pick ray origin in the XZ plane by the brush sample
                wi::primitive::Ray sampleRay = editor->pickRay;
                sampleRay.origin.x += offset.x;
                sampleRay.origin.z += offset.z;

                wi::scene::PickResult hit = wi::scene::Pick(sampleRay, wi::enums::FILTER_OPAQUE, ~0u, scene);
                if (hit.entity != wi::ecs::INVALID_ENTITY)
                {
                    PlaceInstance(hit.position, hit.normal, false);  // accumulate in m_strokeEntities
                }
            }
        }
        else if (wi::input::Release(wi::input::MOUSE_BUTTON_LEFT))
        {
            CommitBrushStroke();
        }
    }
}

// ---------------------------------------------------------------------------
// Ghost entity helpers
// ---------------------------------------------------------------------------

void DMOAssetPlacerWindow::UpdateGhostEntity(const XMFLOAT3& worldPos, const XMFLOAT3& normal)
{
    wi::scene::Scene& scene = editor->GetCurrentScene();

    // Create ghost entity on first use
    if (m_ghostEntity == wi::ecs::INVALID_ENTITY)
    {
        m_ghostEntity = scene.Entity_CreateObject("DMO_GhostPreview");
        // Set 50% opacity
        wi::scene::ObjectComponent* obj = scene.objects.GetComponent(m_ghostEntity);
        if (obj != nullptr)
        {
            obj->color.w = 0.5f;
        }
    }

    // Update transform: position at worldPos, orient Y-axis along normal
    wi::scene::TransformComponent* transform = scene.transforms.GetComponent(m_ghostEntity);
    if (transform != nullptr)
    {
        // Build a rotation that maps local Y to the surface normal
        XMVECTOR up = XMLoadFloat3(&normal);
        // Avoid degenerate case when normal is nearly parallel to world forward
        XMVECTOR worldForward = XMVectorSet(0, 0, 1, 0);
        XMVECTOR worldRight   = XMVectorSet(1, 0, 0, 0);
        XMVECTOR dot = XMVector3Dot(up, worldForward);
        XMVECTOR forward;
        if (std::fabs(XMVectorGetX(dot)) > 0.99f)
            forward = XMVector3Cross(up, worldRight);
        else
            forward = XMVector3Cross(up, worldForward);
        forward = XMVector3Normalize(forward);
        XMVECTOR right = XMVector3Normalize(XMVector3Cross(forward, up));

        // Build rotation matrix from right/up/forward axes
        XMMATRIX rot = XMMatrixIdentity();
        rot.r[0] = right;
        rot.r[1] = up;
        rot.r[2] = forward;
        rot.r[3] = XMVectorSet(0, 0, 0, 1);

        XMVECTOR scale, rotQ, trans;
        XMMatrixDecompose(&scale, &rotQ, &trans, rot);

        transform->ClearTransform();
        XMFLOAT4 rotF;
        XMStoreFloat4(&rotF, rotQ);
        transform->Rotate(rotF);
        transform->Translate(worldPos);
        transform->SetDirty();
    }
}

void DMOAssetPlacerWindow::HideGhostEntity()
{
    if (m_ghostEntity == wi::ecs::INVALID_ENTITY)
        return;

    wi::scene::Scene& scene = editor->GetCurrentScene();
    wi::scene::TransformComponent* transform = scene.transforms.GetComponent(m_ghostEntity);
    if (transform != nullptr)
    {
        // Move far off-screen
        transform->ClearTransform();
        transform->Translate(XMFLOAT3(0.0f, -99999.0f, 0.0f));
        transform->SetDirty();
    }
}

void DMOAssetPlacerWindow::RemoveGhostEntity()
{
    if (m_ghostEntity == wi::ecs::INVALID_ENTITY)
        return;

    editor->GetCurrentScene().Entity_Remove(m_ghostEntity);
    m_ghostEntity = wi::ecs::INVALID_ENTITY;
}

// ---------------------------------------------------------------------------
// Placement helpers
// ---------------------------------------------------------------------------

XMFLOAT3 DMOAssetPlacerWindow::SnapToGrid(const XMFLOAT3& worldPos, float gridSize) const
{
    XMFLOAT3 snapped = worldPos;
    snapped.x = std::round(worldPos.x / gridSize) * gridSize;
    snapped.z = std::round(worldPos.z / gridSize) * gridSize;
    // Y unchanged
    return snapped;
}

int DMOAssetPlacerWindow::ComputeBrushCount(float density, float radius) const
{
    // Implemented in Task 13.
    const int maxCount = static_cast<int>(std::floor(density * radius * radius * 3.14159265f));
    return std::max(1, maxCount);
}

XMFLOAT3 DMOAssetPlacerWindow::ApplyRandomizers(const XMFLOAT3& basePos, const XMFLOAT3& normal) const
{
    // Apply height offset along the surface normal (scroll-wheel height override)
    XMFLOAT3 result = basePos;
    result.x += normal.x * m_scrollHeightOfs;
    result.y += normal.y * m_scrollHeightOfs;
    result.z += normal.z * m_scrollHeightOfs;
    return result;
}

void DMOAssetPlacerWindow::PlaceInstance(const XMFLOAT3& pos, const XMFLOAT3& normal, bool recordHistory)
{
    if (m_selectedAssetPath.empty())
        return;

    wi::scene::Scene& scene = editor->GetCurrentScene();

    // Apply height offset along normal
    XMFLOAT3 finalPos = ApplyRandomizers(pos, normal);

    // Create entity
    wi::ecs::Entity entity = scene.Entity_CreateObject("DMO_PlacedAsset");

    // Compute random scale in [scaleMin, scaleMax]
    float scaleMin = scaleMinSlider.GetValue();
    float scaleMax = scaleMaxSlider.GetValue();
    if (scaleMin > scaleMax)
        scaleMin = scaleMax;  // clamp per requirement 17.3

    float scale = wi::random::GetRandom(scaleMin, scaleMax);
    scale *= m_scrollScale;  // apply scroll-wheel scale override
    scale = std::clamp(scale, 0.01f, 10.0f);

    // Random Y rotation in [0, rotYRange] plus scroll-wheel override
    float rotY = wi::random::GetRandom(0.0f, rotYRangeSlider.GetValue());
    rotY += m_scrollYRotation;

    // Random tilt in [0, tiltRange] around a random horizontal axis
    float tilt = wi::random::GetRandom(0.0f, tiltRangeSlider.GetValue());

    // Apply transform
    wi::scene::TransformComponent* transform = scene.transforms.GetComponent(entity);
    if (transform != nullptr)
    {
        transform->ClearTransform();
        transform->Scale(XMFLOAT3(scale, scale, scale));

        // Apply tilt around a random horizontal axis
        if (tilt > 0.0f)
        {
            float tiltAxisAngle = wi::random::GetRandom(0.0f, 360.0f);
            XMFLOAT3 tiltAxis(
                std::cos(XMConvertToRadians(tiltAxisAngle)),
                0.0f,
                std::sin(XMConvertToRadians(tiltAxisAngle))
            );
            transform->RotateRollPitchYaw(XMFLOAT3(
                XMConvertToRadians(tilt) * tiltAxis.z,
                0.0f,
                XMConvertToRadians(tilt) * tiltAxis.x
            ));
        }

        // Apply Y rotation
        transform->RotateRollPitchYaw(XMFLOAT3(0.0f, XMConvertToRadians(rotY), 0.0f));
        transform->Translate(finalPos);
        transform->SetDirty();
    }

    if (recordHistory)
    {
        editor->AdvanceHistory();
    }
    else
    {
        m_strokeEntities.push_back(entity);
    }
}

void DMOAssetPlacerWindow::CommitBrushStroke()
{
    if (!m_strokeEntities.empty())
    {
        editor->AdvanceHistory();
        m_strokeEntities.clear();
    }
}
