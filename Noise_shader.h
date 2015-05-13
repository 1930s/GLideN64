const char *noise_fragment_shader =
//
// Description : Array and textureless GLSL 2D simplex noise function.
//      Author : Ian McEwan, Ashima Arts.
//  Maintainer : ijm
//     Lastmod : 20110822 (ijm)
//     License : Copyright (C) 2011 Ashima Arts. All rights reserved.
//               Distributed under the MIT License. See LICENSE file.
//               https://github.com/ashima/webgl-noise
//
//
#ifdef SHADER_PRECISION
"#version 150 core													\n"
"mediump vec3 mod289(mediump vec3 x) {								\n"
"  return x - floor(x * (1.0 / 289.0)) * 289.0;						\n"
"}																	\n"
"																	\n"
"mediump vec2 mod289(mediump vec2 x) {										\n"
"  return x - floor(x * (1.0 / 289.0)) * 289.0;						\n"
"}																	\n"
"																	\n"
"mediump vec3 permute(mediump vec3 x) {										\n"
"  return mod289(((x*34.0)+1.0)*x);									\n"
"}																	\n"
"																	\n"
"lowp float snoise(in mediump vec2 v)										\n"
"{																\n"
"  const mediump vec4 C = vec4(0.211324865405187,  // (3.0-sqrt(3.0))/6.0	\n"
"                      0.366025403784439,  // 0.5*(sqrt(3.0)-1.0)	\n"
"                     -0.577350269189626,  // -1.0 + 2.0 * C.x		\n"
"                      0.024390243902439); // 1.0 / 41.0			\n"
// First corner
"  mediump vec2 i  = floor(v + dot(v, C.yy) );								\n"
"  mediump vec2 x0 = v -   i + dot(i, C.xx);								\n"
"																	\n"
// Other corners
"  mediump vec2 i1 = (x0.x > x0.y) ? vec2(1.0, 0.0) : vec2(0.0, 1.0);		\n"
"  mediump vec4 x12 = x0.xyxy + C.xxzz;										\n"
"  x12.xy -= i1;													\n"
"																	\n"
// Permutations
"  i = mod289(i); // Avoid truncation effects in permutation		\n"
"  mediump vec3 p = permute( permute( i.y + vec3(0.0, i1.y, 1.0 ))			\n"
"		+ i.x + vec3(0.0, i1.x, 1.0 ));								\n"
"																	\n"
"  mediump vec3 m = max(0.5 - vec3(dot(x0,x0), dot(x12.xy,x12.xy), dot(x12.zw,x12.zw)), 0.0);\n"
"  m = m*m ;														\n"
"  m = m*m ;														\n"
"																	\n"
// Gradients: 41 points uniformly over a line, mapped onto a diamond.
// The ring size 17*17 = 289 is close to a multiple of 41 (41*7 = 287)
"																	\n"
"  mediump vec3 x = 2.0 * fract(p * C.www) - 1.0;					\n"
"  mediump vec3 h = abs(x) - 0.5;									\n"
"  mediump vec3 ox = floor(x + 0.5);								\n"
"  mediump vec3 a0 = x - ox;										\n"
"																	\n"
// Normalise gradients implicitly by scaling m
// Approximation of: m *= inversesqrt( a0*a0 + h*h );
"  m *= 1.79284291400159 - 0.85373472095314 * ( a0*a0 + h*h );		\n"
"																	\n"
// Compute final noise value at P
"  mediump vec3 g;													\n"
"  g.x  = a0.x  * x0.x  + h.x  * x0.y;								\n"
"  g.yz = a0.yz * x12.xz + h.yz * x12.yw;							\n"
"  return 130.0 * dot(m, g);										\n"
"}																	\n"
"																	\n"
#else
"#version 150 core													\n"
"vec3 mod289(vec3 x) {												\n"
"  return x - floor(x * (1.0 / 289.0)) * 289.0;						\n"
"}																	\n"
"																	\n"
"vec2 mod289(vec2 x) {												\n"
"  return x - floor(x * (1.0 / 289.0)) * 289.0;						\n"
"}																	\n"
"																	\n"
"vec3 permute(vec3 x) {												\n"
"  return mod289(((x*34.0)+1.0)*x);									\n"
"}																	\n"
"																	\n"
"float snoise(in vec2 v)												\n"
"  {																\n"
"  const vec4 C = vec4(0.211324865405187,  // (3.0-sqrt(3.0))/6.0	\n"
"                      0.366025403784439,  // 0.5*(sqrt(3.0)-1.0)	\n"
"                     -0.577350269189626,  // -1.0 + 2.0 * C.x		\n"
"                      0.024390243902439); // 1.0 / 41.0			\n"
// First corner
"  vec2 i  = floor(v + dot(v, C.yy) );								\n"
"  vec2 x0 = v -   i + dot(i, C.xx);								\n"
"																	\n"
// Other corners
"  vec2 i1 = (x0.x > x0.y) ? vec2(1.0, 0.0) : vec2(0.0, 1.0);		\n"
"  vec4 x12 = x0.xyxy + C.xxzz;										\n"
"  x12.xy -= i1;													\n"
"																	\n"
// Permutations
"  i = mod289(i); // Avoid truncation effects in permutation		\n"
"  vec3 p = permute( permute( i.y + vec3(0.0, i1.y, 1.0 ))			\n"
"		+ i.x + vec3(0.0, i1.x, 1.0 ));								\n"
"																	\n"
"  vec3 m = max(0.5 - vec3(dot(x0,x0), dot(x12.xy,x12.xy), dot(x12.zw,x12.zw)), 0.0);\n"
"  m = m*m ;														\n"
"  m = m*m ;														\n"
"																	\n"
// Gradients: 41 points uniformly over a line, mapped onto a diamond.
// The ring size 17*17 = 289 is close to a multiple of 41 (41*7 = 287)
"																	\n"
"  vec3 x = 2.0 * fract(p * C.www) - 1.0;							\n"
"  vec3 h = abs(x) - 0.5;											\n"
"  vec3 ox = floor(x + 0.5);										\n"
"  vec3 a0 = x - ox;												\n"
"																	\n"
// Normalise gradients implicitly by scaling m
// Approximation of: m *= inversesqrt( a0*a0 + h*h );
"  m *= 1.79284291400159 - 0.85373472095314 * ( a0*a0 + h*h );		\n"
"																	\n"
// Compute final noise value at P
"  vec3 g;															\n"
"  g.x  = a0.x  * x0.x  + h.x  * x0.y;								\n"
"  g.yz = a0.yz * x12.xz + h.yz * x12.yw;							\n"
"  return clamp(130.0 * dot(m, g), -1.0, 1.0);						\n"
"}																	\n"
"																	\n"
#endif
;

