#include "Graphics/Meshes.h"

// Project includes.
#include "Core/Logs.h"

namespace Strontium
{
  Mesh::Mesh(const std::string &name, Model* parent)
    : loaded(false)
    , skinned(false)
    , name(name)
    , parent(parent)
    , localTransform(1.0f)
  { }

  Mesh::Mesh(const std::string &name, const std::vector<Vertex> &vertices,
             const std::vector<uint> &indices, Model* parent)
    : loaded(true)
    , skinned(false)
    , data(vertices)
    , indices(indices)
    , vArray(nullptr)
    , name(name)
    , parent(parent)
    , localTransform(1.0f)
  { }

  Mesh::~Mesh()
  { }

  VertexArray*
  Mesh::generateVAO()
  {
    if (!this->isLoaded())
      return nullptr;

    this->vArray = createUnique<VertexArray>(this->data.data(), this->data.size() * sizeof(Vertex), BufferType::Dynamic);
    this->vArray->addIndexBuffer(this->indices.data(), this->indices.size(), BufferType::Dynamic);

    this->vArray->addAttribute(0, AttribType::Vec4, false, sizeof(Vertex), 0);
  	this->vArray->addAttribute(1, AttribType::Vec3, false, sizeof(Vertex), offsetof(Vertex, normal));
    this->vArray->addAttribute(2, AttribType::Vec2, false, sizeof(Vertex), offsetof(Vertex, uv));
    this->vArray->addAttribute(3, AttribType::Vec3, false, sizeof(Vertex), offsetof(Vertex, tangent));
    this->vArray->addAttribute(4, AttribType::Vec3, false, sizeof(Vertex), offsetof(Vertex, bitangent));

    this->vArray->addAttribute(5, AttribType::Vec4, false, sizeof(Vertex), offsetof(Vertex, boneWeights));
    this->vArray->addAttribute(6, AttribType::IVec4, false, sizeof(Vertex), offsetof(Vertex, boneIDs));

    // Don't need vertices and indices anymore, they've uploaded to the GPU.
    this->data.clear();
    this->indices.clear();

    return this->vArray.get();
  }
}
