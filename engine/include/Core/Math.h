#pragma once

// Macro include file.
#include "StrontiumPCH.h"

// Project includes.
#include "Core/ApplicationBase.h"
#include "Graphics/ShadingPrimatives.h"

namespace Strontium
{
  struct Plane
  {
    float d;
    glm::vec3 point;
    glm::vec3 normal;
  };

  struct BoundingBox
  {
    glm::vec3 corners[8];
    Plane sides[6];
    glm::vec3 min;
    glm::vec3 max;
  };

  struct Frustum
  {
    glm::vec3 corners[8];
    Plane sides[6];
    glm::vec3 center;
    glm::vec3 min;
    glm::vec3 max;
    float bSphereRadius;
  };

  BoundingBox buildBoundingBox(const glm::vec3 &min, const glm::vec3 &max);
  Frustum buildCameraFrustum(const Camera &camera);
  Frustum buildCameraFrustum(const glm::mat4 &viewProj, const glm::vec3 &viewVec);

  float signedPlaneDistance(const Plane &plane, const glm::vec3 &point);

  bool sphereInFrustum(const Frustum &frustum, const glm::vec3 center, float radius);
  bool boundingBoxInFrustum(const Frustum &frustum, const glm::vec3 min, const glm::vec3 max);
}