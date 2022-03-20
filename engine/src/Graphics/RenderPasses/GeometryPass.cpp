#include "Graphics/Renderpasses/GeometryPass.h"

// Project includes.
#include "Graphics/Renderer.h"
#include "Graphics/RendererCommands.h"

namespace Strontium
{
  GeometryPass::GeometryPass(Renderer3D::GlobalRendererData* globalRendererData)
	: RenderPass(&this->passData, globalRendererData, { nullptr })
    , timer(5)
  { }

  GeometryPass::~GeometryPass()
  { }
  
  void 
  GeometryPass::onInit()
  {
    this->passData.staticGeometry = ShaderCache::getShader("geometry_pass_shader");
    this->passData.dynamicGeometry = ShaderCache::getShader("dynamic_geometry_pass");
  }

  void 
  GeometryPass::updatePassData()
  { }

  RendererDataHandle 
  GeometryPass::requestRendererData()
  {
    return -1;
  }

  void 
  GeometryPass::deleteRendererData(RendererDataHandle& handle)
  { }

  void 
  GeometryPass::onRendererBegin(uint width, uint height)
  {
    // Clear the lists and staging for the next frame.
    this->passData.staticInstanceMap.clear();
    this->passData.dynamicDrawList.clear();

    // Resize the geometry buffer.
	glm::uvec2 gBufferSize = this->passData.gBuffer.getSize();
	if (width != gBufferSize.x || height != gBufferSize.y)
	  this->passData.gBuffer.resize(width, height);

    this->passData.numUniqueEntities = 0u;

    // Clear the statistics.
    this->passData.numDrawCalls = 0u;
    this->passData.numInstances = 0u;
    this->passData.numTrianglesSubmitted = 0u;
    this->passData.numTrianglesDrawn = 0u;
  }

  void 
  GeometryPass::onRender()
  {
    // Time the entire scope.
    ScopedTimer<AsynchTimer> profiler(this->timer);

    auto rendererData = static_cast<Renderer3D::GlobalRendererData*>(this->globalBlock);

	// Setup the camera uniforms.
	struct CameraBlockData
	{
	  glm::mat4 viewMatrix;
      glm::mat4 projMatrix;
      glm::mat4 invViewProjMatrix;
      glm::vec4 camPosition; // w unused
      glm::vec4 nearFar; // Near plane (x), far plane (y), gamma correction factor (z). w is unused.
	} 
	  cameraBlock 
	{ 
      rendererData->sceneCam.view,
      rendererData->sceneCam.projection,
      rendererData->sceneCam.invViewProj,
      { rendererData->sceneCam.position, 0.0 },
      { rendererData->sceneCam.near,
        rendererData->sceneCam.far, rendererData->gamma, 0.0 }
	};
	this->passData.cameraBuffer.setData(0, sizeof(CameraBlockData), &cameraBlock);
    
    // Upload the cached data for both static and dynamic geometry.
    if (this->passData.entityDataBuffer.size() != (sizeof(PerEntityData) * this->passData.numUniqueEntities))
      this->passData.entityDataBuffer.resize(sizeof(PerEntityData) * this->passData.numUniqueEntities, BufferType::Dynamic);

    uint bufferOffset = 0;
    for (auto& [vaoMat, instancedData] : this->passData.staticInstanceMap)
    {
      this->passData.entityDataBuffer.setData(bufferOffset, sizeof(PerEntityData) * instancedData.size(),
                                              instancedData.data());
      bufferOffset += sizeof(PerEntityData) * instancedData.size();
    }
    for (auto& drawCommand : this->passData.dynamicDrawList)
    {
      this->passData.entityDataBuffer.setData(bufferOffset, sizeof(PerEntityData),
                                              &drawCommand.data);
      bufferOffset += sizeof(PerEntityData);
    }

	// Start the geometry pass.
    this->passData.gBuffer.beginGeoPass();

    this->passData.cameraBuffer.bindToPoint(0);
    this->passData.perDrawUniforms.bindToPoint(1);

    this->passData.entityDataBuffer.bindToPoint(0);

    bufferOffset = 0;
    // Static geometry pass.
    this->passData.staticGeometry->bind();
    for (auto& [drawable, instancedData] : this->passData.staticInstanceMap)
    {
      // Set the index offset. 
      this->passData.perDrawUniforms.setData(0, sizeof(int), &bufferOffset);

      drawable.technique->configureTextures();

      drawable.primatives->bind();
      RendererCommands::drawElementsInstanced(PrimativeType::Triangle, 
                                              drawable.primatives->numToRender(), 
                                              instancedData.size());
      drawable.primatives->unbind();
      
      bufferOffset += instancedData.size();

      // Record some statistics.
      this->passData.numDrawCalls++;
      this->passData.numInstances += instancedData.size();
      this->passData.numTrianglesDrawn += (instancedData.size() * drawable.primatives->numToRender()) / 3;
    }

    // Dynamic geometry pass for skinned objects.
    // TODO: Improve this with compute shader skinning. 
    // Could probably get rid of this pass all together.
    this->passData.dynamicGeometry->bind();
    this->passData.boneBuffer.bindToPoint(4);
    for (auto& drawable : this->passData.dynamicDrawList)
    {
      auto& bones = static_cast<Animator*>(drawable)->getFinalBoneTransforms();
      this->passData.boneBuffer.setData(0, bones.size() * sizeof(glm::mat4),
                                        bones.data());
      
      // Set the index offset. 
      this->passData.perDrawUniforms.setData(0, sizeof(int), &bufferOffset);

      drawable.technique->configureTextures();
      
      drawable.primatives->bind();
      RendererCommands::drawElementsInstanced(PrimativeType::Triangle, 
                                              drawable.primatives->numToRender(), 
                                              drawable.instanceCount);
      drawable.primatives->unbind();

      bufferOffset += drawable.instanceCount;

      // Record some statistics.
      this->passData.numDrawCalls++;
      this->passData.numInstances += drawable.instanceCount;
      this->passData.numTrianglesDrawn += (drawable.instanceCount * drawable.primatives->numToRender()) / 3;
    }
    this->passData.dynamicGeometry->unbind();

    this->passData.gBuffer.endGeoPass();
  }

  void 
  GeometryPass::onRendererEnd(FrameBuffer& frontBuffer)
  {
    this->timer.msRecordTime(this->passData.frameTime);
  }

  void 
  GeometryPass::onShutdown()
  { }

  void 
  GeometryPass::submit(Model* data, ModelMaterial &materials, const glm::mat4 &model,
                       float id, bool drawSelectionMask)
  {
    auto rendererData = static_cast<Renderer3D::GlobalRendererData*>(this->globalBlock);

	for (auto& submesh : data->getSubmeshes())
	{
      auto material = materials.getMaterial(submesh.getName());
      if (!material)
        continue;

      VertexArray* vao = submesh.hasVAO() ? submesh.getVAO() : submesh.generateVAO();
      if (!vao)
        continue;

      // Record some statistics.
      this->passData.numTrianglesSubmitted += vao->numToRender() / 3;

      auto localTransform = model * submesh.getTransform();
      if (!boundingBoxInFrustum(rendererData->camFrustum, submesh.getMinPos(),
						  submesh.getMaxPos(), localTransform))
        continue;

      // Populate the draw list.
      auto drawData = this->passData.staticInstanceMap.find(GeomStaticDrawData(vao, material));

      this->passData.numUniqueEntities++;
      if (drawData != this->passData.staticInstanceMap.end())
      {
        drawData->second.emplace_back(localTransform, 
                                      glm::vec4(drawSelectionMask ? 1.0f : 0.0f, id + 1.0f, 0.0f, 0.0f), 
                                      material->getPackedUniformData());
      }
      else 
      {
        auto& item = this->passData.staticInstanceMap.emplace(GeomStaticDrawData(vao, material), std::vector<PerEntityData>());
        item.first->second.emplace_back(localTransform, glm::vec4(drawSelectionMask ? 1.0f : 0.0f, id + 1.0f, 0.0f, 0.0f), 
                                        material->getPackedUniformData());
      }
	}
  }

  void 
  GeometryPass::submit(Model* data, Animator* animation, ModelMaterial &materials,
                       const glm::mat4 &model, float id, bool drawSelectionMask)
  {
    auto rendererData = static_cast<Renderer3D::GlobalRendererData*>(this->globalBlock);

    if (data->hasSkins())
    {
      // Skinned animated mesh, store the static transform.
      for (auto& submesh : data->getSubmeshes())
	  {
        auto material = materials.getMaterial(submesh.getName());
        if (!material)
          continue;

        VertexArray* vao = submesh.hasVAO() ? submesh.getVAO() : submesh.generateVAO();
        if (!vao)
          continue;

        // Record some statistics.
        this->passData.numTrianglesSubmitted += vao->numToRender() / 3;

        if (!boundingBoxInFrustum(rendererData->camFrustum, data->getMinPos(),
	 						    data->getMaxPos(), model))
          continue;

        // Populate the dynamic draw list.
        this->passData.numUniqueEntities++;
        this->passData.dynamicDrawList.emplace_back(vao, material, animation,
                                                    PerEntityData(model,
                                                    glm::vec4(drawSelectionMask ? 1.0f : 0.0f, 
                                                              id + 1.0f, 0.0f, 0.0f), 
                                                    material->getPackedUniformData()));
	  }
    }
    else
    {
      // Unskinned animated mesh, store the rigged transform.
	  auto& bones = animation->getFinalUnSkinnedTransforms();
	  for (auto& submesh : data->getSubmeshes())
	  {
        auto material = materials.getMaterial(submesh.getName());
        if (!material)
          continue;

        VertexArray* vao = submesh.hasVAO() ? submesh.getVAO() : submesh.generateVAO();
        if (!vao)
          continue;

        // Record some statistics.
        this->passData.numTrianglesSubmitted += vao->numToRender() / 3;

        auto localTransform = model * bones[submesh.getName()];
	    if (!boundingBoxInFrustum(rendererData->camFrustum, submesh.getMinPos(),
	 						      submesh.getMaxPos(), localTransform))
          continue;

        // Populate the draw list.
        auto drawData = this->passData.staticInstanceMap.find(GeomStaticDrawData(vao, material));
        
        this->passData.numUniqueEntities++;
        if (drawData != this->passData.staticInstanceMap.end())
        {
          this->passData.staticInstanceMap.at(drawData->first)
                                          .emplace_back(localTransform, glm::vec4(drawSelectionMask ? 1.0f : 0.0f, 
                                                        id + 1.0f, 0.0f, 0.0f), material->getPackedUniformData());
        }
        else 
        {
          this->passData.staticInstanceMap.emplace(GeomStaticDrawData(vao, material), std::vector<PerEntityData>());
          this->passData.staticInstanceMap.at(GeomStaticDrawData(vao, material))
                                          .emplace_back(localTransform, glm::vec4(drawSelectionMask ? 1.0f : 0.0f, 
                                                        id + 1.0f, 0.0f, 0.0f), material->getPackedUniformData());
        }
      }  
	}
  }
}