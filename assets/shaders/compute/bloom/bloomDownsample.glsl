#type compute
#version 460 core
/*
 * A bloom downsampling compute shader.
*/

layout(local_size_x = 8, local_size_y = 8) in;

// The previous mip in the downsampling mip chain.
layout(rgba16f, binding = 0) readonly uniform image2D previousImage;

// The next mip in the downsampling mip chain.
layout(rgba16f, binding = 1) restrict writeonly uniform image2D nextImage;

vec3 samplePreviousImage(ivec2 sampleCoords);

vec3 downsampleBox13Tap(ivec2 coords);
vec3 bilinearFetch(ivec2 coords);

void main()
{
  ivec2 destCoords = ivec2(gl_GlobalInvocationID.xy);
  ivec2 sourceCoords = 2 * destCoords;

  vec3 colour = downsampleBox13Tap(sourceCoords);
  imageStore(nextImage, destCoords, vec4(colour, 1.0));
}

vec3 samplePreviousImage(ivec2 sampleCoords)
{
  ivec2 readImageSize = imageSize(previousImage);
  ivec2 actualCoords = clamp(sampleCoords, ivec2(0), readImageSize);
  return imageLoad(previousImage, actualCoords).rgb;
}

/*
  A   B   C
    J   K
  D   E   F
    L   M
  G   H   I
*/
// http://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare
vec3 downsampleBox13Tap(ivec2 coords)
{
  vec3 A = bilinearFetch(coords + ivec2(-2, 2));
  vec3 B = bilinearFetch(coords + ivec2(0, 2));
  vec3 C = bilinearFetch(coords + ivec2(2, 2));
  vec3 D = bilinearFetch(coords + ivec2(-2, 0));
  vec3 E = bilinearFetch(coords + ivec2(0, 0));
  vec3 F = bilinearFetch(coords + ivec2(2, 0));
  vec3 G = bilinearFetch(coords + ivec2(-2, -2));
  vec3 H = bilinearFetch(coords + ivec2(0, -2));
  vec3 I = bilinearFetch(coords + ivec2(2, -2));

  vec3 J = bilinearFetch(coords + ivec2(-1, 1));
  vec3 K = bilinearFetch(coords + ivec2(1, 1));
  vec3 L = bilinearFetch(coords + ivec2(-1, -1));
  vec3 M = bilinearFetch(coords + ivec2(1, -1));

  vec2 weights = 0.25 * vec2(0.5, 0.125);
  vec3 result = (J + K + L + M) * weights.x;
  result += (A + B + D + E) * weights.y;
  result += (B + C + E + F) * weights.y;
  result += (D + E + G + H) * weights.y;
  result += (E + F + H + I) * weights.y;

  return result;
}

vec3 bilinearFetch(ivec2 coords)
{
  vec3 current = samplePreviousImage(coords).rgb;
  vec3 right = samplePreviousImage(coords + ivec2(1, 0)).rgb;
  vec3 top = samplePreviousImage(coords + ivec2(0, 1)).rgb;
  vec3 topRight = samplePreviousImage(coords + ivec2(1, 1)).rgb;

  return 0.25 * (current + right + top + topRight);
}
