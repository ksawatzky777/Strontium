#type common
#version 460 core
/*
 * Shader program for a 9-tap Gaussian blur in the horizontal direction.
 * TODO: Combine both the horizontal and vertical blur together and use a
 * uniform to swap between them
 */

#type vertex
void main()
{
  vec2 position = vec2(gl_VertexID % 2, gl_VertexID / 2) * 4.0 - 1;

  gl_Position = vec4(position, 0.0, 1.0);
}

#type fragment
layout(binding = 0) uniform sampler2D inputTex;

layout(location = 0) out vec4 fragColour;

vec4 blur9(sampler2D image, vec2 uv, vec2 resolution, vec2 direction);

void main()
{
  vec2 size = textureSize(inputTex, 0);
  vec2 fTexCoords = gl_FragCoord.xy / size;

  fragColour = blur9(inputTex, fTexCoords, size, vec2(1.0, 0.0));
}

vec4 blur9(sampler2D image, vec2 uv, vec2 resolution, vec2 direction)
{
  vec4 color = vec4(0.0);
  vec2 off1 = vec2(1.3846153846) * direction;
  vec2 off2 = vec2(3.2307692308) * direction;
  color += texture2D(image, uv) * 0.2270270270;
  color += texture2D(image, uv + (off1 / resolution)) * 0.3162162162;
  color += texture2D(image, uv - (off1 / resolution)) * 0.3162162162;
  color += texture2D(image, uv + (off2 / resolution)) * 0.0702702703;
  color += texture2D(image, uv - (off2 / resolution)) * 0.0702702703;
  return color;
}
