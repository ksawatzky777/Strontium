#include "Serialization/YamlSerialization.h"

// Project includes.
#include "Scenes/Components.h"
#include "Scenes/Entity.h"
#include "Graphics/Renderer.h"
#include "Utils/AsyncAssetLoading.h"

// YAML includes.
#include "yaml-cpp/yaml.h"

namespace YAML
{
  template <>
	struct convert<glm::vec2>
	{
		static Node encode(const glm::vec2& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);

			node.SetStyle(EmitterStyle::Flow);

			return node;
		}

		static bool decode(const Node& node, glm::vec2& rhs)
		{
			if (!node.IsSequence() || node.size() != 2)
				return false;

			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();

			return true;
		}
	};

  template <>
	struct convert<glm::vec3>
	{
		static Node encode(const glm::vec3& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.push_back(rhs.z);

			node.SetStyle(EmitterStyle::Flow);

			return node;
		}

		static bool decode(const Node& node, glm::vec3& rhs)
		{
			if (!node.IsSequence() || node.size() != 3)
				return false;

			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			rhs.z = node[2].as<float>();

			return true;
		}
	};

	template <>
	struct convert<glm::vec4>
	{
		static Node encode(const glm::vec4& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.push_back(rhs.z);
			node.push_back(rhs.w);

			node.SetStyle(EmitterStyle::Flow);

			return node;
		}

		static bool decode(const Node& node, glm::vec4& rhs)
		{
			if (!node.IsSequence() || node.size() != 4)
				return false;

			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			rhs.z = node[2].as<float>();
			rhs.w = node[3].as<float>();

			return true;
		}
	};
}

namespace Strontium
{
  namespace YAMLSerialization
  {
    YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec2& v)
    {
    	out << YAML::Flow;
    	out << YAML::BeginSeq << v.x << v.y << YAML::EndSeq;
    	return out;
    }

    YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec3& v)
    {
    	out << YAML::Flow;
    	out << YAML::BeginSeq << v.x << v.y << v.z << YAML::EndSeq;
    	return out;
    }

    YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec4& v)
    {
    	out << YAML::Flow;
    	out << YAML::BeginSeq << v.x << v.y << v.z << v.w << YAML::EndSeq;
    	return out;
    }

    void
    serializeMaterial(YAML::Emitter &out, const AssetHandle &materialHandle, bool override = false)
    {
      auto textureCache = AssetManager<Texture2D>::getManager();
      auto materialAssets = AssetManager<Material>::getManager();

      auto material = materialAssets->getAsset(materialHandle);

      out << YAML::BeginMap;

      // Save the material name.
      out << YAML::Key << "MaterialName" << YAML::Value << materialHandle;
      if (material->getFilepath() != "" && override)
      {
        out << YAML::Key << "MaterialPath" << YAML::Value << material->getFilepath();
        out << YAML::EndMap;
        return;
      }

      // Save the material type.
      if (material->getType() == MaterialType::PBR)
        out << YAML::Key << "MaterialType" << YAML::Value << "pbr_shader";
      else
        out << YAML::Key << "MaterialType" << YAML::Value << "unknown_shader";

      out << YAML::Key << "Floats";
      out << YAML::BeginSeq;
      for (auto& uFloat : material->getFloats())
      {
        out << YAML::BeginMap;
        out << YAML::Key << "UniformName" << YAML::Value << uFloat.first;
        out << YAML::Key << "UniformValue" << YAML::Value << uFloat.second;
        out << YAML::EndMap;
      }
      out << YAML::EndSeq;

      out << YAML::Key << "Vec2s";
      out << YAML::BeginSeq;
      for (auto& uVec2 : material->getVec2s())
      {
        out << YAML::BeginMap;
        out << YAML::Key << "UniformName" << YAML::Value << uVec2.first;
        out << YAML::Key << "UniformValue" << YAML::Value << uVec2.second;
        out << YAML::EndMap;
      }
      out << YAML::EndSeq;

      out << YAML::Key << "Vec3s";
      out << YAML::BeginSeq;
      for (auto& uVec3 : material->getVec3s())
      {
        out << YAML::BeginMap;
        out << YAML::Key << "UniformName" << YAML::Value << uVec3.first;
        out << YAML::Key << "UniformValue" << YAML::Value << uVec3.second;
        out << YAML::EndMap;
      }
      out << YAML::EndSeq;

      out << YAML::Key << "Sampler2Ds";
      out << YAML::BeginSeq;
      for (auto& uSampler2D : material->getSampler2Ds())
      {
        out << YAML::BeginMap;
        out << YAML::Key << "SamplerName" << YAML::Value << uSampler2D.first;
        out << YAML::Key << "SamplerHandle" << YAML::Value << uSampler2D.second;
        out << YAML::Key << "ImagePath" << YAML::Value << textureCache->getAsset(uSampler2D.second)->getFilepath();
        out << YAML::EndMap;
      }
      out << YAML::EndSeq;

      out << YAML::EndMap;
    }

    void serializeMaterial(const AssetHandle &materialHandle,
                           const std::string &filepath)
    {
      YAML::Emitter out;
      serializeMaterial(out, materialHandle);

      std::ofstream output(filepath, std::ofstream::trunc | std::ofstream::out);
      output << out.c_str();
      output.close();
    }

    void
    serializeEntity(YAML::Emitter &out, Entity entity)
    {
      if (!entity)
        return;

      auto materialAssets = AssetManager<Material>::getManager();

      // Serialize the entity.
      out << YAML::BeginMap;
      out << YAML::Key << "EntityID" << YAML::Value << (uint) entity;

      if (entity.hasComponent<NameComponent>())
      {
        out << YAML::Key << "NameComponent";
        out << YAML::BeginMap;

        auto& component = entity.getComponent<NameComponent>();
        out << YAML::Key << "Name" << YAML::Value << component.name;
        out << YAML::Key << "Description" << YAML::Value << component.description;

        out << YAML::EndMap;
      }

      if (entity.hasComponent<PrefabComponent>())
      {
        out << YAML::Key << "PrefabComponent";
        out << YAML::BeginMap;

        auto& component = entity.getComponent<PrefabComponent>();
        out << YAML::Key << "Synch" << YAML::Value << component.synch;
        out << YAML::Key << "PreFabID" << YAML::Value << component.prefabID;
        out << YAML::Key << "PreFabPath" << YAML::Value << component.prefabPath;

        out << YAML::EndMap;
      }

      if (entity.hasComponent<ChildEntityComponent>())
      {
        out << YAML::Key << "ChildEntities" << YAML::Value << YAML::BeginSeq;

        auto& children = entity.getComponent<ChildEntityComponent>().children;
        for (auto& child : children)
          serializeEntity(out, child);

        out << YAML::EndSeq;
      }

      if (entity.hasComponent<TransformComponent>())
      {
        out << YAML::Key << "TransformComponent";
        out << YAML::BeginMap;

        auto& component = entity.getComponent<TransformComponent>();
        out << YAML::Key << "Translation" << YAML::Value << component.translation;
        out << YAML::Key << "Rotation" << YAML::Value << component.rotation;
        out << YAML::Key << "Scale" << YAML::Value << component.scale;

        out << YAML::EndMap;
      }

      if (entity.hasComponent<RenderableComponent>())
      {
        auto modelAssets = AssetManager<Model>::getManager();

        auto& component = entity.getComponent<RenderableComponent>();
        auto model = modelAssets->getAsset(component.meshName);
        auto& material = component.materials;

        if (model != nullptr)
        {
          out << YAML::Key << "RenderableComponent";
          out << YAML::BeginMap;

          // Serialize the model path and name.
          out << YAML::Key << "ModelPath" << YAML::Value << model->getFilepath();
          out << YAML::Key << "ModelName" << YAML::Value << component.meshName;

          auto currentAnimation = component.animator.getStoredAnimation();
          if (currentAnimation)
            out << YAML::Key << "CurrentAnimation" << YAML::Value << currentAnimation->getName();
          else
            out << YAML::Key << "CurrentAnimation" << YAML::Value << "None";

          out << YAML::Key << "Material";
          out << YAML::BeginSeq;
          for (auto& pair : material.getStorage())
          {
            auto material = materialAssets->getAsset(pair.first);
            out << YAML::BeginMap;
            out << YAML::Key << "SubmeshName" << YAML::Value << pair.first;
            out << YAML::Key << "MaterialHandle" << YAML::Value << pair.second;
            out << YAML::EndMap;
          }
          out << YAML::EndSeq;

          out << YAML::EndMap;
        }
        else
        {
          out << YAML::Key << "RenderableComponent";
          out << YAML::BeginMap;

          out << YAML::Key << "ModelPath" << YAML::Value << "None";

          out << YAML::EndMap;
        }
      }

      if (entity.hasComponent<DirectionalLightComponent>())
      {
        out << YAML::Key << "DirectionalLightComponent";
        out << YAML::BeginMap;

        auto& component = entity.getComponent<DirectionalLightComponent>();
        out << YAML::Key << "Direction" << YAML::Value << component.light.direction;
        out << YAML::Key << "Colour" << YAML::Value << component.light.colour;
        out << YAML::Key << "Intensity" << YAML::Value << component.light.intensity;
        out << YAML::Key << "CastShadows" << YAML::Value << component.light.castShadows;
        out << YAML::Key << "PrimaryLight" << YAML::Value << component.light.primaryLight;

        out << YAML::EndMap;
      }

      if (entity.hasComponent<PointLightComponent>())
      {
        out << YAML::Key << "PointLightComponent";
        out << YAML::BeginMap;

        auto& component = entity.getComponent<PointLightComponent>();
        out << YAML::Key << "Position" << YAML::Value << component.light.position;
        out << YAML::Key << "Colour" << YAML::Value << component.light.colour;
        out << YAML::Key << "Intensity" << YAML::Value << component.light.intensity;
        out << YAML::Key << "Radius" << YAML::Value << component.light.radius;
        out << YAML::Key << "Falloff" << YAML::Value << component.light.falloff;
        out << YAML::Key << "CastShadows" << YAML::Value << component.light.castShadows;

        out << YAML::EndMap;
      }

      if (entity.hasComponent<SpotLightComponent>())
      {
        out << YAML::Key << "SpotLightComponent";
        out << YAML::BeginMap;

        auto& component = entity.getComponent<SpotLightComponent>();
        out << YAML::Key << "Position" << YAML::Value << component.light.position;
        out << YAML::Key << "Direction" << YAML::Value << component.light.direction;
        out << YAML::Key << "Colour" << YAML::Value << component.light.colour;
        out << YAML::Key << "Intensity" << YAML::Value << component.light.intensity;
        out << YAML::Key << "InnerCutoff" << YAML::Value << component.light.innerCutoff;
        out << YAML::Key << "OuterCutoff" << YAML::Value << component.light.outerCutoff;
        out << YAML::Key << "Radius" << YAML::Value << component.light.radius;
        out << YAML::Key << "CastShadows" << YAML::Value << component.light.castShadows;

        out << YAML::EndMap;
      }

      if (entity.hasComponent<AmbientComponent>())
      {
        out << YAML::Key << "AmbientComponent";
        out << YAML::BeginMap;

        auto& component = entity.getComponent<AmbientComponent>();
        auto state = Renderer3D::getState();
        out << YAML::Key << "IBLPath" << YAML::Value << component.ambient->getFilepath();
        out << YAML::Key << "EnviRes" << YAML::Value << state->skyboxWidth;
        out << YAML::Key << "IrraRes" << YAML::Value << state->irradianceWidth;
        out << YAML::Key << "FiltRes" << YAML::Value << state->prefilterWidth;
        out << YAML::Key << "FiltSam" << YAML::Value << state->prefilterSamples;
        out << YAML::Key << "IBLRough" << YAML::Value << component.ambient->getRoughness();
        out << YAML::Key << "Intensity" << YAML::Value << component.ambient->getIntensity();
        out << YAML::Key << "SkyboxType" << YAML::Value << static_cast<uint>(component.ambient->getDrawingType());

        auto dynamicSkyType = component.ambient->getDynamicSkyType();
        out << YAML::Key << "DynamicSkyType" << YAML::Value << static_cast<uint>(dynamicSkyType);

        out << YAML::Key << "DynamicSkyParams";
        out << YAML::BeginMap;

        EnvironmentMap* env = component.ambient;
        switch (dynamicSkyType)
        {
          case DynamicSkyType::Preetham:
          {
            auto& preethamSkyParams = env->getSkyParams<PreethamSkyParams>(DynamicSkyType::Preetham);

            out << YAML::Key << "SunPosition" << YAML::Value << preethamSkyParams.sunPos;
            out << YAML::Key << "SunSize" << YAML::Value << preethamSkyParams.sunSize;
            out << YAML::Key << "SunIntensity" << YAML::Value << preethamSkyParams.sunIntensity;
            out << YAML::Key << "SkyIntensity" << YAML::Value << preethamSkyParams.skyIntensity;
            out << YAML::Key << "Turbidity" << YAML::Value << preethamSkyParams.turbidity;
            break;
          }

          case DynamicSkyType::Hillaire:
          {
            auto& hillaireSkyParams = env->getSkyParams<HillaireSkyParams>(DynamicSkyType::Hillaire);

            out << YAML::Key << "SunPosition" << YAML::Value << hillaireSkyParams.sunPos;
            out << YAML::Key << "SunSize" << YAML::Value << hillaireSkyParams.sunSize;
            out << YAML::Key << "SunIntensity" << YAML::Value << hillaireSkyParams.sunIntensity;
            out << YAML::Key << "SkyIntensity" << YAML::Value << hillaireSkyParams.skyIntensity;
            out << YAML::Key << "RayleighScatteringBase" << YAML::Value << hillaireSkyParams.rayleighScatteringBase;
            out << YAML::Key << "RayleighAbsorptionBase" << YAML::Value << hillaireSkyParams.rayleighAbsorptionBase;
            out << YAML::Key << "MieScatteringBase" << YAML::Value << hillaireSkyParams.mieScatteringBase;
            out << YAML::Key << "MieAbsorptionBase" << YAML::Value << hillaireSkyParams.mieAbsorptionBase;
            out << YAML::Key << "OzoneAbsorptionBase" << YAML::Value << hillaireSkyParams.ozoneAbsorptionBase;
            out << YAML::Key << "PlanetRadius" << YAML::Value << hillaireSkyParams.planetRadius;
            out << YAML::Key << "AtmosphereRadius" << YAML::Value << hillaireSkyParams.atmosphereRadius;
            out << YAML::Key << "ViewPosition" << YAML::Value << hillaireSkyParams.viewPos;
            break;
          }
        }
        out << YAML::EndMap;

        out << YAML::EndMap;
      }

      out << YAML::EndMap;
    }

    void
    serializeScene(Shared<Scene> scene, const std::string &filepath,
                   const std::string &name)
    {
      auto state = Renderer3D::getState();
      auto materialAssets = AssetManager<Material>::getManager();

      YAML::Emitter out;
      out << YAML::BeginMap;
      out << YAML::Key << "Scene" << YAML::Value << name;
      out << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;

      scene->getRegistry().each([&](auto entityID)
      {
        Entity entity(entityID, scene.get());
        if (!entity)
          return;

        if (entity.hasComponent<ParentEntityComponent>())
          return;

        serializeEntity(out, entity);
      });

      out << YAML::EndSeq;

      out << YAML::Key << "RendererSettings";
      out << YAML::BeginMap;
      out << YAML::Key << "BasicSettings";
      out << YAML::BeginMap;
      out << YAML::Key << "FrustumCull" << YAML::Value << state->frustumCull;
      out << YAML::EndMap;

      out << YAML::Key << "ShadowSettings";
      out << YAML::BeginMap;
      out << YAML::Key << "CascadeLambda" << YAML::Value << state->cascadeLambda;
      out << YAML::Key << "CascadeSize" << YAML::Value << state->cascadeSize;
      out << YAML::Key << "CascadeLightBleed" << YAML::Value << state->bleedReduction;
      out << YAML::EndMap;

      out << YAML::Key << "VolumetricLightSettings";
      out << YAML::BeginMap;
      out << YAML::Key << "EnableVolumetricPrimaryLight" << YAML::Value << state->enableSkyshafts;
      out << YAML::Key << "VolumetricIntensity" << YAML::Value << state->mieScatIntensity.w;
      out << YAML::Key << "ParticleDensity" << YAML::Value << state->mieAbsDensity.w;
      out << YAML::Key << "MieScattering" << YAML::Value << glm::vec3(state->mieScatIntensity);
      out << YAML::Key << "MieAbsorption" << YAML::Value << glm::vec3(state->mieAbsDensity);
      out << YAML::EndMap;

      out << YAML::Key << "BloomSettings";
      out << YAML::BeginMap;
      out << YAML::Key << "EnableBloom" << YAML::Value << state->enableBloom;
      out << YAML::Key << "Threshold" << YAML::Value << state->bloomThreshold;
      out << YAML::Key << "Knee" << YAML::Value << state->bloomKnee;
      out << YAML::Key << "Intensity" << YAML::Value << state->bloomIntensity;
      out << YAML::Key << "Radius" << YAML::Value << state->bloomRadius;
      out << YAML::EndMap;

      out << YAML::Key << "ToneMapSettings";
      out << YAML::BeginMap;
      out << YAML::Key << "ToneMapType" << YAML::Value << state->postProcessSettings.x;
      out << YAML::Key << "Gamma" << YAML::Value << state->gamma;
      out << YAML::EndMap;

      out << YAML::EndMap;

      out << YAML::Key << "Materials";
      out << YAML::BeginSeq;
      for (auto& materialHandle : materialAssets->getStorage())
        serializeMaterial(out, materialHandle, true);
      out << YAML::EndSeq;

      out << YAML::EndMap;

      std::ofstream output(filepath, std::ofstream::trunc | std::ofstream::out);
      output << out.c_str();
      output.close();
    }

    void
    serializePrefab(Entity prefab, const std::string &filepath,
                    const std::string &name)
    {
      YAML::Emitter out;
      out << YAML::BeginMap;
      out << YAML::Key << "PreFab" << YAML::Value << name;
      out << YAML::Key << "EntityInfo";

      serializeEntity(out, prefab);

      out << YAML::EndMap;

      std::ofstream output(filepath, std::ofstream::trunc | std::ofstream::out);
      output << out.c_str();
      output.close();
    }

    void
    deserializeMaterial(YAML::Node &mat, std::vector<std::string> &texturePaths,
                        bool override = false, const std::string &filepath = "")
    {
      auto materialAssets = AssetManager<Material>::getManager();

      auto parsedMaterialName = mat["MaterialName"];
      auto parsedMaterialPath = mat["MaterialPath"];

      std::string materialPath = "";
      if (parsedMaterialPath && override)
      {
        materialPath = parsedMaterialPath.as<std::string>();
        AssetHandle handle;
        deserializeMaterial(materialPath, handle);
        return;
      }
      else if (!parsedMaterialPath && filepath != "")
        materialPath = filepath;

      auto matType = mat["MaterialType"];

      std::string shaderName, materialName;
      if (matType && parsedMaterialName)
      {
        Material* outMat;

        shaderName = matType.as<std::string>();
        materialName = parsedMaterialName.as<std::string>();
        if (shaderName == "pbr_shader")
          outMat = new Material(MaterialType::PBR);
        else
          outMat = new Material(MaterialType::Unknown);

        materialAssets->attachAsset(materialName, outMat);

        outMat->getFilepath() = materialPath;

        auto floats = mat["Floats"];
        if (floats)
        {
          for (auto uFloat : floats)
          {
            auto uName = uFloat["UniformName"];
            if (uName)
              outMat->getFloat(uName.as<std::string>()) = uFloat["UniformValue"].as<float>();
          }
        }

        auto vec2s = mat["Vec2s"];
        if (vec2s)
        {
          for (auto uVec2 : vec2s)
          {
            auto uName = uVec2["UniformName"];
            if (uName)
              outMat->getVec2(uName.as<std::string>()) = uVec2["UniformValue"].as<glm::vec2>();
          }
        }

        auto vec3s = mat["Vec3s"];
        if (vec3s)
        {
          for (auto uVec3 : vec3s)
          {
            auto uName = uVec3["UniformName"];
            if (uName)
              outMat->getVec3(uName.as<std::string>()) = uVec3["UniformValue"].as<glm::vec3>();
          }
        }

        auto sampler2Ds = mat["Sampler2Ds"];
        if (sampler2Ds)
        {
          auto textureCache = AssetManager<Texture2D>::getManager();

          for (auto uSampler2D : sampler2Ds)
          {
            auto uName = uSampler2D["SamplerName"];
            if (uName)
            {
              if (uSampler2D["ImagePath"].as<std::string>() == "")
                continue;

              bool shouldLoad = std::find(texturePaths.begin(),
                                          texturePaths.end(),
                                          uSampler2D["ImagePath"].as<std::string>()
                                          ) == texturePaths.end();

              if (!textureCache->hasAsset(uSampler2D["SamplerHandle"].as<std::string>()) && shouldLoad)
                texturePaths.emplace_back(uSampler2D["ImagePath"].as<std::string>());

              outMat->attachSampler2D(uSampler2D["SamplerName"].as<std::string>(),
                                      uSampler2D["SamplerHandle"].as<std::string>());
            }
          }
        }
      }
    }

    bool deserializeMaterial(const std::string &filepath, AssetHandle &handle, bool override)
    {
      YAML::Node data = YAML::LoadFile(filepath);

      if (!data["MaterialName"])
        return false;

      handle = data["MaterialName"].as<std::string>();

      std::vector<std::string> texturePaths;
      deserializeMaterial(data, texturePaths, false, filepath);

      for (auto& texturePath : texturePaths)
        AsyncLoading::loadImageAsync(texturePath);

      return true;
    }

    Entity
    deserializeEntity(YAML::Node &entity, Shared<Scene> scene, Entity parent = Entity())
    {
      // Fetch the logs.
      Logger* logs = Logger::getInstance();

      uint entityID = entity["EntityID"].as<uint>();

      Entity newEntity = scene->createEntity(entityID);

      auto nameComponent = entity["NameComponent"];
      if (nameComponent)
      {
        std::string name = nameComponent["Name"].as<std::string>();
        std::string description = nameComponent["Description"].as<std::string>();

        auto& nComponent = newEntity.getComponent<NameComponent>();
        nComponent.name = name;
        nComponent.description = description;
      }

      auto prefabComponent = entity["PrefabComponent"];
      if (prefabComponent)
      {
        auto pfID = prefabComponent["PreFabID"].as<std::string>();
        auto pfPath = prefabComponent["PreFabPath"].as<std::string>();
        auto& pfComponent = newEntity.addComponent<PrefabComponent>(pfID, pfPath);
        pfComponent.synch = prefabComponent["Synch"].as<bool>();
      }

      auto childEntityComponents = entity["ChildEntities"];
      if (childEntityComponents)
      {
        auto& children = newEntity.addComponent<ChildEntityComponent>().children;

        for (auto childNode : childEntityComponents)
        {
          Entity child = deserializeEntity(childNode, scene, newEntity);
          children.push_back(child);
        }
      }

      if (parent)
        newEntity.addComponent<ParentEntityComponent>(parent);

      auto transformComponent = entity["TransformComponent"];
      if (transformComponent)
      {
        glm::vec3 translation = transformComponent["Translation"].as<glm::vec3>();
        glm::vec3 rotation = transformComponent["Rotation"].as<glm::vec3>();
        glm::vec3 scale = transformComponent["Scale"].as<glm::vec3>();

        newEntity.addComponent<TransformComponent>(translation, rotation, scale);
      }

      auto renderableComponent = entity["RenderableComponent"];
      if (renderableComponent)
      {
        std::string modelPath = renderableComponent["ModelPath"].as<std::string>();

        std::ifstream test(modelPath);
        if (test)
        {
          std::string modelName = renderableComponent["ModelName"].as<std::string>();
          auto& rComponent = newEntity.addComponent<RenderableComponent>(modelName);

          // If the path is "None" its an internal model asset.
          // TODO: Handle internals separately.
          if (modelPath != "None")
          {
            auto animationName = renderableComponent["CurrentAnimation"];
            if (animationName)
              rComponent.animationHandle = animationName.as<std::string>();

            auto materials = renderableComponent["Material"];
            if (materials)
            {
              for (auto mat : materials)
              {
                rComponent.materials.attachMesh(mat["SubmeshName"].as<std::string>(),
                                                mat["MaterialHandle"].as<std::string>());
              }
            }
            AsyncLoading::asyncLoadModel(modelPath, modelName, newEntity, scene.get());
          }
        }
        else
          logs->logMessage(LogMessage("Error, file " + modelPath + " cannot be opened.", true, true));
      }

      auto directionalComponent = entity["DirectionalLightComponent"];
      if (directionalComponent)
      {
        auto& dComponent = newEntity.addComponent<DirectionalLightComponent>();
        dComponent.light.direction = directionalComponent["Direction"].as<glm::vec3>();
        dComponent.light.colour = directionalComponent["Colour"].as<glm::vec3>();
        dComponent.light.intensity = directionalComponent["Intensity"].as<float>();
        dComponent.light.castShadows = directionalComponent["CastShadows"].as<bool>();
        dComponent.light.primaryLight = directionalComponent["PrimaryLight"].as<bool>();
      }

      auto pointComponent = entity["PointLightComponent"];
      if (pointComponent)
      {
        auto& pComponent = newEntity.addComponent<PointLightComponent>();
        pComponent.light.position = pointComponent["Position"].as<glm::vec3>();
        pComponent.light.colour = pointComponent["Colour"].as<glm::vec3>();
        pComponent.light.intensity = pointComponent["Intensity"].as<float>();
        pComponent.light.radius = pointComponent["Radius"].as<float>();
        pComponent.light.falloff = pointComponent["Falloff"].as<float>();
        pComponent.light.castShadows = pointComponent["CastShadows"].as<bool>();
      }

      auto spotComponent = entity["SpotLightComponent"];
      if (spotComponent)
      {
        auto& sComponent = newEntity.addComponent<SpotLightComponent>();
        sComponent.light.position = spotComponent["Position"].as<glm::vec3>();
        sComponent.light.direction = spotComponent["Direction"].as<glm::vec3>();
        sComponent.light.colour = spotComponent["Colour"].as<glm::vec3>();
        sComponent.light.intensity = spotComponent["Intensity"].as<float>();
        sComponent.light.innerCutoff = spotComponent["InnerCutoff"].as<float>();
        sComponent.light.outerCutoff = spotComponent["OuterCutoff"].as<float>();
        sComponent.light.radius = spotComponent["Radius"].as<float>();
        sComponent.light.castShadows = spotComponent["CastShadows"].as<bool>();
      }

      auto ambientComponent = entity["AmbientComponent"];
      if (ambientComponent)
      {
        auto state = Renderer3D::getState();
        auto storage = Renderer3D::getStorage();

        std::string iblImagePath = ambientComponent["IBLPath"].as<std::string>();
        state->skyboxWidth = ambientComponent["EnviRes"].as<uint>();
        state->irradianceWidth = ambientComponent["IrraRes"].as<uint>();
        state->prefilterWidth = ambientComponent["FiltRes"].as<uint>();
        state->prefilterSamples = ambientComponent["FiltSam"].as<uint>();
        storage->currentEnvironment->unloadEnvironment();

        auto& aComponent = newEntity.addComponent<AmbientComponent>(iblImagePath);
        storage->currentEnvironment->equiToCubeMap(true, state->skyboxWidth, state->skyboxWidth);
        storage->currentEnvironment->precomputeIrradiance(state->irradianceWidth, state->irradianceWidth, true);
        storage->currentEnvironment->precomputeSpecular(state->prefilterWidth, state->prefilterWidth, true);
        aComponent.ambient->getRoughness() = ambientComponent["IBLRough"].as<float>();
        aComponent.ambient->getIntensity() = ambientComponent["Intensity"].as<float>();

        uint skyboxType = ambientComponent["SkyboxType"].as<uint>();
        aComponent.ambient->setDrawingType(static_cast<MapType>(skyboxType));

        auto dynamicSkyType = static_cast<DynamicSkyType>(ambientComponent["DynamicSkyType"].as<uint>());
        aComponent.ambient->setDynamicSkyType(dynamicSkyType);

        if (ambientComponent["DynamicSkyParams"])
        {
          auto dynamicSkyParams = ambientComponent["DynamicSkyParams"];

          EnvironmentMap* env = aComponent.ambient;
          switch (dynamicSkyType)
          {
            case DynamicSkyType::Preetham:
            {
              auto preethamSkyParams = env->getSkyParams<PreethamSkyParams>(DynamicSkyType::Preetham);

              preethamSkyParams.sunPos = dynamicSkyParams["SunPosition"].as<glm::vec3>();
              preethamSkyParams.sunSize = dynamicSkyParams["SunSize"].as<float>();
              preethamSkyParams.sunIntensity = dynamicSkyParams["SunIntensity"].as<float>();
              preethamSkyParams.skyIntensity = dynamicSkyParams["SkyIntensity"].as<float>();
              preethamSkyParams.turbidity = dynamicSkyParams["Turbidity"].as<float>();

              env->setSkyModelParams<PreethamSkyParams>(preethamSkyParams);
              break;
            }

            case DynamicSkyType::Hillaire:
            {
              auto hillaireSkyParams = env->getSkyParams<HillaireSkyParams>(DynamicSkyType::Hillaire);

              hillaireSkyParams.sunPos = dynamicSkyParams["SunPosition"].as<glm::vec3>();
              hillaireSkyParams.sunSize = dynamicSkyParams["SunSize"].as<float>();
              hillaireSkyParams.sunIntensity = dynamicSkyParams["SunIntensity"].as<float>();
              hillaireSkyParams.skyIntensity = dynamicSkyParams["SkyIntensity"].as<float>();
              hillaireSkyParams.rayleighScatteringBase = dynamicSkyParams["RayleighScatteringBase"].as<glm::vec3>();
              hillaireSkyParams.rayleighAbsorptionBase = dynamicSkyParams["RayleighAbsorptionBase"].as<float>();
              hillaireSkyParams.mieScatteringBase = dynamicSkyParams["MieScatteringBase"].as<float>();
              hillaireSkyParams.mieAbsorptionBase = dynamicSkyParams["MieAbsorptionBase"].as<float>();
              hillaireSkyParams.ozoneAbsorptionBase = dynamicSkyParams["OzoneAbsorptionBase"].as<glm::vec3>();
              hillaireSkyParams.planetRadius = dynamicSkyParams["PlanetRadius"].as<float>();
              hillaireSkyParams.atmosphereRadius = dynamicSkyParams["AtmosphereRadius"].as<float>();
              hillaireSkyParams.viewPos = dynamicSkyParams["ViewPosition"].as<glm::vec3>();

              env->setSkyModelParams<HillaireSkyParams>(hillaireSkyParams);
              break;
            }
          }
        }
      }

      return newEntity;
    }

    bool
    deserializeScene(Shared<Scene> scene, const std::string &filepath)
    {
      YAML::Node data = YAML::LoadFile(filepath);

      if (!data["Scene"])
        return false;

      auto entities = data["Entities"];
      if (!entities)
        return false;

      for (auto entity : entities)
        deserializeEntity(entity, scene);

      auto rendererSettings = data["RendererSettings"];
      if (rendererSettings)
      {
        auto state = Renderer3D::getState();

        auto basicSettings = rendererSettings["BasicSettings"];
        if (basicSettings)
        {
          state->frustumCull = basicSettings["FrustumCull"].as<bool>();
        }

        auto shadowSettings = rendererSettings["ShadowSettings"];
        if (shadowSettings)
        {
          state->cascadeLambda = shadowSettings["CascadeLambda"].as<float>();
          state->cascadeSize = shadowSettings["CascadeSize"].as<uint>();
          state->bleedReduction = shadowSettings["CascadeLightBleed"].as<float>();
        }

        auto volumetricSettings = rendererSettings["VolumetricLightSettings"];
        if (volumetricSettings)
        {
          state->enableSkyshafts = volumetricSettings["EnableVolumetricPrimaryLight"].as<bool>();
          state->mieScatIntensity.w = volumetricSettings["VolumetricIntensity"].as<float>();
          state->mieAbsDensity.w = volumetricSettings["ParticleDensity"].as<float>();
          glm::vec3 mieScattering = volumetricSettings["MieScattering"].as<glm::vec3>();
          state->mieScatIntensity.x = mieScattering.x;
          state->mieScatIntensity.y = mieScattering.y;
          state->mieScatIntensity.z = mieScattering.z;

          glm::vec3 mieAbsorption = volumetricSettings["MieAbsorption"].as<glm::vec3>();
          state->mieAbsDensity.x = mieAbsorption.x;
          state->mieAbsDensity.y = mieAbsorption.y;
          state->mieAbsDensity.z = mieAbsorption.z;
        }

        auto bloomSettings = rendererSettings["BloomSettings"];
        if (bloomSettings)
        {
          state->bloomThreshold = bloomSettings["Threshold"].as<float>();
          state->bloomKnee = bloomSettings["Knee"].as<float>();
          state->bloomIntensity = bloomSettings["Intensity"].as<float>();
          state->bloomRadius = bloomSettings["Radius"].as<float>();
        }

        auto tonemapSettings = rendererSettings["ToneMapSettings"];
        if (tonemapSettings)
        {
          state->postProcessSettings.x = tonemapSettings["ToneMapType"].as<uint>();
          state->gamma = tonemapSettings["Gamma"].as<float>();
        }
      }

      auto materials = data["Materials"];
      if (materials)
      {
        std::vector<std::string> texturePaths;
        for (auto mat : materials)
          deserializeMaterial(mat, texturePaths, true);

        for (auto& texturePath : texturePaths)
          AsyncLoading::loadImageAsync(texturePath);
      }

      return true;
    }

    bool
    deserializePrefab(Shared<Scene> scene, const std::string &filepath)
    {
      YAML::Node data = YAML::LoadFile(filepath);

      if (!data["PreFab"])
        return false;

      auto preFabName = data["PreFab"].as<std::string>();

      auto preFabInfo = data["EntityInfo"];
      if (data["EntityInfo"])
      {
        auto preFabEntity = deserializeEntity(preFabInfo, scene);

        return true;
      }
      else
        return false;
    }
  }
}