#type compute
#version 460 core
/*
 * Compute shader to generate the aerial perspective lookup texture as described in
 * [Hillaire2020].
 * https://sebh.github.io/publications/egsr2020.pdf
 * Currently doesn't implement any form of volumetric shadowing.
*/

#define PI 3.141592654
#define PLANET_RADIUS_OFFSET 0.01
#define NUM_STEPS 32

layout(local_size_x = 8, local_size_y = 8, local_size_z = 8) in;

struct ScatteringParams
{
  vec4 rayleighScat; //  Rayleigh scattering base (x, y, z) and height falloff (w).
  vec4 rayleighAbs; //  Rayleigh absorption base (x, y, z) and height falloff (w).
  vec4 mieScat; //  Mie scattering base (x, y, z) and height falloff (w).
  vec4 mieAbs; //  Mie absorption base (x, y, z) and height falloff (w).
  vec4 ozoneAbs; //  Ozone absorption base (x, y, z) and scale (w).
};

layout(rgba16f, binding = 0) restrict writeonly uniform image3D aerialLUT;

layout(binding = 2) uniform sampler2D transLUT;
layout(binding = 3) uniform sampler2D multiScatLut;

// Camera specific uniforms.
layout(std140, binding = 0) uniform CameraBlock
{
  mat4 u_viewMatrix;
  mat4 u_projMatrix;
  mat4 u_invViewProjMatrix;
  vec3 u_camPosition;
  vec4 u_nearFar; // Near plane (x), far plane (y). z and w are unused.
};

// Planetary radii are in Mm. Height falloffs are in km.
layout(std140, binding = 1) uniform HillaireParams
{
  ScatteringParams u_params;
  vec4 u_planetAlbedoRadius; // Planet albedo (x, y, z) and radius.
  vec4 u_sunDirAtmRadius; // Sun direction (x, y, z) and atmosphere radius (w).
  vec4 u_viewPos; // View position (x, y, z). w is unused.
};

// Helper functions.
float safeACos(float x)
{
  return acos(clamp(x, -1.0, 1.0));
}

float rayIntersectSphere(vec3 ro, vec3 rd, float rad)
{
  float b = dot(ro, rd);
  float c = dot(ro, ro) - rad * rad;
  if (c > 0.0f && b > 0.0)
    return -1.0;

  float discr = b * b - c;
  if (discr < 0.0)
    return -1.0;

  // Special case: inside sphere, use far discriminant
  if (discr > b * b)
    return (-b + sqrt(discr));

  return -b - sqrt(discr);
}

// Raymarch to compute the scattering integral.
vec4 raymarchScattering(vec3 pos, vec3 rayDir, vec3 sunDir, float tMax,
                        ScatteringParams params, float groundRadiusMM,
                        float atmoRadiusMM);

void main()
{
  ivec3 invoke = ivec3(gl_GlobalInvocationID.xyz);

  vec3 tex = vec3(imageSize(aerialLUT).xyz);
  vec3 texelSize = 1.0.xxx / tex;

  float groundRadiusMM = u_planetAlbedoRadius.w;
  float atmosphereRadiusMM = u_sunDirAtmRadius.w;

  vec3 sunDir = normalize(-u_sunDirAtmRadius.xyz);
  vec3 viewPos = vec3(u_viewPos.xyz);

  // Compute the center of the voxel in worldspace (relative to the planet).
  vec2 pixelPos = vec2(invoke.xy) + vec2(0.5);
  vec3 clipSpace = vec3(vec2(2.0) * (pixelPos * texelSize.xy) - vec2(1.0), 0);
  vec4 hPos = u_invViewProjMatrix * vec4(clipSpace, 1.0);

  vec3 camPos = (u_camPosition * 1e-6) + viewPos;
  vec3 worlDir = normalize(hPos.xyz);

  float slice = (float(invoke.z) + 0.5) * texelSize.z;
  slice *= slice;
  slice *= tex.z;

  // Cover the entire view frustum.
  float planeDelta = (u_nearFar.y - u_nearFar.x) * texelSize.z * 1e-6;

  float tMax = planeDelta * slice;
  vec3 newWorldPos = camPos + (tMax * worlDir);
  float voxelHeight = length(newWorldPos);
  if (voxelHeight <= groundRadiusMM + PLANET_RADIUS_OFFSET)
  {
    float offset = groundRadiusMM + PLANET_RADIUS_OFFSET + 0.001;
    newWorldPos = normalize(newWorldPos) * offset;
    tMax = length(newWorldPos - camPos);
  }

  vec4 result = raymarchScattering(camPos, worlDir, sunDir, tMax, u_params,
                                   groundRadiusMM, atmosphereRadiusMM);

  // Density AA as described in:
  // http://advances.realtimerendering.com/s2021/jpatry_advances2021/index.html
  result.rgb /= result.a;

  imageStore(aerialLUT, invoke, result);
}

vec3 getValFromLUT(sampler2D tex, vec3 pos, vec3 sunDir,
                   float groundRadiusMM, float atmosphereRadiusMM)
{
  float height = length(pos);
  vec3 up = pos / height;

  float sunCosZenithAngle = dot(sunDir, up);

  float u = clamp(0.5 + 0.5 * sunCosZenithAngle, 0.0, 1.0);
  float v = max(0.0, min(1.0, (height - groundRadiusMM) / (atmosphereRadiusMM - groundRadiusMM)));

  return texture(tex, vec2(u, v)).rgb;
}

vec3 computeExtinction(vec3 pos, ScatteringParams params, float groundRadiusMM)
{
  float altitudeKM = (length(pos) - groundRadiusMM) * 1000.0;

  float rayleighDensity = exp(-altitudeKM / params.rayleighScat.w);
  float mieDensity = exp(-altitudeKM / params.mieScat.w);

  vec3 rayleighScattering = params.rayleighScat.rgb * rayleighDensity;
  vec3 rayleighAbsorption = params.rayleighAbs.rgb * rayleighDensity;

  vec3 mieScattering = params.mieScat.rgb * mieDensity;
  vec3 mieAbsorption = params.mieAbs.rgb * mieDensity;

  vec3 ozoneAbsorption = params.ozoneAbs.w * params.ozoneAbs.rgb * max(0.0, 1.0 - abs(altitudeKM - 25.0) / 15.0);

  return rayleighScattering + vec3(rayleighAbsorption + mieScattering + mieAbsorption) + ozoneAbsorption;
}

vec3 computeRayleighScattering(vec3 pos, ScatteringParams params, float groundRadiusMM)
{
  float altitudeKM = (length(pos) - groundRadiusMM) * 1000.0;
  float rayleighDensity = exp(-altitudeKM / params.rayleighScat.w);

  return params.rayleighScat.rgb * rayleighDensity;
}

vec3 computeMieScattering(vec3 pos, ScatteringParams params, float groundRadiusMM)
{
  float altitudeKM = (length(pos) - groundRadiusMM) * 1000.0;
  float mieDensity = exp(-altitudeKM / params.mieScat.w);

  return params.mieScat.rgb * mieDensity;
}

float getMiePhase(float cosTheta)
{
  const float g = 0.8;
  const float scale = 3.0 / (8.0 * PI);

  float num = (1.0 - g * g) * (1.0 + cosTheta * cosTheta);
  float denom = (2.0 + g * g) * pow((1.0 + g * g - 2.0 * g * cosTheta), 1.5);

  return scale * num / denom;
}

float getRayleighPhase(float cosTheta)
{
  const float k = 3.0 / (16.0 * PI);
  return k * (1.0 + cosTheta * cosTheta);
}

// TODO: Add in global CSM for volumetric shadows.
vec4 raymarchScattering(vec3 pos, vec3 rayDir, vec3 sunDir, float tMax,
                        ScatteringParams params, float groundRadiusMM,
                        float atmoRadiusMM)
{
  float cosTheta = dot(rayDir, sunDir);

  float miePhaseValue = getMiePhase(cosTheta);
  float rayleighPhaseValue = getRayleighPhase(-cosTheta);

  vec3 lum = vec3(0.0);
  vec3 transmittance = vec3(1.0);
  float t = 0.0;
  for (uint i = 0; i < NUM_STEPS; i++)
  {
    float newT = ((float(i) + 0.3) / float(NUM_STEPS)) * tMax;
    float dt = newT - t;
    t = newT;

    vec3 newPos = pos + t * rayDir;

    vec3 rayleighScattering = computeRayleighScattering(newPos, params, groundRadiusMM);
    vec3 extinction = computeExtinction(newPos, params, groundRadiusMM);
    vec3 mieScattering = computeMieScattering(newPos, params, groundRadiusMM);

    vec3 sampleTransmittance = exp(-dt * extinction);

    vec3 sunTransmittance = getValFromLUT(transLUT, newPos, sunDir,
                                          groundRadiusMM, atmoRadiusMM);

    vec3 psiMS = getValFromLUT(multiScatLut, newPos, sunDir,
                               groundRadiusMM, atmoRadiusMM);

    vec3 rayleighInScattering = rayleighScattering * (rayleighPhaseValue * sunTransmittance + psiMS);
    vec3 mieInScattering = mieScattering * (miePhaseValue * sunTransmittance + psiMS);
    vec3 inScattering = (rayleighInScattering + mieInScattering);

    // Integrated scattering within path segment.
    vec3 scatteringIntegral = (inScattering - inScattering * sampleTransmittance) / extinction;

    lum += scatteringIntegral * transmittance;

    transmittance *= sampleTransmittance;
  }

  return vec4(lum, 1.0 - dot(transmittance, vec3(1.0 / 3.0)));
}
