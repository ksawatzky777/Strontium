#type common
#version 460 core
/*
 * The post-processing megashader program.
*/

#type vertex
void main()
{
  vec2 position = vec2(gl_VertexID % 2, gl_VertexID / 2) * 4.0 - 1;

  gl_Position = vec4(position, 0.0, 1.0);
}

#type fragment
#define FXAA_SPAN_MAX 8.0
#define FXAA_REDUCE_MUL 0.125
#define FXAA_REDUCE_MIN 0.0078125 // 1.0 / 180.0

// Camera specific uniforms.
layout(std140, binding = 0) uniform CameraBlock
{
  mat4 u_viewMatrix;
  mat4 u_projMatrix;
  mat4 u_invViewProjMatrix;
  vec3 u_camPosition;
  vec4 u_nearFar; // Near plane (x), far plane (y). z and w are unused.
};

// The post processing properties.
layout(std140, binding = 1) uniform PostProcessBlock
{
  vec4 u_camPosScreenSize; // Camera position (x, y, z) and the screen width (w).
  vec4 u_screenSizeGammaBloom;  // Screen height (x), gamma (y) and bloom intensity (z). w is unused.
  ivec4 u_postProcessingPasses; // Tone mapping operator (x), using bloom (y), using FXAA (z) and using sunshafts (w).
};

layout(binding = 0) uniform sampler2D screenColour;
layout(binding = 1) uniform sampler2D entityIDs;
layout(binding = 2) uniform sampler2D bloomColour;

// Output colour variable.
layout(location = 0) out vec4 fragColour;
layout(location = 1) out float fragID;

// Helper functions.
float rgbToLuminance(vec3 rgbColour);

// Bloom.
vec3 blendBloom(vec2 bloomTexCoords, sampler2D bloomTexture, float bloomIntensity);

// FXAA.
vec3 applyFXAA(vec2 screenSize, vec2 uv, sampler2D screenTexture);

// Tone mapping.
vec3 toneMap(vec3 colour, uint operator);

// Gamma correct.
vec3 applyGamma(vec3 colour, float gamma);

void main()
{
  vec2 screenSize = vec2(textureSize(screenColour, 0).xy);
  vec2 fTexCoords = gl_FragCoord.xy / screenSize;

  vec3 colour;
  if (u_postProcessingPasses.z != 0)
    colour = applyFXAA(screenSize, fTexCoords, screenColour);
  else
    colour = texture(screenColour, fTexCoords).rgb;

  if (u_postProcessingPasses.y != 0)
    colour += blendBloom(fTexCoords, bloomColour, u_screenSizeGammaBloom.z);

  colour = toneMap(colour, uint(u_postProcessingPasses.x));
  colour = applyGamma(colour, u_screenSizeGammaBloom.y);

  fragColour = vec4(colour, 1.0);
  fragID = texture(entityIDs, fTexCoords).a;
}

// Helper functions.
float rgbToLuminance(vec3 rgbColour)
{
  return dot(rgbColour, vec3(0.2126f, 0.7152f, 0.0722f));
}

/*
  A  B  C
  D  E  F
  G  H  I
*/
// http://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare
vec3 upsampleBoxTent(sampler2D bloomTexture, vec2 uv, float radius)
{
  vec2 texelSize = 1.0.xx / vec2(textureSize(bloomTexture, 0).xy);
  vec4 d = texelSize.xyxy * vec4(1.0, 1.0, -1.0, 0.0) * radius;

  vec3 A = textureLod(bloomTexture, uv - d.xy, 0).rgb;
  vec3 B = textureLod(bloomTexture, uv - d.wy, 0).rgb * 2.0;
  vec3 C = textureLod(bloomTexture, uv - d.zy, 0).rgb;
  vec3 D = textureLod(bloomTexture, uv + d.zw, 0).rgb * 2.0;
  vec3 E = textureLod(bloomTexture, uv, 0).rgb * 4.0;
  vec3 F = textureLod(bloomTexture, uv + d.xw, 0).rgb * 2.0;
  vec3 G = textureLod(bloomTexture, uv + d.zy, 0).rgb;
  vec3 H = textureLod(bloomTexture, uv + d.wy, 0).rgb * 2.0;
  vec3 I = textureLod(bloomTexture, uv + d.xy, 0).rgb;

  return (A + B + C + D + E + F + G + H + I) * 0.0625; // * 1/16
}

// Bloom.
vec3 blendBloom(vec2 bloomTexCoords, sampler2D bloomTexture, float bloomIntensity)
{
  // Final upsample.
  return upsampleBoxTent(bloomTexture, bloomTexCoords, 1.0) * bloomIntensity;
}

// FXAA.
vec3 applyFXAA(vec2 screenSize, vec2 uv, sampler2D screenTexture)
{
  vec2 texelSize = 1.0 / screenSize;
  vec4 offsetPos = vec4(texelSize, texelSize) * vec4(-1.0, 1.0, 1.0, -1.0);

  vec3 rgbNW = max(texture2D(screenTexture, uv + vec2(-1.0, -1.0) * texelSize).xyz, vec3(0.0));
  vec3 rgbNE = max(texture2D(screenTexture, uv + vec2(1.0, -1.0) * texelSize).xyz, vec3(0.0));
  vec3 rgbSW = max(texture2D(screenTexture, uv + vec2(-1.0, 1.0) * texelSize).xyz, vec3(0.0));
  vec3 rgbSE = max(texture2D(screenTexture, uv + vec2(1.0, 1.0) * texelSize).xyz, vec3(0.0));
  vec3 rgbM = max(texture2D(screenTexture, uv).xyz, vec3(0.0));

  vec3 luma = vec3(0.299, 0.587, 0.114);
  float lumaNW = dot(rgbNW, luma);
  float lumaNE = dot(rgbNE, luma);
  float lumaSW = dot(rgbSW, luma);
  float lumaSE = dot(rgbSE, luma);
  float lumaM  = dot(rgbM,  luma);

  float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
  float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

  vec2 dir;
  dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
  dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));

  float dirReduce = max(
      (lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * FXAA_REDUCE_MUL),
      FXAA_REDUCE_MIN);

  float rcpDirMin = 1.0 / max(min(abs(dir.x), abs(dir.y)) + dirReduce, 1e-4);

  dir = min(vec2(FXAA_SPAN_MAX, FXAA_SPAN_MAX),
        max(vec2(-FXAA_SPAN_MAX, -FXAA_SPAN_MAX),
        dir * rcpDirMin)) / screenSize;

  vec3 rgbA = 0.5 * (
      max(texture2D(screenTexture, uv + dir * (1.0 / 3.0 - 0.5)).xyz, vec3(0.0)) +
      max(texture2D(screenTexture, uv + dir * (2.0 / 3.0 - 0.5)).xyz, vec3(0.0)));
  vec3 rgbB = 0.5 * rgbA + 0.25 * (
      max(texture2D(screenTexture, uv + dir * -0.5).xyz, vec3(0.0)) +
      max(texture2D(screenTexture, uv + dir * 0.5).xyz, vec3(0.0)));

  float lumaB = dot(rgbB, luma);
  if ((lumaB < lumaMin) || (lumaB > lumaMax))
    return rgbA;
  else
    return rgbB;
}

vec3 reinhardOperator(vec3 rgbColour)
{
  return rgbColour / (vec3(1.0) + rgbColour);
}

vec3 luminanceReinhardOperator(vec3 rgbColour)
{
  float luminance = rgbToLuminance(rgbColour);

  return rgbColour / (1.0 + luminance);
}

vec3 luminanceReinhardJodieOperator(vec3 rgbColour)
{
  float luminance = rgbToLuminance(rgbColour);
  vec3 tv = rgbColour / (vec3(1.0) + rgbColour);

  return mix(rgbColour / (1.0 + luminance), tv, tv);
}

vec3 partialUnchartedOperator(vec3 rgbColour)
{
  const float a = 0.15;
  const float b = 0.50;
  const float c = 0.10;
  const float d = 0.20;
  const float e = 0.02;
  const float f = 0.30;

  return ((rgbColour * (a * rgbColour + c * b) + d * e)
          / (rgbColour * (a * rgbColour + b) + d * f)) - e / f;
}

vec3 filmicUnchartedOperator(vec3 rgbColour)
{
  float bias = 2.0;
  vec3 current = partialUnchartedOperator(bias * rgbColour);

  vec3 white = vec3(11.2);
  vec3 whiteScale = vec3(1.0) / partialUnchartedOperator(white);

  return current * whiteScale;
}

vec3 fastAcesOperator(vec3 rgbColour)
{
  rgbColour *= 0.6;
  const float a = 2.51;
  const float b = 0.03;
  const float c = 2.43;
  const float d = 0.59;
  const float e = 0.14;

  return clamp((rgbColour * (a * rgbColour + b))
               / (rgbColour * (c * rgbColour + d) + e), 0.0f, 1.0f);
}

vec3 acesOperator(vec3 rgbColour)
{
  const mat3 inputMatrix = mat3
  (
    vec3(0.59719, 0.07600, 0.02840),
    vec3(0.35458, 0.90834, 0.13383),
    vec3(0.04823, 0.01566, 0.83777)
  );

  const mat3 outputMatrix = mat3
  (
    vec3(1.60475, -0.10208, -0.00327),
    vec3(-0.53108, 1.10813, -0.07276),
    vec3(-0.07367, -0.00605, 1.07602)
  );

  vec3 inputColour = inputMatrix * rgbColour;
  vec3 a = inputColour * (inputColour + vec3(0.0245786)) - vec3(0.000090537);
  vec3 b = inputColour * (0.983729 * inputColour + 0.4329510) + 0.238081;
  vec3 c = a / b;
  return max(outputMatrix * c, 0.0.xxx);
}

// Tone mapping.
vec3 toneMap(vec3 colour, uint operator)
{
  switch (operator)
  {
    case 0: return reinhardOperator(colour);
    case 1: return luminanceReinhardOperator(colour);
    case 2: return luminanceReinhardJodieOperator(colour);
    case 3: return filmicUnchartedOperator(colour);
    case 4: return fastAcesOperator(colour);
    case 5: return acesOperator(colour);
    default: return colour;
  }
}

// Gamma correct.
vec3 applyGamma(vec3 colour, float gamma)
{
  return pow(colour, vec3(1.0 / gamma));
}
