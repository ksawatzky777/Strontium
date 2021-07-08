#version 440

layout(binding = 0) uniform sampler2D inputTex;

layout(location = 0) out vec4 fragColour;

float weights[5] = float[] (0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

void main()
{
  vec2 texelSize = 1.0 / textureSize(inputTex, 0);
  vec2 fTexCoords = gl_FragCoord.xy * texelSize;
  vec2 result = texture(inputTex, fTexCoords).rg * weights[0];

  for (uint i = 1; i < 5; i++)
  {
    result += texture(inputTex, fTexCoords + vec2(0, texelSize.y * i)).rg * weights[i];
    result += texture(inputTex, fTexCoords - vec2(0, texelSize.y * i)).rg * weights[i];
  }

  fragColour = vec4(result, 0.0, 1.0);
}
