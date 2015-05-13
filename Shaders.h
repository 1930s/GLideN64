static const char* vertex_shader =
"uniform float time;											\n"
"varying vec2 noiseCoord2D;										\n"
"varying vec4 secondary_color;									\n"
"void main()                                                    \n"
"{                                                              \n"
"  gl_Position = ftransform();                                  \n"
"  gl_FrontColor = gl_Color;                                    \n"
"  gl_TexCoord[0] = gl_MultiTexCoord0;                          \n"
"  gl_TexCoord[1] = gl_MultiTexCoord1;                          \n"
"  gl_FogFragCoord = (gl_Fog.end - gl_FogCoord) * gl_Fog.scale;	\n"
"  gl_FogFragCoord = clamp(gl_FogFragCoord, 0.0, 1.0);			\n"
"  secondary_color = gl_SecondaryColor;							\n"
"  noiseCoord2D = gl_Vertex.xy + vec2(0.0, time);				\n"
"}                                                              \n"
;

static const char* fragment_shader_header_common_variables =
"uniform sampler2D texture0;	\n"
"uniform sampler2D texture1;	\n"
"uniform vec4 prim_color;		\n"
"uniform vec4 env_color;		\n"
"uniform vec4 center_color;		\n"
"uniform vec4 scale_color;		\n"
"uniform float k4;				\n"
"uniform float k5;				\n"
"uniform float prim_lod;		\n"
"uniform int dither_enabled;	\n"
"uniform int fog_enabled;		\n"
"uniform int fb_8bit_mode;		\n"
"uniform int fb_fixed_alpha;	\n"
"varying vec4 secondary_color;	\n"
"varying vec2 noiseCoord2D;		\n"
"vec3 input_color;				\n"
;

static const char* fragment_shader_header_common_functions =
"															\n"
"float snoise(vec2 v);										\n"
"float calc_light(in int nLights, out vec3 output_color);	\n"
"float calc_lod(in float prim_lod, in vec2 texCoord);		\n"
"bool depth_compare();										\n"
"bool alpha_test(in float alphaValue);						\n"
#ifdef USE_TOONIFY
"void toonify(in float intensity);	\n"
#endif
;

static const char* fragment_shader_calc_light =
"																\n"
"float calc_light(in int nLights, out vec3 output_color) {		\n"
"  output_color = gl_Color.rgb;									\n"
"  if (nLights == 0)											\n"
"     return 1.0;												\n"
"  float full_intensity = 0.0;									\n"
"  output_color = vec3(gl_LightSource[nLights].ambient);		\n"
"  vec3 lightDir, lightColor;									\n"
"  float intensity;												\n"
"  vec3 n = normalize(gl_Color.rgb);							\n"
"  for (int i = 0; i < nLights; i++)	{						\n"
"    lightDir = vec3(gl_LightSource[i].position);				\n"
"    intensity = max(dot(n,lightDir),0.0);						\n"
"    full_intensity += intensity;								\n"
"    lightColor = vec3(gl_LightSource[i].ambient)*intensity;	\n"
"    output_color += lightColor;								\n"
"  };															\n"
"  return full_intensity;										\n"
"}																\n"
;

static const char* fragment_shader_calc_lod =
"uniform int lod_enabled;		\n"
"uniform float lod_x_scale;		\n"
"uniform float lod_y_scale;		\n"
"uniform float min_lod;			\n"
"uniform int max_tile;			\n"
"uniform int texture_detail;	\n"
"														\n"
"float calc_lod(in float prim_lod, in vec2 texCoord) {	\n"
"  if (lod_enabled == 0)								\n"
"    return prim_lod;									\n"
" vec2 dx = dFdx(texCoord);								\n"
" dx.x *= lod_x_scale;									\n"
" dx.y *= lod_y_scale;									\n"
" vec2 dy = dFdy(texCoord);								\n"
" dy.x *= lod_x_scale;									\n"
" dy.y *= lod_y_scale;									\n"
" float lod = max(length(dx), length(dy));				\n"
" float lod_frac;										\n"
"  if (lod < 1.0) {										\n"
"    lod_frac = max(lod, min_lod);						\n"
"    if (texture_detail == 1)							\n"
"      lod_frac = 1.0 - lod_frac;						\n"
"  } else {												\n"
"    float tile = min(float(max_tile), floor(log2(floor(lod)))); \n"
"    lod_frac = max(min_lod, fract(lod/pow(2.0, tile)));\n"
"  }													\n"
"  return lod_frac;										\n"
"}														\n"
;

static const char* fragment_shader_header_main =
"									\n"
"void main()						\n"
"{									\n"
"  if (dither_enabled > 0)			\n"
"    if (snoise(noiseCoord2D) < 0.0) discard; \n"
"  vec4 vec_color, combined_color;	\n"
"  float alpha1, alpha2;			\n"
"  vec3 color1, color2;				\n"
;

#ifdef USE_TOONIFY
static const char* fragment_shader_toonify =
"																	\n"
"void toonify(in float intensity) {									\n"
"   if (intensity > 0.5)											\n"
"	   return;														\n"
"	else if (intensity > 0.125)										\n"
"		gl_FragColor = vec4(vec3(gl_FragColor)*0.5, gl_FragColor.a);\n"
"	else															\n"
"		gl_FragColor = vec4(vec3(gl_FragColor)*0.2, gl_FragColor.a);\n"
"}																	\n"
;
#endif

static const char* fragment_shader_default =
//"  gl_FragColor = texture2D(texture0, gl_TexCoord[0].st); \n"
//"  gl_FragColor = gl_Color; \n"
"  vec4 color = texture2D(texture0, gl_TexCoord[0].st); \n"
"  gl_FragColor = gl_Color*color; \n"
;

static const char* fragment_shader_readtex0color =
"  vec4 readtex0 = texture2D(texture0, gl_TexCoord[0].st);	\n"
"  if (fb_8bit_mode == 1 || fb_8bit_mode == 3) readtex0 = vec4(readtex0.r);	\n"
"  if (fb_fixed_alpha == 1 || fb_fixed_alpha == 3) readtex0.a = 0.825;	\n"
;

static const char* fragment_shader_readtex1color =
"  vec4 readtex1 = texture2D(texture1, gl_TexCoord[1].st);	\n"
"  if (fb_8bit_mode == 2 || fb_8bit_mode == 3) readtex1 = vec4(readtex1.r);	\n"
"  if (fb_fixed_alpha == 2 || fb_fixed_alpha == 3) readtex1.a = 0.825;	\n"
;

static const char* fragment_shader_end =
"}                               \n"
;

static const char* depth_compare_shader_int =
"#version 420 core			\n"
"uniform int depthEnabled;	\n"
"uniform int depthCompareEnabled;	\n"
"uniform int depthUpdateEnabled;	\n"
"uniform unsigned int depthPolygonOffset;	\n"
"layout(binding = 0, r16ui) uniform readonly uimage2D zlut_image;\n"
"layout(binding = 2, r16ui) uniform restrict uimage2D depth_image;\n"
"bool depth_compare()				\n"
"{									\n"
"  if (depthEnabled == 0) return true;				\n"
"  ivec2 coord = ivec2(gl_FragCoord.xy);			\n"
"  highp uvec4 depth = imageLoad(depth_image,coord);		\n"
"  highp unsigned int bufZ = depth.r;						\n"
"  highp int iZ = int(gl_FragCoord.z*262143.0);				\n"
"  int y0 = iZ / 512;						\n"
"  int x0 = iZ - 512*y0;					\n"
"  highp unsigned int curZ = imageLoad(zlut_image,ivec2(x0,y0)).r; \n"
"  curZ = curZ - depthPolygonOffset;\n"
"  if (depthUpdateEnabled > 0  && curZ < depth.r) {	\n"
"    depth.r = curZ;						\n"
"    imageStore(depth_image,coord,depth);				\n"
"  }													\n"
"  memoryBarrier();										\n"
"  if (depthCompareEnabled > 0)							\n"
"    return curZ <= bufZ;								\n"
"  return true;											\n"
"}														\n"
;

static const char* depth_compare_shader_float =
"#version 420 core			\n"
"uniform int depthEnabled;	\n"
"uniform int depthCompareEnabled;	\n"
"uniform int depthUpdateEnabled;	\n"
"uniform float depthPolygonOffset;	\n"
"layout(binding = 0, r16ui) uniform readonly uimage2D zlut_image;\n"
"layout(binding = 2, r32f) uniform restrict image2D depth_image;\n"
"bool depth_compare()				\n"
"{									\n"
"  if (depthEnabled == 0) return true;				\n"
"  ivec2 coord = ivec2(gl_FragCoord.xy);			\n"
"  highp vec4 depth = imageLoad(depth_image,coord);		\n"
"  highp float bufZ = depth.r;							\n"
"  highp int iZ = max(0, int((gl_FragCoord.z-0.005)*262143.0));				\n"
"  int y0 = clamp(iZ / 512, 0, 511);						\n"
"  int x0 = iZ - 512*y0;					\n"
"  unsigned int icurZ = imageLoad(zlut_image,ivec2(x0,y0)).r;\n"
//"  highp float curZ = clamp(float(icurZ)/65535.0  - depthPolygonOffset, 0.0, 1.0);	\n"
"  highp float curZ = clamp(float(icurZ)/65532.0, 0.0, 1.0);	\n"
"  if (depthUpdateEnabled > 0  && curZ < depth.r) {	\n"
"    depth.r = curZ;						\n"
"    imageStore(depth_image,coord,depth);				\n"
"  }					\n"
"  memoryBarrier();										\n"
"  if (depthCompareEnabled > 0)							\n"
"    return curZ <= bufZ;								\n"
"  return true;											\n"
"}														\n"
;

static const char* alpha_test_fragment_shader =
"uniform int alphaTestEnabled;				\n"
"uniform float alphaTestValue;				\n"
"bool alpha_test(in float alphaValue)		\n"
"{											\n"
"  if (alphaTestEnabled == 0) return true;	\n"
"  if (alphaTestValue > 0.0) return alphaValue >= alphaTestValue;\n"
"  return alphaValue > 0.0;					\n"
"}											\n"
;

static const char* shadow_map_vertex_shader =
"void main()                                                    \n"
"{                                                              \n"
"  gl_Position = ftransform();                                  \n"
"  gl_FrontColor = gl_Color;                                    \n"
"}                                                              \n"
;

static const char* shadow_map_fragment_shader_float =
"#version 420 core											\n"
"layout(binding = 1, r16ui) uniform readonly uimage1D tlut_image;\n"
"layout(binding = 2, r32f) uniform readonly image2D depth_image;\n"
"float get_alpha()											\n"
"{															\n"
"  ivec2 coord = ivec2(gl_FragCoord.xy);					\n"
"  float bufZ = imageLoad(depth_image,coord).r;		\n"
"  int index = min(255, int(bufZ*255.0));					\n"
"  unsigned int iAlpha = imageLoad(tlut_image,index).r; \n"
"  memoryBarrier();										\n"
"  return float(iAlpha/256)/255.0;						\n"
"}														\n"
"void main()											\n"
"{														\n"
"  gl_FragColor = vec4(gl_Fog.color.rgb, get_alpha());	\n"
"}														\n"
;

static const char* shadow_map_fragment_shader_int =
"#version 420 core											\n"
"layout(binding = 1, r16ui) uniform readonly uimage1D tlut_image;\n"
"layout(binding = 2, r16ui) uniform readonly uimage2D depth_image;\n"
"float get_alpha()											\n"
"{															\n"
"  ivec2 coord = ivec2(gl_FragCoord.xy);					\n"
"  unsigned int bufZ = imageLoad(depth_image,coord).r;		\n"
"  int index = min(255, int(bufZ/256));						\n"
"  index += 80;												\n"
"  unsigned int iAlpha = imageLoad(tlut_image,index).r; \n"
"  memoryBarrier();										\n"
"  return float(iAlpha/256)/255.0;						\n"
"}														\n"
"void main()											\n"
"{														\n"
"  gl_FragColor = vec4(gl_Fog.color.rgb, get_alpha());	\n"
"}														\n"
;
