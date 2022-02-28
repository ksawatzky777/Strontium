#type compute
#version 460 core
/*
 * A volumetric directional light compute shader. Computes godrays at half
 * resolution with some dithering.
*/

#define PI 3.141592654

#define NUM_STEPS 16
#define NUM_CASCADES 4

layout(local_size_x = 8, local_size_y = 8) in;

// The output texture for the volumetric effect.
layout(rgba16f, binding = 0) restrict writeonly uniform image2D volumetric;

layout(binding = 3) uniform sampler2D gDepth;
layout(binding = 7) uniform sampler2D cascadeMaps[NUM_CASCADES]; // TODO: Texture arrays.

// Camera specific uniforms.
layout(std140, binding = 0) uniform CameraBlock
{
  mat4 u_viewMatrix;
  mat4 u_projMatrix;
  mat4 u_invViewProjMatrix;
  vec3 u_camPosition;
  vec4 u_nearFarGamma; // Near plane (x), far plane (y), gamma correction factor (z). w is unused.
};

// Directional light uniforms.
layout(std140, binding = 5) uniform DirectionalBlock
{
  vec4 u_lColourIntensity;
  vec4 u_lDirection;
  ivec4 u_directionalSettings; // Shadow quality (x). y, z and w are unused.
};

layout(std140, binding = 7) uniform CascadedShadowBlock
{
  mat4 u_lightVP[NUM_CASCADES];
  vec4 u_cascadeData[NUM_CASCADES]; // Cascade split distance (x). y, z and w are unused.
  vec4 u_shadowParams1; // Light bleed reduction (x), light size for PCSS (y), minimum PCF radius (z) and the normal bias (w).
  vec4 u_shadowParams2; // The constant bias (x). y, z and w are unused.
};

layout(std140, binding = 1) readonly buffer GodrayParams
{
  vec4 u_mieScatIntensity; // Mie scattering coefficients (x, y, z), light shaft intensity (w).
  vec4 u_mieAbsDensity; // Mie scattering coefficients (x, y, z), density (w).
};

// Helper functions.
float sampleDither(ivec2 coords);

// Mie phase function.
float getMiePhase(float cosTheta);

// Shadow calculations.
float calcShadow(uint cascadeIndex, vec3 position);

// Decodes the worldspace position of the fragment from depth.
vec3 decodePosition(vec2 texCoords, sampler2D depthMap, mat4 invMVP)
{
  float depth = texture(depthMap, texCoords).r;
  vec3 clipCoords = 2.0 * vec3(texCoords, depth) - 1.0.xxx;
  vec4 temp = invMVP * vec4(clipCoords, 1.0);
  return temp.xyz / temp.w;
}

void main()
{
  ivec2 invoke = ivec2(gl_GlobalInvocationID.xy);

  // Half resolution.
  ivec2 gBufferCoords = 2 * invoke;
  vec2 gBufferSize = textureSize(gDepth, 0).xy;
  vec2 gBufferUV = (vec2(gBufferCoords) + 0.5.xx) / gBufferSize;

  // Light properties.
  vec3 lightDir = normalize(u_lDirection.xyz);
  float volumetricIntensity = u_mieScatIntensity.w;

  // Participating medium properties.
  float density = u_mieAbsDensity.w;
  vec3 mieScattering = u_mieScatIntensity.xyz * 0.01;
  vec3 mieAbsorption = u_mieAbsDensity.xyz * 0.01;
  vec3 extinction = (mieScattering + mieAbsorption);

  // March from the camera position to the fragment position.
  vec3 endPos = decodePosition(gBufferUV, gDepth, u_invViewProjMatrix);
  vec3 startPos = u_camPosition.xyz;
  vec3 ray = endPos - startPos;
  vec3 rayDir = normalize(ray);
  float tMax = length(ray);

  // Dither to add some noise.
  vec3 realStartPos = startPos + (rayDir * sampleDither(invoke) * tMax / (float(NUM_STEPS) + 1.0));

  // Integrate the light contribution and transmittance along the ray.
  vec3 fog = vec3(0.0);
  vec3 totalTransmittance = vec3(1.0);
  float miePhaseValue = getMiePhase(dot(lightDir, rayDir));
  for (float i = 0; i < NUM_STEPS; i++)
  {
    float t0 = float(i) / float(NUM_STEPS);
    float t1 = (float(i) + 1.0f) / float(NUM_STEPS);

    // Non linear distribution of samples within the range.
    t0 = t0 * t0;
    t1 = t1 * t1;

    // Make t0 and t1 world space.
    t0 = tMax * t0;
    float comp = float(t1 <= 1.0);
    t1 = mix(tMax, tMax * t1, comp);

    float t = t0 + (t1 - t0) * 0.3f;
    float dt = t1 - t0;

    vec3 pos = realStartPos + (t * rayDir);
    float shadowFactor = 1.0;
    vec4 clipSpacePos = u_viewMatrix * vec4(pos, 1.0);

    // Cascaded shadow maps.
    for (uint j = 0; j < NUM_CASCADES; j++)
    {
      if (clipSpacePos.z > -(u_cascadeData[j].x))
      {
        shadowFactor = calcShadow(j, pos);
        break;
      }
    }

    float sampleDensity = density;
    vec3 sampleExtinction = sampleDensity * extinction;
    vec3 sampleScattering = sampleDensity * mieScattering;
    vec3 sampleTransmittance = exp(-dt * sampleExtinction);

    totalTransmittance *= sampleTransmittance;

    vec3 inScattering = sampleScattering * miePhaseValue;
    vec3 scatteringIntegral = (inScattering - inScattering * sampleTransmittance) / sampleExtinction;

    fog += scatteringIntegral * totalTransmittance * shadowFactor;
  }

  // Density AA as described in [Patry2021]:
  // http://advances.realtimerendering.com/s2021/jpatry_advances2021/index.html
  float averageOpacity = (1.0 - dot(totalTransmittance, (1.0 / 3.0).xxx));
  fog /= averageOpacity;
  fog *= u_mieScatIntensity.w;
  imageStore(volumetric, invoke, vec4(fog, averageOpacity));
}

float sampleDither(ivec2 coords)
{
  const mat4 ditherMatrix = mat4
  (
    vec4(0.0, 0.5, 0.125, 0.625),
    vec4(0.75, 0.22, 0.875, 0.375),
    vec4(0.1875, 0.6875, 0.0625, 0.5625),
    vec4(0.9375, 0.4375, 0.8125, 0.3125)
  );

  return ditherMatrix[coords.x % 4][coords.y % 4];
}

float getMiePhase(float cosTheta)
{
  const float g = 0.8;
  const float scale = 3.0 / (8.0 * PI);

  float num = (1.0 - g * g) * (1.0 + cosTheta * cosTheta);
  float denom = (2.0 + g * g) * pow((1.0 + g * g - 2.0 * g * cosTheta), 1.5);

  return scale * num / denom;
}

float offsetLookup(sampler2D map, vec2 loc, vec2 offset, float depth)
{
  vec2 texel = 1.0 / textureSize(map, 0).xy;
  return float(texture(map, vec2(loc.xy + offset * texel)).r >= depth);
}

// Calculate if the fragment is in shadow or not, than shadow mapping.
float calcShadow(uint cascadeIndex, vec3 position)
{
  float depthRange = 0.0;
  if (cascadeIndex == 0)
  {
    depthRange = u_cascadeData[cascadeIndex].x;
  }
  else
  {
    depthRange = u_cascadeData[cascadeIndex].x - u_cascadeData[cascadeIndex - 1].x;
  }

  float bias = u_shadowParams2.x / 1000.0 * depthRange;

  vec4 lightClipPos = u_lightVP[cascadeIndex] * vec4(position, 1.0);
  vec3 projCoords = lightClipPos.xyz / lightClipPos.w;
  projCoords = 0.5 * projCoords + 0.5;
  projCoords = clamp(projCoords, vec3(0.0), vec3(1.0));

  return offsetLookup(cascadeMaps[cascadeIndex], projCoords.xy, 0.0.xx,
                      projCoords.z - bias);
}
