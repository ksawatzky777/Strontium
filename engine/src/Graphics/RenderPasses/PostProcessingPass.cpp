#include "Graphics/RenderPasses/PostProcessingPass.h"

// Project includes.
#include "Graphics/Renderer.h"
#include "Graphics/RendererCommands.h"

namespace Strontium
{
  PostProcessingPass::PostProcessingPass(Renderer3D::GlobalRendererData* globalRendererData,
                                         GeometryPass* previousGeoPass)
    : RenderPass(&this->passData, globalRendererData, { previousGeoPass })
    , previousGeoPass(previousGeoPass)
    , timer(5)
  {

  }

  PostProcessingPass::~PostProcessingPass()
  { }

  void 
  PostProcessingPass::onInit()
  {
    this->passData.postProcessingShader = ShaderCache::getShader("post_processing");
  }

  void 
  PostProcessingPass::updatePassData()
  { }

  RendererDataHandle 
  PostProcessingPass::requestRendererData()
  {
    return -1;
  }

  void 
  PostProcessingPass::deleteRendererData(RendererDataHandle& handle)
  { }

  void 
  PostProcessingPass::onRendererBegin(uint width, uint height)
  { }

  void 
  PostProcessingPass::onRender()
  { }

  void 
  PostProcessingPass::onRendererEnd(FrameBuffer& frontBuffer)
  {
    this->timer.begin();

    // Bind the lighting buffer.
    this->globalBlock->lightingBuffer.bind(0);

    // Bind the IDs and the entity mask.
    this->previousGeoPass->getInternalDataBlock<GeometryPassDataBlock>()
                         ->gBuffer.bindAttachment(FBOTargetParam::Colour3, 1);

    // TODO: Bind bloom texture.
    

    // Bind the camera uniforms.
    this->previousGeoPass->getInternalDataBlock<GeometryPassDataBlock>()->cameraBuffer.bindToPoint(0);

    // Upload the post processing settings.
    struct PostBlockData
    {
      glm::vec4 bloom;  // Bloom intensity (x). y, z and w are unused.
      glm::ivec2 postSettings; // Using bloom (bit 1), using FXAA (bit 2) (x). Tone mapping operator (y). z and w are unused.
    } 
      postBlock
    {
      { 0.0f, 0.0f, 0.0f, 0.0f },
      { 0, static_cast<int>(this->passData.toneMapOp) }
    };
    postBlock.postSettings.x |= 1 * static_cast<int>(this->passData.useFXAA);
    postBlock.postSettings.x |= 0; // TODO: Apply bloom goes here.

    this->passData.postProcessingParams.bindToPoint(1);
    this->passData.postProcessingParams.setData(0, sizeof(PostBlockData), &postBlock);

    RendererCommands::disable(RendererFunction::DepthTest);
    frontBuffer.clear();
    frontBuffer.setViewport();
    frontBuffer.bind();

    this->globalBlock->blankVAO.bind();
    this->passData.postProcessingShader->bind();
    RendererCommands::drawArrays(PrimativeType::Triangle, 0, 3);

    frontBuffer.unbind();
    RendererCommands::enable(RendererFunction::DepthTest);

    this->timer.end();
    this->timer.msRecordTime(this->passData.frameTime);
  }

  void 
  PostProcessingPass::onShutdown()
  {}
}