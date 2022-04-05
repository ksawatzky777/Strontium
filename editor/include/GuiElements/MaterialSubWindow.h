#pragma once

// Macro include file.
#include "StrontiumPCH.h"

// Project includes.
#include "Core/ApplicationBase.h"
#include "Graphics/Material.h"
#include "Scenes/Scene.h"
#include "Scenes/Entity.h"
#include "GuiElements/GuiWindow.h"

namespace Strontium
{
  class MaterialSubWindow : public GuiWindow
  {
  public:
    MaterialSubWindow(EditorLayer* parentLayer);
    ~MaterialSubWindow();

    void onImGuiRender(bool &isOpen, Shared<Scene> activeScene);
    void onUpdate(float dt, Shared<Scene> activeScene);
    void onEvent(Event &event);

    void setSelectedMaterial(const Asset::Handle &matHandle) { this->selectedMaterial = matHandle; }

    Asset::Handle getSelectedMaterialHandle() const { return this->selectedMaterial; }
  private:
    Asset::Handle selectedMaterial;
    FileLoadTargets fileLoadTargets;
  };
}
