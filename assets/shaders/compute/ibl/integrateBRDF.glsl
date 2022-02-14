#type compute
#version 460 core
/*
 * A compute shader to integrate the BRDF function. Many thanks
 * to http://alinloghin.com/articles/compute_ibl.html, some of the code here was
 * adapted from their excellent article.
*/

#define PI 3.141592654

layout(local_size_x = 8, local_size_y = 8) in;

// The output BRDF integration LUT.
layout(rg16f, binding = 3) restrict writeonly uniform image2D brdfIntMap;

// Schlick-Beckmann geometry function.
float SBGeometry(float NdotV, float roughness);
// Smith-Schlick-Beckmann geometry function.
float SSBGeometry(vec3 N, vec3 V, vec3 L, float roughness);
// Hammersley sequence pseudorandom number generator.
vec2 Hammersley(uint i, uint N);
// Importance sampling of the Smith-Schlick-Beckmann geometry function.
vec3 SSBImportance(vec2 Xi, vec3 N, float roughness);

// Total number of samples. Constant for now, might expose this later.
const uint SAMPLE_COUNT = 1024u;

void main()
{
  ivec2 invoke = ivec2(gl_GlobalInvocationID.xy);

  vec2 size = vec2(imageSize(brdfIntMap).xy);

  float NdotV = (float(invoke.x) + 0.5) / size.x;
  float roughness = (float(invoke.y) + 0.5) / size.y;

  vec3 V;
  V.x = sqrt(1.0 - NdotV * NdotV);
  V.y = 0.0;
  V.z = NdotV;

  float A = 0.0;
  float B = 0.0;

  vec3 N = vec3(0.0, 0.0, 1.0);

  for (uint i = 0u; i < SAMPLE_COUNT; ++i)
  {
    vec2 Xi = Hammersley(i, SAMPLE_COUNT);
    vec3 H = SSBImportance(Xi, N, roughness);
    vec3 L = normalize(2.0 * dot(V, H) * H - V);

    float NdotL = max(L.z, 0.0);
    float NdotH = max(H.z, 0.0);
    float VdotH = max(dot(V, H), 0.0);

    if (NdotL > 0.0)
    {
      float G = SSBGeometry(N, V, L, roughness);
      float G_Vis = (G * VdotH) / max(NdotH * NdotV, 1e-4);
      float Fc = pow(1.0 - VdotH, 5.0);

      A += Fc * G_Vis;
      B += G_Vis;
    }
  }

  A /= float(SAMPLE_COUNT);
  B /= float(SAMPLE_COUNT);

  imageStore(brdfIntMap, invoke, vec4(A, B, 0.0, 1.0));
}

// Schlick-Beckmann geometry function.
float SBGeometry(float NdotV, float roughness)
{
  float a = roughness;
  float k = (a * a) / 2.0;

  float nom   = NdotV;
  float denom = NdotV * (1.0 - k) + k;

  return nom / max(denom, 1e-4);
}

// Smith-Schlick-Beckmann geometry function.
float SSBGeometry(vec3 N, vec3 V, vec3 L, float roughness)
{
  float NdotV = max(dot(N, V), 0.0);
  float NdotL = max(dot(N, L), 0.0);
  float ggx2 = SBGeometry(NdotV, roughness);
  float ggx1 = SBGeometry(NdotL, roughness);

  return ggx1 * ggx2;
}

// Hammersley sequence pseudorandom number generator.
vec2 Hammersley(uint i, uint N)
{
  float fbits;
  uint bits = i;

  bits  = (bits << 16u) | (bits >> 16u);
  bits  = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
  bits  = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
  bits  = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
  bits  = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
  fbits = float(bits) * 2.3283064365386963e-10;

  return vec2(float(i) / float(N), fbits);
}

// Importance sampling of the Smith-Schlick-Beckmann geometry function.
vec3 SSBImportance(vec2 Xi, vec3 N, float roughness)
{
	float a = roughness * roughness;

	float phi = 2.0 * PI * Xi.x;
	float cosTheta = sqrt((1.0 - Xi.y) / max(1.0 + (a*a - 1.0) * Xi.y, 1e-4));
	float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

	// from spherical coordinates to cartesian coordinates - halfway vector
	vec3 H;
	H.x = cos(phi) * sinTheta;
	H.y = sin(phi) * sinTheta;
	H.z = cosTheta;

	// from tangent-space H vector to world-space sample vector
	vec3 up        = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
	vec3 tangent   = normalize(cross(up, N));
	vec3 bitangent = cross(N, tangent);

	vec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
	return normalize(sampleVec);
}
