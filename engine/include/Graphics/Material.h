#pragma once

// Macro include file.
#include "StrontiumPCH.h"

// Project includes.
#include "Core/ApplicationBase.h"
#include "Assets/AssetManager.h"
#include "Graphics/Shaders.h"
#include "Graphics/Textures.h"
#include "Graphics/Meshes.h"
#include "Utils/Utilities.h"

#include "ShadingPrimatives.h"

namespace Strontium
{
  // Individual material class to hold shaders and shader data.
  class Material
  {
  public:
    // Type of the material.
    enum class Type
    {
      PBR = 0u, 
      Unknown = 1u
    };

    Material(Type type = Type::PBR);
    ~Material();

    // Prepare for drawing.
    void configureTextures(bool bindOnlyAlbedo = false);

    // Sampler configuration.
    bool hasSampler1D(const std::string &samplerName);
    void attachSampler1D(const std::string &samplerName, const Asset::Handle &handle);
    bool hasSampler2D(const std::string &samplerName);
    void attachSampler2D(const std::string &samplerName, const Asset::Handle &handle);
    bool hasSampler3D(const std::string &samplerName);
    void attachSampler3D(const std::string &samplerName, const Asset::Handle &handle);
    bool hasSamplerCubemap(const std::string &samplerName);
    void attachSamplerCubemap(const std::string &samplerName, const Asset::Handle &handle);

    // TODO: Implement 1D and 3D texture fetching.
    Texture2D* getSampler2D(const std::string &samplerName);
    Asset::Handle& getSampler2DHandle(const std::string &samplerName);

    MaterialBlockData getPackedUniformData();

    // Get the shader and pipeline type.
    void setType(Type type) { this->type = type; }
    Type getType() { return this->type; }

    // Get the shader program.
    Shader* getShader() { return this->program; }

    // Get the shader data.
    float getfloat(const std::string &name)
    {
      auto loc = Utilities::pairGet<std::string, float>(this->floats, name);
      return loc->second;
    };

    glm::vec2 getvec2(const std::string &name)
    {
      auto loc = Utilities::pairGet<std::string, glm::vec2>(this->vec2s, name);
      return loc->second;
    };

    glm::vec3 getvec3(const std::string &name)
    {
      auto loc = Utilities::pairGet<std::string, glm::vec3>(this->vec3s, name);
      return loc->second;
    };

    glm::vec4 getvec4(const std::string &name)
    {
      auto loc = Utilities::pairGet<std::string, glm::vec4>(this->vec4s, name);
      return loc->second;
    };

    glm::mat3 getmat3(const std::string &name)
    {
      auto loc = Utilities::pairGet<std::string, glm::mat3>(this->mat3s, name);
      return loc->second;
    };

    glm::mat4 getmat4(const std::string &name)
    {
      auto loc = Utilities::pairGet<std::string, glm::mat4>(this->mat4s, name);
      return loc->second;
    };

    void set(float newFloat, const std::string& name)
    {
      auto loc = Utilities::pairGet<std::string, float>(this->floats, name);
      if (loc->second != newFloat)
      {
          loc->second = newFloat;
      }
    }

    void set(const glm::vec2 &newVec2, const std::string& name)
    {
        auto loc = Utilities::pairGet<std::string, glm::vec2>(this->vec2s, name);
        if (loc->second != newVec2)
        {
            loc->second = newVec2;
        }
    }

    void set(const glm::vec3& newVec3, const std::string& name)
    {
        auto loc = Utilities::pairGet<std::string, glm::vec3>(this->vec3s, name);
        if (loc->second != newVec3)
        {
            loc->second = newVec3;
        }
    }

    void set(const glm::vec4& newVec4, const std::string& name)
    {
        auto loc = Utilities::pairGet<std::string, glm::vec4>(this->vec4s, name);
        if (loc->second != newVec4)
        {
            loc->second = newVec4;
        }
    }

    void set(const glm::mat3& newMat3, const std::string& name)
    {
        auto loc = Utilities::pairGet<std::string, glm::mat3>(this->mat3s, name);
        if (loc->second != newMat3)
        {
            loc->second = newMat3;
        }
    }

    void set(const glm::mat4& newMat4, const std::string& name)
    {
        auto loc = Utilities::pairGet<std::string, glm::mat4>(this->mat4s, name);
        if (loc->second != newMat4)
        {
            loc->second = newMat4;
        }
    }

    // Operator overloading makes this nice and easy.
    operator Shader*() { return this->program; }

    // Get the internal storage for serialization.
    std::vector<std::pair<std::string, float>>& getFloats() { return this->floats; }
    std::vector<std::pair<std::string, glm::vec2>>& getVec2s() { return this->vec2s; }
    std::vector<std::pair<std::string, glm::vec3>>& getVec3s() { return this->vec3s; }
    std::vector<std::pair<std::string, glm::vec4>>& getVec4s() { return this->vec4s; }
    std::vector<std::pair<std::string, glm::mat3>>& getMat3s() { return this->mat3s; }
    std::vector<std::pair<std::string, glm::mat4>>& getMat4s() { return this->mat4s; }
    std::vector<std::pair<std::string, Asset::Handle>>& getSampler1Ds() { return this->sampler1Ds; }
    std::vector<std::pair<std::string, Asset::Handle>>& getSampler2Ds() { return this->sampler2Ds; }
    std::vector<std::pair<std::string, Asset::Handle>>& getSampler3Ds() { return this->sampler3Ds; }
    std::vector<std::pair<std::string, Asset::Handle>>& getSamplerCubemaps() { return this->samplerCubes; }
  private:
    // Reflect the attached shader.
    void reflect();

    // The material type and pipeline.
    Type type;
    bool pipeline;

    // The shader and shader data.
    Shader* program;
    std::vector<std::pair<std::string, float>> floats;
    std::vector<std::pair<std::string, glm::vec2>> vec2s;
    std::vector<std::pair<std::string, glm::vec3>> vec3s;
    std::vector<std::pair<std::string, glm::vec4>> vec4s;
    std::vector<std::pair<std::string, glm::mat3>> mat3s;
    std::vector<std::pair<std::string, glm::mat4>> mat4s;

    std::vector<std::pair<std::string, Asset::Handle>> sampler1Ds;
    std::vector<std::pair<std::string, Asset::Handle>> sampler2Ds;
    std::vector<std::pair<std::string, Asset::Handle>> sampler3Ds;
    std::vector<std::pair<std::string, Asset::Handle>> samplerCubes;
  };

  // Macro material which holds all the individual material objects for each
  // submesh of a model.
  class ModelMaterial
  {
  public:
    ModelMaterial() = default;
    ~ModelMaterial() = default;

    void attachMesh(const std::string &meshName, Material::Type type = Material::Type::PBR);
    void attachMesh(const std::string &meshName, const Asset::Handle &material);
    void swapMaterial(const std::string &meshName, const Asset::Handle &newMaterial);

    Material* getMaterial(const std::string &meshName);
    Asset::Handle getMaterialHandle(const std::string &meshName);
    uint getNumStored() { return this->materials.size(); }

    // Get the storage.
    std::vector<std::pair<std::string, Asset::Handle>>& getStorage() { return this->materials; };
  private:
    std::vector<std::pair<std::string, Asset::Handle>> materials;
  };
}
