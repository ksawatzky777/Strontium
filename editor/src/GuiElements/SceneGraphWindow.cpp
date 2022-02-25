#include "GuiElements/SceneGraphWindow.h"

// STL includes.
#include <cstring>

// Project includes.
#include "Graphics/Renderer.h"
#include "GuiElements/Styles.h"
#include "Scenes/Components.h"
#include "Serialization/YamlSerialization.h"
#include "Utils/AsyncAssetLoading.h"

// ImGui includes.
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

// Some math for decomposing matrix transformations.
#include "glm/gtx/matrix_decompose.hpp"

// ImGizmo goodies.
#include "imguizmo/ImGuizmo.h"

namespace Strontium
{
  // Templated helper functions for components.
  //----------------------------------------------------------------------------
  // Draw the component's 'properties' gui elements.
  template <typename T, typename Function>
  static void drawComponentProperties(const std::string &name, Entity parent, Function ui)
  {
    if (!parent)
      return;

    if (parent.hasComponent<T>())
    {
      auto& component = parent.getComponent<T>();
      if (ImGui::CollapsingHeader(name.c_str(), ImGuiTreeNodeFlags_AllowItemOverlap))
      {
        ImGui::SameLine(ImGui::GetWindowWidth() - 30);
        if (ImGui::Button(ICON_FA_TRASH_O))
        {
          parent.removeComponent<T>();
        }
        ui(component);
      }
      else
      {
        ImGui::SameLine(ImGui::GetWindowWidth() - 30);
        if (ImGui::Button(ICON_FA_TRASH_O))
        {
          parent.removeComponent<T>();
        }
      }
    }
  }

  // Draw the component's 'add' gui elements.
  template <typename T, typename ... Args>
  static void drawComponentAdd(const std::string &name, Entity parent,
                               Args ... args)
  {
    if (!parent)
      return;

    if (!parent.hasComponent<T>())
    {
      if (ImGui::MenuItem(name.c_str()))
        parent.addComponent<T>(std::forward<Args>(args)...);
    }
    else
    {
      ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
      ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
      ImGui::MenuItem(name.c_str());
      ImGui::PopItemFlag();
      ImGui::PopStyleVar();
    }
  }

  // Draw the component's 'remove' gui elements.
  template <typename T>
  static void drawComponentRemove(const std::string &name, Entity parent)
  {
    if (!parent)
      return;

    if (parent.hasComponent<T>())
    {
      if (ImGui::MenuItem(name.c_str()))
        parent.removeComponent<T>();
    }
    else
    {
      ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
      ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
      ImGui::MenuItem(name.c_str());
      ImGui::PopItemFlag();
      ImGui::PopStyleVar();
    }
  }

  // Copy a component between a source and a target.
  template <typename T>
  static void copyComponent(Entity source, Entity target)
  {
    if (!(source && target))
      return;

    if (source.hasComponent<T>())
    {
      if (target.hasComponent<T>())
      {
        auto& comp = target.getComponent<T>();
        auto temp = source.getComponent<T>();
        comp = temp;
      }
      else
      {
        auto& comp = target.addComponent<T>();
        auto temp = source.getComponent<T>();
        comp = temp;
      }
    }
  }
  //----------------------------------------------------------------------------

  // Other helper functions.
  //----------------------------------------------------------------------------
  Entity
  createChildEntity(Entity entity, Shared<Scene> activeScene, const std::string &name = "New Entity")
  {
    if (!entity.hasComponent<ChildEntityComponent>())
    {
      // Add the child entity component.
      auto& children = entity.addComponent<ChildEntityComponent>();
      auto child = activeScene->createEntity(name);
      children.children.push_back(child);

      // Add the parent component to the child.
      child.addComponent<ParentEntityComponent>(entity);
      return child;
    }
    else
    {
      auto& children = entity.getComponent<ChildEntityComponent>();

      auto child = activeScene->createEntity(name);
      children.children.push_back(child);
      child.addComponent<ParentEntityComponent>(entity);
      return child;
    }
  }
  //----------------------------------------------------------------------------

  // Fixed selection bug for now -> removed the ability to deselect entities.
  // TODO: Re-add the ability to deselect entities.
  SceneGraphWindow::SceneGraphWindow(EditorLayer* parentLayer)
    : GuiWindow(parentLayer)
    , materialEditor(parentLayer)
    , selectedString("")
    , fileTargets(FileLoadTargets::TargetNone)
    , saveTargets(FileSaveTargets::TargetNone)
    , dirWidgetShader("./assets/shaders/widgets/lightWidget.glsl")
    , widgetWidth(0.0f)
    , selectedSubmesh(nullptr)
  {
    auto cSpec = Texture2D::getFloatColourParams();
    auto attachment = FBOAttachment(FBOTargetParam::Colour0, FBOTextureParam::Texture2D,
                                    cSpec.internal, cSpec.format, cSpec.dataType);
    this->dirBuffer = createShared<FrameBuffer>(512, 512);
    this->dirBuffer->attach(cSpec, attachment);
    this->dirBuffer->attachRenderBuffer();
    this->dirBuffer->setClearColour(glm::vec4(0.0f));

    this->sphere.load("./assets/.internal/sphere.fbx");
  }

  SceneGraphWindow::~SceneGraphWindow()
  { }

  void
  SceneGraphWindow::onImGuiRender(bool &isOpen, Shared<Scene> activeScene)
  {
    ImGui::Begin("Scene Graph", &isOpen);

    static bool openPropWindow = true;
    ImGui::Checkbox("Show Component Properties", &openPropWindow);

    activeScene->sceneECS.each([&](auto entityID)
    {
      // Fetch the entity and its tag component.
      Entity current = Entity(entityID, activeScene.get());

      if (!current.hasComponent<ParentEntityComponent>())
        this->drawEntityNode(current, activeScene);
    });

    // Right-click on blank space to create a new entity.
	if (ImGui::BeginPopupContextWindow(0, ImGuiPopupFlags_MouseButtonRight, false))
	{
      if (ImGui::MenuItem("New Model"))
      {
		auto model = activeScene->createEntity("New Model");
        model.addComponent<TransformComponent>();
        model.addComponent<RenderableComponent>();
      }

      if (ImGui::BeginMenu("New Light"))
      {
        if (ImGui::MenuItem("Directional Light"))
        {
          auto light = activeScene->createEntity("New Directional Light");
          light.addComponent<DirectionalLightComponent>();
          light.addComponent<TransformComponent>();
        }

        if (ImGui::MenuItem("Point Light"))
        {
          auto light = activeScene->createEntity("New Point Light");
          light.addComponent<PointLightComponent>();
          light.addComponent<TransformComponent>();
        }

        ImGui::EndMenu();
      }

      if (ImGui::MenuItem("New Empty Entity"))
	    activeScene->createEntity();

	  ImGui::EndPopup();
	}

    ImGui::End();

    if (openPropWindow)
      this->drawPropsWindow(openPropWindow, activeScene);

    if (this->materialEditor.isOpen)
      this->materialEditor.onImGuiRender(this->materialEditor.isOpen, activeScene);
  }

  void
  SceneGraphWindow::onUpdate(float dt, Shared<Scene> activeScene)
  { }

  void
  SceneGraphWindow::onEvent(Event &event)
  {
    switch(event.getType())
    {
      // Swap the selected entity to something else.
      case EventType::EntitySwapEvent:
      {
        auto entSwapEvent = *(static_cast<EntitySwapEvent*>(&event));

        auto entityID = entSwapEvent.getStoredEntity();
        auto entityParentScene = entSwapEvent.getStoredScene();
        if (entityID < 0)
          this->selectedEntity = Entity();
        else
          this->selectedEntity = Entity((entt::entity) entityID, entityParentScene);

        this->selectedSubmesh = nullptr;
        this->materialEditor.isOpen = false;
        this->materialEditor.setSelectedMaterial("");

        break;
      }

      // Process a file loading event. Using enum barriers to prevent files from
      // being improperly loaded when this window didn't dispatch the event.
      // TODO: Bitmask instead of enums?
      case EventType::LoadFileEvent:
      {
        if (!this->selectedEntity)
          return;

        auto loadEvent = *(static_cast<LoadFileEvent*>(&event));
        auto& path = loadEvent.getAbsPath();
        auto& name = loadEvent.getFileName();

        switch (this->fileTargets)
        {
          case FileLoadTargets::TargetModel:
          {
            auto modelAssets = AssetManager<Model>::getManager();

            // If it already has a mesh component, remove it and add a new one.
            if (this->selectedEntity.hasComponent<RenderableComponent>())
              this->selectedEntity.removeComponent<RenderableComponent>();

            auto& renderable = this->selectedEntity.addComponent<RenderableComponent>(name);
            AsyncLoading::asyncLoadModel(path, name, this->selectedEntity, this->selectedEntity);

            break;
          }

          case FileLoadTargets::TargetMaterial:
          {
            if (this->selectedSubmesh)
            {
              if (this->selectedEntity.hasComponent<RenderableComponent>())
              {
                YAMLSerialization::deserializeMaterial(path, name, true);
                auto& submeshName = this->selectedSubmesh->getName();
                auto& renderable = this->selectedEntity.getComponent<RenderableComponent>();
                auto& materials = renderable.materials;
                materials.swapMaterial(submeshName, name);
              }
            }

            break;
          }

          default: break;
        }

        this->fileTargets = FileLoadTargets::TargetNone;
        break;
      }

      case EventType::SaveFileEvent:
      {
        if (!this->selectedEntity)
          return;

        auto saveEvent = *(static_cast<SaveFileEvent*>(&event));
        auto& path = saveEvent.getAbsPath();
        auto& name = saveEvent.getFileName();

        switch (this->saveTargets)
        {
          case FileSaveTargets::TargetPrefab:
          {
            auto fabName = name.substr(0, name.find_last_of('.'));

            this->selectedEntity.addComponent<PrefabComponent>(fabName, path);
            YAMLSerialization::serializePrefab(this->selectedEntity, path, fabName);
            break;
          }

          case FileSaveTargets::TargetMaterial:
          {
            if (this->selectedSubmesh)
            {
              if (this->selectedEntity.hasComponent<RenderableComponent>())
              {
                auto& renderable = this->selectedEntity.getComponent<RenderableComponent>();
                auto& materials = renderable.materials;
                auto handle = materials.getMaterialHandle(this->selectedSubmesh->getName());
                YAMLSerialization::serializeMaterial(handle, path);
              }
            }

            break;
          }
        }
        this->saveTargets = FileSaveTargets::TargetNone;

        break;
      }

      default: break;
    }

    this->materialEditor.onEvent(event);
  }

  void
  SceneGraphWindow::drawEntityNode(Entity entity, Shared<Scene> activeScene)
  {
    auto& nameTag = entity.getComponent<NameComponent>().name;

    // Draw the entity + components as a treenode.
    ImGuiTreeNodeFlags flags = ((this->selectedEntity == entity) ? ImGuiTreeNodeFlags_Selected : 0);
    flags |= ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow
          | ImGuiTreeNodeFlags_OpenOnDoubleClick;

    if (entity.hasComponent<PrefabComponent>())
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.54f, 0.11f, 0.0f, 1.0f));

    bool opened = ImGui::TreeNodeEx((void*)(uint64_t)(uint32_t)entity, flags, nameTag.c_str());

    // Set the new selected entity.
    if (ImGui::IsItemClicked())
    {
      EventDispatcher* dispatcher = EventDispatcher::getInstance();
      dispatcher->queueEvent(new EntitySwapEvent(entity, activeScene.get()));

      this->selectedEntity = entity;
    }

    // Menu with entity properties. Allows the addition and deletion of
    // components, copying of the entity and deletion of the entity.
    if (ImGui::BeginPopupContextItem())
    {
      if (ImGui::BeginMenu("Attach Component"))
      {
        // Add various components.
        drawComponentAdd<TransformComponent>("Transform Component", entity);
        drawComponentAdd<RenderableComponent>("Renderable Component", entity);
        drawComponentAdd<CameraComponent>("Camera Component", entity);
        drawComponentAdd<SkyAtmosphereComponent>("Sky and Atmosphere Component", entity);

        if (ImGui::BeginMenu("Light Components"))
        {
          drawComponentAdd<DirectionalLightComponent>("Directional Light Component", entity);
          drawComponentAdd<PointLightComponent>("Point Light Component", entity);

          ImGui::EndMenu();
        }

        ImGui::EndMenu();
      }

      if (ImGui::BeginMenu("Remove Component"))
      {
        // Remove various components.
        drawComponentRemove<TransformComponent>("Transform Component", entity);
        drawComponentRemove<RenderableComponent>("Renderable Component", entity);
        drawComponentRemove<CameraComponent>("Camera Component", entity);
        drawComponentRemove<SkyAtmosphereComponent>("Sky and Atmosphere Component", entity);

        if (ImGui::BeginMenu("Light Components"))
        {
          drawComponentRemove<DirectionalLightComponent>("Directional Light Component", entity);
          drawComponentRemove<PointLightComponent>("Point Light Component", entity);

          ImGui::EndMenu();
        }

        ImGui::EndMenu();
      }

      if (ImGui::BeginMenu("Add Child Entity"))
      {
        if (ImGui::MenuItem("New Model"))
        {
  	      auto model = createChildEntity(entity, activeScene, "New Model");
          model.addComponent<TransformComponent>();
          model.addComponent<RenderableComponent>();
        }

        if (ImGui::BeginMenu("New Light"))
        {
          if (ImGui::MenuItem("Directional Light"))
          {
            auto light = createChildEntity(entity, activeScene, "New Directional Light");
            light.addComponent<DirectionalLightComponent>();
            light.addComponent<TransformComponent>();
          }

          if (ImGui::MenuItem("Point Light"))
          {
            auto light = createChildEntity(entity, activeScene, "New Point Light");
            light.addComponent<PointLightComponent>();
            light.addComponent<TransformComponent>();
          }

          ImGui::EndMenu();
        }

        if (ImGui::MenuItem("New Empty Entity"))
  	      createChildEntity(entity, activeScene);

        ImGui::EndMenu();
      }

      if (ImGui::MenuItem("Create Copy of Entity"))
      {
        auto newEntity = activeScene->createEntity();

        copyComponent<NameComponent>(entity, newEntity);
        copyComponent<TransformComponent>(entity, newEntity);
        copyComponent<RenderableComponent>(entity, newEntity);
        copyComponent<DirectionalLightComponent>(entity, newEntity);
        copyComponent<PointLightComponent>(entity, newEntity);
        copyComponent<SkyAtmosphereComponent>(entity, newEntity);
      }

      if (ImGui::MenuItem("Register as PreFab"))
      {
        EventDispatcher* dispatcher = EventDispatcher::getInstance();
        dispatcher->queueEvent(new OpenDialogueEvent(DialogueEventType::FileSave, ".sfab"));

        this->saveTargets = FileSaveTargets::TargetPrefab;

        dispatcher->queueEvent(new EntitySwapEvent(entity, activeScene.get()));

        this->selectedEntity = entity;
      }

      // Check to see if we should delete the entity.
      if (ImGui::MenuItem("Delete Entity"))
      {
        EventDispatcher* dispatcher = EventDispatcher::getInstance();
        dispatcher->queueEvent(new EntitySwapEvent(-1, activeScene.get()));
        dispatcher->queueEvent(new EntityDeleteEvent(entity, activeScene.get()));
      }

      ImGui::EndPopup();
    }

    // Open the list of attached components.
    if (opened)
    {
      this->drawComponentNodes(entity, activeScene);
      ImGui::TreePop();
    }

    if (entity.hasComponent<PrefabComponent>())
      ImGui::PopStyleColor();
  }

  // Function for drawing sub-entities and components of an entity.
  void
  SceneGraphWindow::drawComponentNodes(Entity entity, Shared<Scene> activeScene)
  {
    if (!entity)
      return;

    ImGuiTreeNodeFlags leafFlag = ImGuiTreeNodeFlags_Leaf
                                | ImGuiTreeNodeFlags_NoTreePushOnOpen;

    // Display the child entities
    if (entity.hasComponent<ChildEntityComponent>())
    {
      auto& children = entity.getComponent<ChildEntityComponent>();
      for (auto& child : children.children)
        drawEntityNode(child, activeScene);
    }

    // Display components here.
    if (entity.hasComponent<TransformComponent>())
    {
      ImGui::TreeNodeEx("Transform Componenet", leafFlag);
    }
    if (entity.hasComponent<RenderableComponent>())
    {
      ImGui::TreeNodeEx("Renderable Componenet", leafFlag);
    }
    if (entity.hasComponent<CameraComponent>())
    {
      ImGui::TreeNodeEx("Camera Componenet", leafFlag);
    }
    if (entity.hasComponent<DirectionalLightComponent>())
    {
      ImGui::TreeNodeEx("Directional Light Componenet", leafFlag);
    }
    if (entity.hasComponent<PointLightComponent>())
    {
      ImGui::TreeNodeEx("Point Light Componenet", leafFlag);
    }
    if (entity.hasComponent<SkyAtmosphereComponent>())
    {
      ImGui::TreeNodeEx("Sky and Atmosphere Component", leafFlag);
    }
  }

  // The property panel for an entity.
  void
  SceneGraphWindow::drawPropsWindow(bool &isOpen, Shared<Scene> activeScene)
  {
    ImGui::Begin("Components", &isOpen);
    if (this->selectedEntity)
    {
      auto& name = this->selectedEntity.getComponent<NameComponent>().name;
      auto& description = this->selectedEntity.getComponent<NameComponent>().description;

      char nameBuffer[256];
      memset(nameBuffer, 0, sizeof(nameBuffer));
      std::strncpy(nameBuffer, name.c_str(), sizeof(nameBuffer));

      char descBuffer[256];
      memset(descBuffer, 0, sizeof(descBuffer));
      std::strncpy(descBuffer, description.c_str(), sizeof(descBuffer));

      ImGui::Text("Name:");
      if (ImGui::InputText("##name", nameBuffer, sizeof(nameBuffer)))
        name = std::string(nameBuffer);

      ImGui::Text("Description:");
      if (ImGui::InputText("##desc", descBuffer, sizeof(descBuffer)))
        description = std::string(descBuffer);

      if (this->selectedEntity.hasComponent<PrefabComponent>())
      {
        auto& pfComponent = this->selectedEntity.getComponent<PrefabComponent>();

        ImGui::Text(" ");
        ImGui::Text("Prefab Settings");
        ImGui::Separator();
        ImGui::Checkbox("Synch Prefab", &pfComponent.synch);

        auto tempID = pfComponent.prefabID;
        auto tempPath = pfComponent.prefabPath;

        ImGui::Text((std::string("Prefab ID: ") + tempID).c_str());
        ImGui::Text((std::string("Prefab path: ") + tempPath).c_str());
        if (pfComponent.synch)
        {
          if (ImGui::Button("Push Prefab Settings"))
          {
            YAMLSerialization::serializePrefab(this->selectedEntity, pfComponent.prefabPath, pfComponent.prefabID);

            auto autoPrefabs = activeScene->sceneECS.view<PrefabComponent>();
            for (auto entity : autoPrefabs)
            {
              auto prefab = autoPrefabs.get<PrefabComponent>(entity);
              if (prefab.synch && prefab.prefabID == tempID)
              {
                EventDispatcher* dispatcher = EventDispatcher::getInstance();
                dispatcher->queueEvent(new EntitySwapEvent(-1, activeScene.get()));
                dispatcher->queueEvent(new EntityDeleteEvent((int) entity, activeScene.get()));

                YAMLSerialization::deserializePrefab(activeScene, tempPath);
              }
            }
          }
        }
      }

      drawComponentProperties<TransformComponent>("Transform Component",
        this->selectedEntity, [](auto& component)
      {
        Styles::drawVec3Controls("Translation", glm::vec3(0.0f), component.translation);
        glm::vec3 tEulerRotation = glm::degrees(component.rotation);
        Styles::drawVec3Controls("Rotation", glm::vec3(0.0f), tEulerRotation);
        component.rotation = glm::radians(tEulerRotation);
        Styles::drawVec3Controls("Scale", glm::vec3(1.0f), component.scale);
      });

      drawComponentProperties<RenderableComponent>("Renderable Component",
        this->selectedEntity, [this](auto& component)
      {
        Model* componentModel = component;
        char nameBuffer[256];
        memset(nameBuffer, 0, sizeof(nameBuffer));
        if (componentModel)
          std::strncpy(nameBuffer, componentModel->getFilepath().c_str(), sizeof(nameBuffer));

        ImGui::Text("Mesh Information");
        ImGui::Separator();
        if (ImGui::Button(ICON_FA_FOLDER_OPEN))
        {
          EventDispatcher::getInstance()->queueEvent(new OpenDialogueEvent(DialogueEventType::FileOpen,
                                                     ".obj,.FBX,.fbx"));

          this->fileTargets = FileLoadTargets::TargetModel;
        }

        ImGui::SameLine();
        ImGui::InputText("##modelPath", nameBuffer, sizeof(nameBuffer), ImGuiInputTextFlags_ReadOnly);
        this->loadDNDAsset();
        ImGui::Button("Open Model Viewer");

        ImGui::Text("");
        ImGui::Separator();
        ImGui::Text("Materials");
        if (this->selectedSubmesh && componentModel)
        {
          if (ImGui::BeginCombo("##sceneGraphSelectedSubmesh", this->selectedSubmesh->getName().c_str()))
          {
            for (auto& submesh : componentModel->getSubmeshes())
            {
              bool isSelected = (&submesh == this->selectedSubmesh);

              if (ImGui::Selectable(submesh.getName().c_str(), isSelected))
                this->selectedSubmesh = (&submesh);

              if (isSelected)
                ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
          }

          std::string submeshName = this->selectedSubmesh->getName();
          auto materialHandle = component.materials.getMaterialHandle(submeshName);
          auto material = component.materials.getMaterial(this->selectedSubmesh->getName());

          if (material)
          {
            ImGui::PushID("MaterialPreview");
            if (ImGui::ImageButton((ImTextureID) (unsigned long) material->getSampler2D("albedoMap")->getID(),
                                   ImVec2(48, 48), ImVec2(0, 1), ImVec2(1, 0)))
            {

            }
            this->loadDNDAsset(this->selectedSubmesh->getName());
            ImGui::PopID();

            if (ImGui::Button(ICON_FA_COG))
            {
              this->materialEditor.isOpen = true;
              this->materialEditor.setSelectedMaterial(materialHandle);
            }

            static bool makingNewmaterial = false;
            static std::string newMaterialname = "";
            ImGui::SameLine();
            if (ImGui::Button(ICON_FA_FILE))
            {
              newMaterialname = "";
              makingNewmaterial = true;
            }

            if (makingNewmaterial)
            {
              char nameBuffer[256];
              memset(nameBuffer, 0, sizeof(nameBuffer));
              std::strncpy(nameBuffer, newMaterialname.c_str(), sizeof(nameBuffer));

              ImGui::Begin("New Material", &makingNewmaterial, ImGuiWindowFlags_AlwaysAutoResize);
              if (ImGui::InputText("Name##newMaterialName", nameBuffer, sizeof(nameBuffer)))
              {
                newMaterialname = nameBuffer;
              }

              if (ImGui::Button("Create##newMaterial"))
              {
                auto newMat = new Material();
                AssetManager<Material>::getManager()->attachAsset(newMaterialname, newMat);
                component.materials.swapMaterial(submeshName, newMaterialname);
                makingNewmaterial = false;
              }

              ImGui::End();
            }

            if (ImGui::Button(ICON_FA_FOLDER_O))
            {
              EventDispatcher::getInstance()->queueEvent(new OpenDialogueEvent(DialogueEventType::FileOpen,
                                                         ".srmat"));
              this->fileTargets = FileLoadTargets::TargetMaterial;
            }

            ImGui::SameLine();
            if (ImGui::Button(ICON_FA_FLOPPY_O))
            {
              EventDispatcher::getInstance()->queueEvent(new OpenDialogueEvent(DialogueEventType::FileSave,
                                                         ".srmat"));
              this->saveTargets = FileSaveTargets::TargetMaterial;
            }
          }
        }
        else if (componentModel)
        {
          if (ImGui::BeginCombo("##sceneGraphSelectedSubmesh", ""))
          {
            for (auto& submesh : componentModel->getSubmeshes())
            {
              bool isSelected = (&submesh == this->selectedSubmesh);

              if (ImGui::Selectable(submesh.getName().c_str(), isSelected))
                this->selectedSubmesh = (&submesh);

              if (isSelected)
                ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
          }
        }

        if (componentModel)
        {
          if (componentModel->getAnimations().size() > 0)
          {
            ImGui::Text("");
            ImGui::Separator();
            ImGui::Text("Animations");
            auto storedAnimation = component.animator.getStoredAnimation();
            if (storedAnimation)
            {
              if (ImGui::BeginCombo("##animator", storedAnimation->getName().c_str()))
              {
                for (auto& animation : componentModel->getAnimations())
                {
                  bool isSelected = (&animation == storedAnimation);
          
                  if (ImGui::Selectable(animation.getName().c_str(), isSelected))
                    component.animator.setAnimation(&animation, component.meshName);
          
                  if (isSelected)
                    ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
              }
          
              ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 4.0f));
              if (component.animator.isPaused())
              {
                if (ImGui::Button(ICON_FA_PLAY))
                  component.animator.startAnimation();
              }
              else
              {
                ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
                ImGui::Button(ICON_FA_PLAY);
                ImGui::PopItemFlag();
                ImGui::PopStyleVar();
              }
          
              ImGui::SameLine();
              if (!component.animator.isPaused())
              {
                if (ImGui::Button(ICON_FA_PAUSE))
                  component.animator.pauseAnimation();
              }
              else
              {
                ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
                ImGui::Button(ICON_FA_PAUSE);
                ImGui::PopItemFlag();
                ImGui::PopStyleVar();
              }
          
              ImGui::SameLine();
              if (component.animator.isAnimating())
              {
                if (ImGui::Button(ICON_FA_STOP))
                  component.animator.stopAnimation();
              }
              else
              {
                ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
                ImGui::Button(ICON_FA_STOP);
                ImGui::PopItemFlag();
                ImGui::PopStyleVar();
              }
              ImGui::PopStyleVar();
          
              if (ImGui::SliderFloat("##AnimationTime", &component.animator.getAnimationTime(), 
                                     0.0f, storedAnimation->getDuration()))
                component.animator.setScrubbing();
            }
            else
            {
              if (ImGui::BeginCombo("##animator", ""))
              {
                for (auto& animation : componentModel->getAnimations())
                {
                  bool isSelected = (&animation == storedAnimation);
          
                  if (ImGui::Selectable(animation.getName().c_str(), isSelected))
                    component.animator.setAnimation(&animation, component.meshName);
          
                  if (isSelected)
                    ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
              }
            }
          }
        }
      });

      drawComponentProperties<CameraComponent>("Camera Component",
        this->selectedEntity, [this, activeScene](auto& component)
      {
        auto& camera = component.entCamera;

        float degFOV = glm::degrees(camera.fov);

        ImGui::Checkbox("Primary Camera", &component.isPrimary);
        Styles::drawFloatControl("Near", 0.1f, camera.near, 0.0f, 0.1f, 0.1f, 100.0f);
        Styles::drawFloatControl("Far", 30.0f, camera.far, 0.0f, 0.1f, 30.0f, 500.0f);
        Styles::drawFloatControl("FOV", 45.0f, degFOV, 0.0f, 0.1f, 30.0f, 180.0f);

        camera.fov = glm::radians(degFOV);
      });

      drawComponentProperties<SkyAtmosphereComponent>("Sky-Atmosphere Component", 
                                                      this->selectedEntity, 
                                                      [this](auto& component)
      {
        ImGui::Text("Renderer handle: %i", component.handle);
        ImGui::Checkbox("Use Primary Light", &component.usePrimaryLight);
        ImGui::Indent();
        if (ImGui::CollapsingHeader("Scattering Parameters##UE4Atmo"))
        {
          Styles::drawFloatControl("Rayleigh Density", 8.0f, component.rayleighScat.w,
                                   0.0f, 0.01f, 0.0f, 10.0f);

          auto rayleighScattering = glm::vec3(component.rayleighScat);
          Styles::drawVec3Controls("Rayleigh Scattering", glm::vec3(5.802f, 13.558f, 33.1f),
                                   rayleighScattering, 0.0f, 0.1f,
                                   0.0f, 100.0f);
          component.rayleighScat.x = rayleighScattering.x;
          component.rayleighScat.y = rayleighScattering.y;
          component.rayleighScat.z = rayleighScattering.z;

          auto rayleighAbsorption = glm::vec3(component.rayleighAbs);
          Styles::drawVec3Controls("Rayleigh Absorption", glm::vec3(0.0f),
                                   rayleighAbsorption, 0.0f, 0.1f,
                                   0.0f, 100.0f);
          component.rayleighAbs.x = rayleighAbsorption.x;
          component.rayleighAbs.y = rayleighAbsorption.y;
          component.rayleighAbs.z = rayleighAbsorption.z;
          component.rayleighAbs.w = component.rayleighScat.w;

          Styles::drawFloatControl("Mie Density", 1.2f, component.mieScat.w,
                                   0.0f, 0.01f, 0.0f, 10.0f);

          auto mieScat = glm::vec3(component.mieScat);
          Styles::drawVec3Controls("Mie Scattering", glm::vec3(3.996f), mieScat,
                                   0.0f, 0.1f, 0.0f, 100.0f);
          component.mieScat.x = mieScat.x;
          component.mieScat.y = mieScat.y;
          component.mieScat.z = mieScat.z;

          auto mieAbs = glm::vec3(component.mieAbs);
          Styles::drawVec3Controls("Mie Absorption", glm::vec3(4.4f), mieAbs,
                                   0.0f, 0.1f, 0.0f, 100.0f);
          component.mieAbs.x = mieAbs.x;
          component.mieAbs.y = mieAbs.y;
          component.mieAbs.z = mieAbs.z;
          component.mieAbs.w = component.mieScat.w;

          Styles::drawFloatControl("Ozone Strength", 0.002f, component.ozoneAbs.w,
                                   0.0f, 0.001f, 0.0f, 1.0f);

          auto ozone = glm::vec3(component.ozoneAbs);
          Styles::drawVec3Controls("Ozone Absorption", glm::vec3(0.650f, 1.881f, 0.085f),
                                   ozone, 0.0f, 0.1f,  0.0f, 100.0f);
          component.ozoneAbs.x = ozone.x;
          component.ozoneAbs.y = ozone.y;
          component.ozoneAbs.z = ozone.z;
        }

        if (ImGui::CollapsingHeader("Planetary Parameters##UE4Atmo"))
        {
          ImGui::ColorEdit3("Planet Albedo", &component.planetAlbedo.x);
          float planetRadiusKM = component.planetAtmRadius.x * 1000.0f;
          float atmoRadiusKM = component.planetAtmRadius.y * 1000.0f;
          Styles::drawFloatControl("Planet Radius (Km)",
                                   6360.0f, planetRadiusKM,
                                   0.0f, 1.0f, 0.0f, atmoRadiusKM);
          Styles::drawFloatControl("Atmosphere Radius (Km)",
                                   6460.0f, atmoRadiusKM,
                                   0.0f, 1.0f, planetRadiusKM, 10000.0f);
          component.planetAtmRadius.x = planetRadiusKM / 1000.0f;
          component.planetAtmRadius.y = atmoRadiusKM / 1000.0f;
        }
        ImGui::Unindent();
      });

      drawComponentProperties<DirectionalLightComponent>("Directional Light Component",
        this->selectedEntity, [this, activeScene](auto& component)
      {
        ImGui::PushID("DirectionalLight");

        bool isPrimaryLight = component.light.primaryLight;
        ImGui::Checkbox("Primary Light", &component.light.primaryLight);
        if (component.light.primaryLight && !isPrimaryLight)
        {
          auto dirLight = activeScene->sceneECS.view<DirectionalLightComponent>();

          for (auto entity : dirLight)
          {
            auto& directional = dirLight.get<DirectionalLightComponent>(entity);
            if (this->selectedEntity != entity && directional.light.primaryLight)
              directional.light.primaryLight;
          }
        }

        ImGui::Checkbox("Cast Shadows", &component.light.castShadows);
        ImGui::ColorEdit3("Colour", &(component.light.colourIntensity.r));
        Styles::drawFloatControl("Intensity", 0.0f, component.light.colourIntensity.a,
                                 0.0f, 0.01f, 0.0f, 100.0f);
        ImGui::PopID();

        this->drawDirectionalWidget();
      });

      drawComponentProperties<PointLightComponent>("Point Light Component",
                                                   this->selectedEntity, 
                                                   [this](auto& component)
      {
        ImGui::PushID("PointLight");
        ImGui::Checkbox("Cast Shadows", &component.castShadows);
        ImGui::ColorEdit3("Colour", &component.light.colourIntensity.r);
        Styles::drawFloatControl("Radius", 0.0f, component.light.positionRadius.w, 0.0f, 0.1f, 0.0f, 100.0f);
        Styles::drawFloatControl("Intensity", 0.0f, component.light.colourIntensity.w,
                                 0.0f, 0.01f, 0.0f, 100.0f);
        ImGui::PopID();
      });
    }

    ImGui::End();
  }

  // Draw a widget to make controlling directional lights more intuitive.
  void
  SceneGraphWindow::drawDirectionalWidget()
  {
    if (this->selectedEntity.hasComponent<TransformComponent>() &&
        this->selectedEntity.hasComponent<DirectionalLightComponent>())
    {
      auto model = glm::mat4(1.0);
      auto viewPos = glm::vec3(2.0f);
      auto viewDir = glm::normalize(glm::vec3(0.0f, 0.0f, 0.0f) - viewPos);
      auto view = glm::lookAt(viewPos, viewPos + viewDir, glm::vec3(0.0f, 1.0f, 0.0f));
      auto projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
      auto mVP = projection * view * model;

      auto& transform = this->selectedEntity.getComponent<TransformComponent>();
      auto& light = this->selectedEntity.getComponent<DirectionalLightComponent>();

      auto lightDir = -1.0f * glm::vec3(glm::toMat4(glm::quat(transform.rotation))
                            * glm::vec4(0.0f, -1.0f, 0.0f, 0.0f));

      this->dirBuffer->clear();
      this->dirBuffer->bind();
      this->dirBuffer->setViewport();

      this->dirWidgetShader.addUniformMatrix("mVP", mVP, false);
      this->dirWidgetShader.addUniformMatrix("normalMat", glm::transpose(glm::inverse(glm::mat3(model))), false);
      this->dirWidgetShader.addUniformMatrix("model", model, false);
      this->dirWidgetShader.addUniformVector("lDirection", lightDir);

      for (auto& submesh : this->sphere.getSubmeshes())
      {
        if (submesh.hasVAO())
          Renderer3D::draw(submesh.getVAO(), &this->dirWidgetShader);
        else
        {
          submesh.generateVAO();
          if (submesh.hasVAO())
            Renderer3D::draw(submesh.getVAO(), &this->dirWidgetShader);
        }
      }

      this->dirBuffer->unbind();

      this->widgetWidth = ImGui::GetWindowSize().x * 0.75f;
      ImGui::BeginChild("LightDirection", ImVec2(this->widgetWidth, this->widgetWidth));
      ImGui::Image((ImTextureID) (unsigned long) this->dirBuffer->getAttachID(FBOTargetParam::Colour0),
                   ImVec2(this->widgetWidth, this->widgetWidth), ImVec2(0, 1), ImVec2(1, 0));

      // ImGuizmo boilerplate. Prepare the drawing context and set the window to
      // draw the gizmos to.
      ImGuizmo::SetOrthographic(false);
      ImGuizmo::SetDrawlist();

      auto windowMin = ImGui::GetWindowContentRegionMin();
      auto windowMax = ImGui::GetWindowContentRegionMax();
      auto windowOffset = ImGui::GetWindowPos();
      ImVec2 bounds[2];
      bounds[0] = ImVec2(windowMin.x + windowOffset.x,
                         windowMin.y + windowOffset.y);
      bounds[1] = ImVec2(windowMax.x + windowOffset.x,
                         windowMax.y + windowOffset.y);

      ImGuizmo::SetRect(bounds[0].x - 100.0f, bounds[0].y - 100.0f,
                        (bounds[1].x - bounds[0].x) + 200.0f,
                        (bounds[1].y - bounds[0].y) + 200.0f);

      glm::mat4 transformMatrix = transform;

      // Manipulate the matrix. TODO: Add snapping.
      ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(projection),
                           ImGuizmo::ROTATE, ImGuizmo::WORLD,
                           glm::value_ptr(transformMatrix), nullptr, nullptr);

      if (ImGuizmo::IsUsing())
      {
        glm::vec3 translation, scale, skew;
        glm::vec4 perspective;
        glm::quat rotation;
        glm::decompose(transformMatrix, scale, rotation, translation, skew, perspective);

        transform.translation = translation;
        transform.rotation = glm::eulerAngles(rotation);
        transform.scale = scale;
      }

      ImGui::EndChild();
    }
  }

  void
  SceneGraphWindow::loadDNDAsset()
  {
    if (!this->selectedEntity)
      return;

    if (ImGui::BeginDragDropTarget())
    {
      if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH"))
      {
        auto modelAssets = AssetManager<Model>::getManager();

        std::string filepath = (char*) payload->Data;
        std::string filename = filepath.substr(filepath.find_last_of('/') + 1);
        std::string filetype = filename.substr(filename.find_last_of('.'));

        // Attach a mesh component.
        if (filetype == ".obj" || filetype == ".FBX" || filetype == ".fbx"
            || filetype == ".blend" || filetype == ".gltf" || filetype == ".glb"
            || filetype == ".dae")
        {
          // If it already has a mesh component, remove it and add a new one.
          // Otherwise just add a component.
          if (this->selectedEntity.hasComponent<RenderableComponent>())
            this->selectedEntity.removeComponent<RenderableComponent>();

          auto& renderable = this->selectedEntity.addComponent<RenderableComponent>(filename);
          AsyncLoading::asyncLoadModel(filepath, filename, this->selectedEntity, this->selectedEntity);
        }
      }

      ImGui::EndDragDropTarget();
    }
  }

  void
  SceneGraphWindow::loadDNDAsset(const std::string &submeshName)
  {
    if (!this->selectedEntity)
      return;

    if (ImGui::BeginDragDropTarget())
    {
      if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH"))
      {
        auto materialAssets = AssetManager<Material>::getManager();

        std::string filepath = (char*) payload->Data;
        std::string filename = filepath.substr(filepath.find_last_of('/') + 1);
        std::string filetype = filename.substr(filename.find_last_of('.'));

        if (filetype == ".smtl")
        {
          AssetHandle handle;
          if (YAMLSerialization::deserializeMaterial(filepath, handle))
          {
            materialAssets->getAsset(handle)->getFilepath() = filepath;

            auto& rComponent = this->selectedEntity.getComponent<RenderableComponent>();
            rComponent.materials.swapMaterial(submeshName, handle);
          }
        }
      }

      ImGui::EndDragDropTarget();
    }
  }
}
