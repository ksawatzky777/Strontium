#type common
#version 460 core
/*
 * The skybox shader. Works with an HDR skybox, IBL convolutions of the skybox,
 * and several dynamic skies.
 */

// Camera specific uniforms.
layout(std140, binding = 0) uniform CameraBlock
{
  mat4 u_viewMatrix;
  mat4 u_projMatrix;
  mat4 u_invViewProjMatrix;
  vec3 u_camPosition;
  vec4 u_nearFar; // Near plane (x), far plane (y). z and w are unused.
};

// Skybox specific uniforms
layout(std140, binding = 1) uniform SkyboxBlock
{
  vec4 u_lodDirection; // IBL lod (x), sun direction (y, z, w).
  vec4 u_sunIntensitySizeGRadiusARadius; // Sun intensity (x), size (y), ground radius (z) and atmosphere radius (w).
  vec4 u_viewPosSkyIntensity; // View position (x, y, z) and sky intensity (w).
  ivec4 u_skyboxParams2; // The skybox to use (x). y, z and w are unused.
};

#type vertex
out VERT_OUT
{
  vec3 fTexCoords;
} vertOut;

// Taken from: https://gist.github.com/rikusalminen/9393151
vec3 createCube(int vertexID)
{
  int tri = vertexID / 3;
  int idx = vertexID % 3;
  int face = tri / 2;
  int top = tri % 2;

  int dir = face % 3;
  int pos = face / 3;

  int nz = dir >> 1;
  int ny = dir & 1;
  int nx = 1 ^ (ny | nz);

  vec3 d = vec3(nx, ny, nz);
  float flip = 1 - 2 * pos;

  vec3 n = flip * d;
  vec3 u = -d.yzx;
  vec3 v = flip * d.zxy;

  float mirror = -1 + 2 * top;

  return n + mirror * (1 - 2 * (idx & 1)) * u + mirror * (1 - 2 * (idx >> 1)) * v;
}

void main()
{
  vec3 cubeCoords = createCube(gl_VertexID);
  vertOut.fTexCoords = cubeCoords;

  mat4 newView = mat4(mat3(u_viewMatrix));
  gl_Position = (u_projMatrix * newView * vec4(cubeCoords, 1.0)).xyww;
}

#type fragment
#define MAX_MIP 4.0

#define TWO_PI 6.283185308
#define PI 3.141592654
#define PI_OVER_TWO 1.570796327

layout(binding = 0) uniform samplerCube skybox;
layout(binding = 1) uniform sampler2D dynamicSkyLUT;
layout(binding = 2) uniform sampler2D transmittanceLUT;

in VERT_OUT
{
  vec3 fTexCoords;
} fragIn;

// Output colour variable.
layout(location = 0) out vec4 fragColour;

// Helper functions.
float rayIntersectSphere(vec3 ro, vec3 rd, float rad);
float safeACos(float x);
vec3 sampleSphericalMap(sampler2D map, vec3 viewDir);
vec3 sampleSkyViewLUT(sampler2D lut, vec3 viewPos, vec3 viewDir,
                      vec3 sunDir, float groundRadiusMM);
vec3 sampleHLUT(sampler2D tex, vec3 pos, vec3 sunDir, float groundRadiusMM,
                float atmosphereRadiusMM);

// Add in the sun disk.
vec3 computeSunLuminance(vec3 e, vec3 s, float size, float intensity);

void main()
{
  vec3 viewDir = normalize(fragIn.fTexCoords);
  vec3 sunPos = normalize(u_lodDirection.yzw);

  float skyIntensity = u_viewPosSkyIntensity.w;
  float sunIntensity = u_sunIntensitySizeGRadiusARadius.x;
  float sunSize = u_sunIntensitySizeGRadiusARadius.y;

  vec3 colour;
  switch (u_skyboxParams2.x)
  {
    // Generic skybox skybox.
    case 0:
    {
      colour = textureLod(skybox, viewDir, u_lodDirection.x * MAX_MIP).rgb;
      break;
    }

    // Preetham dynamic sky.
    case 1:
    {
      sunPos.z *= -1.0;
      colour = computeSunLuminance(viewDir, sunPos, sunSize, sunIntensity);
      colour += skyIntensity * sampleSphericalMap(dynamicSkyLUT, viewDir).rgb;
      break;
    }

    // Hillaire2020 dynamic sky.
    case 2:
    {
      vec3 viewPos = u_viewPosSkyIntensity.xyz;
      float groundRadiusMM =  u_sunIntensitySizeGRadiusARadius.z;
      float atmosphereRadiusMM = u_sunIntensitySizeGRadiusARadius.w;

      sunPos *= -1.0;
      colour = computeSunLuminance(viewDir, sunPos, sunSize, sunIntensity);

      float intGround = float(rayIntersectSphere(viewPos, viewDir, groundRadiusMM) < 0.0);

      vec3 groundTrans = sampleHLUT(transmittanceLUT, viewPos, sunPos, groundRadiusMM,
                                    atmosphereRadiusMM);
      vec3 spaceTrans = sampleHLUT(transmittanceLUT, vec3(0.0, groundRadiusMM, 0.0),
                                   vec3(0.0, 1.0, 0.0), groundRadiusMM,
                                   atmosphereRadiusMM);
      colour *= groundTrans / spaceTrans;

      colour *= intGround;
      colour += skyIntensity * sampleSkyViewLUT(dynamicSkyLUT, viewPos, viewDir, sunPos,
                                                groundRadiusMM);
      break;
    }
  }

  fragColour = vec4(colour, 1.0);
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

float safeACos(float x)
{
  return acos(clamp(x, -1.0, 1.0));
}

vec3 sampleSphericalMap(sampler2D map, vec3 viewDir)
{
  float inclination = asin(viewDir.y);
  float azimuth = atan(viewDir.x, viewDir.z) + PI;

  float u = azimuth / TWO_PI;
  float v = 0.5 * (inclination / PI_OVER_TWO + 1.0);

  return texture(map, vec2(u, v)).rgb;
}

vec3 sampleSkyViewLUT(sampler2D lut, vec3 viewPos, vec3 viewDir,
                      vec3 sunDir, float groundRadiusMM)
{
  float height = length(viewPos);
  vec3 up = viewPos / height;

  float horizonAngle = safeACos(sqrt(height * height - groundRadiusMM * groundRadiusMM) / height);
  float altitudeAngle = horizonAngle - acos(dot(viewDir, up));

  vec3 right = cross(sunDir, up);
  vec3 forward = cross(up, right);

  vec3 projectedDir = normalize(viewDir - up * (dot(viewDir, up)));
  float sinTheta = dot(projectedDir, right);
  float cosTheta = dot(projectedDir, forward);
  float azimuthAngle = atan(sinTheta, cosTheta) + PI;

  float u = azimuthAngle / (TWO_PI);
  float v = 0.5 + 0.5 * sign(altitudeAngle) * sqrt(abs(altitudeAngle) / PI_OVER_TWO);

  return textureLod(lut, vec2(u, v), 0.0).rgb;
}

vec3 sampleHLUT(sampler2D tex, vec3 pos, vec3 sunDir,
                float groundRadiusMM, float atmosphereRadiusMM)
{
  float height = length(pos);
  vec3 up = pos / height;

  float sunCosZenithAngle = dot(sunDir, up);

  float u = clamp(0.5 + 0.5 * sunCosZenithAngle, 0.0, 1.0);
  float v = max(0.0, min(1.0, (height - groundRadiusMM) / (atmosphereRadiusMM - groundRadiusMM)));

  return textureLod(tex, vec2(u, v), 0.0).rgb;
}

vec3 computeSunLuminance(vec3 e, vec3 s, float size, float intensity)
{
  float sunSolidAngle = size * PI * 0.005555556; // 1.0 / 180.0
  float minSunCosTheta = cos(sunSolidAngle);

  float cosTheta = dot(s, e);
  float angle = safeACos(cosTheta);
  float radiusRatio = angle / sunSolidAngle;
  float limbDarkening = sqrt(clamp(1.0 - radiusRatio * radiusRatio, 0.0001, 1.0));

  float comp = float(cosTheta >= minSunCosTheta);
  return intensity * comp * vec3(1.0) * limbDarkening;
}
