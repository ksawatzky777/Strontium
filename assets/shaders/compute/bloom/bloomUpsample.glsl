#type compute
#version 460 core
/*
 * A bloom blurring compute shader.
*/

layout(local_size_x = 8, local_size_y = 8) in;

// The equivalent downsample mip to the next mip in the upsample chain, it will
// be filtered and added to the previous mip of the upsampling chain.
layout(rgba16f, binding = 0) readonly uniform image2D blurImage;

// The next mip in the upsampling mip chain.
layout(rgba16f, binding = 1) restrict writeonly uniform image2D nextImage;

layout(std140, binding = 3) readonly buffer PrefilterParams
{
  vec4 u_filterParams; // Threshold (x), threshold - knee (y), 2.0 * knee (z) and 0.25 / knee (w).
  vec2 u_upsampleRadius; // Upsampling filter radius (x).
};

// Interpolating sampler.
vec3 samplePrevious(ivec2 centerCoords, ivec2 offsetCoords, float radius);

// The upsampling tend filter.
vec3 upsampleBoxTent(ivec2 coords, float radius);

void main()
{
  ivec2 sourceCoords = ivec2(gl_GlobalInvocationID.xy);

  vec3 filteredColour = upsampleBoxTent(sourceCoords, u_upsampleRadius.x);
  imageStore(nextImage, sourceCoords, vec4(filteredColour, 1.0));
}

// Interpolating sampler.
vec3 samplePrevious(ivec2 centerCoords, ivec2 offsetCoords, float radius)
{
  ivec2 sampleCoords0 = centerCoords + int(floor(radius)) * offsetCoords;
  vec3 sample0 = imageLoad(blurImage, sampleCoords0).rgb;

  ivec2 sampleCoords1 = centerCoords + int(ceil(radius)) * offsetCoords;
  vec3 sample1 = imageLoad(blurImage, sampleCoords1).rgb;

  return mix(sample0, sample1, sign(fract(radius)));
}

/*
  A  B  C
  D  E  F
  G  H  I
*/
// http://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare
vec3 upsampleBoxTent(ivec2 coords, float radius)
{
  vec3 A = samplePrevious(coords, ivec2(-1, 1), radius);
  vec3 B = samplePrevious(coords, ivec2(0, 1), radius) * 2.0;
  vec3 C = samplePrevious(coords, ivec2(1, 1), radius);
  vec3 D = samplePrevious(coords, ivec2(-1, 0), radius) * 2.0;
  vec3 E = samplePrevious(coords, ivec2(0, 0), radius) * 4.0;
  vec3 F = samplePrevious(coords, ivec2(1, 0), radius) * 2.0;
  vec3 G = samplePrevious(coords, ivec2(-1, -1), radius);
  vec3 H = samplePrevious(coords, ivec2(0, -1), radius) * 2.0;
  vec3 I = samplePrevious(coords, ivec2(1, -1), radius);

  return (A + B + C + D + E + F + G + H + I) * 0.0625; // * 1/16
}
