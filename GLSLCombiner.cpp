#ifndef __LINUX__
# include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#else
# include "winlnxdefs.h"
# include <stdlib.h> // malloc()
#endif
#include <assert.h>
#include "OpenGL.h"
#include "Combiner.h"
#include "GLSLCombiner.h"
#include "Noise_shader.h"

static GLhandleARB g_vertex_shader_object;
static GLhandleARB g_calc_light_shader_object;
static GLhandleARB g_calc_lod_shader_object;
static GLhandleARB g_calc_noise_shader_object;
static GLhandleARB g_calc_depth_shader_object;
static GLhandleARB g_test_alpha_shader_object;
static GLuint g_zlut_tex = 0;

static
void display_warning(const char *text, ...)
{
	static int first_message = 100;
	if (first_message)
	{
		char buf[1000];

		va_list ap;

		va_start(ap, text);
		vsprintf(buf, text, ap);
		va_end(ap);
		first_message--;
	}
}

const char *ColorInput_1cycle[] = {
	"combined_color.rgb",
	"readtex0.rgb",
	"readtex1.rgb",
	"prim_color.rgb",
	"vec_color.rgb",
	"env_color.rgb",
	"center_color.rgb",
	"scale_color.rgb",
	"combined_color.a",
	"readtex0.a",
	"readtex1.a",
	"prim_color.a",
	"vec_color.a",
	"env_color.a",
	"lod_frac", // TODO: emulate lod_fraction
	"vec3(prim_lod)",
	"vec3(0.5 + 0.5*snoise(noiseCoord2D))",
	"vec3(k4)",
	"vec3(k5)",
	"vec3(1.0)",
	"vec3(0.0)"
};

const char *ColorInput_2cycle[] = {
	"combined_color.rgb",
	"readtex1.rgb",
	"readtex0.rgb",
	"prim_color.rgb",
	"vec_color.rgb",
	"env_color.rgb",
	"center_color.rgb",
	"scale_color.rgb",
	"combined_color.a",
	"readtex1.a",
	"readtex0.a",
	"prim_color.a",
	"vec_color.a",
	"env_color.a",
	"lod_frac", // TODO: emulate lod_fraction
	"vec3(prim_lod)",
	"vec3(0.5 + 0.5*snoise(noiseCoord2D))",
	"vec3(k4)",
	"vec3(k5)",
	"vec3(1.0)",
	"vec3(0.0)"
};

const char *AlphaInput_1cycle[] = {
	"combined_color.a",
	"readtex0.a",
	"readtex1.a",
	"prim_color.a",
	"vec_color.a",
	"env_color.a",
	"center_color.a",
	"scale_color.a",
	"combined_color.a",
	"readtex0.a",
	"readtex1.a",
	"prim_color.a",
	"vec_color.a",
	"env_color.a",
	"lod_frac", // TODO: emulate lod_fraction
	"prim_lod",
	"1.0",
	"k4",
	"k5",
	"1.0",
	"0.0"
};

const char *AlphaInput_2cycle[] = {
	"combined_color.a",
	"readtex1.a",
	"readtex0.a",
	"prim_color.a",
	"vec_color.a",
	"env_color.a",
	"center_color.a",
	"scale_color.a",
	"combined_color.a",
	"readtex1.a",
	"readtex0.a",
	"prim_color.a",
	"vec_color.a",
	"env_color.a",
	"lod_frac", // TODO: emulate lod_fraction
	"prim_lod",
	"1.0",
	"k4",
	"k5",
	"1.0",
	"0.0"
};

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

static const char* vertex_shader =
"uniform float time;											\n"
"varying vec2 noiseCoord2D;										\n"
"varying vec4 secondary_color;                                  \n"
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

static const char* depth_compare_shader_int =
"#version 420 core			\n"
"uniform int depthEnabled;	\n"
"uniform int depthCompareEnabled;	\n"
"uniform int depthUpdateEnabled;	\n"
"uniform unsigned int depthPolygonOffset;	\n"
"layout(binding = 0, r16ui) uniform readonly uimage2D zlut_image;\n"
"layout(binding = 1, r16ui) uniform restrict uimage2D depth_image;\n"
"bool depth_compare()				\n"
"{									\n"
"  if (depthEnabled == 0) return true;				\n"
"  ivec2 coord = ivec2(gl_FragCoord.xy);			\n"
"  highp uvec4 depth = imageLoad(depth_image,coord);		\n"
"  highp unsigned int bufZ = depth.r;							\n"
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
"layout(binding = 1, r32f) uniform restrict image2D depth_image;\n"
"bool depth_compare()				\n"
"{									\n"
"  if (depthEnabled == 0) return true;				\n"
"  ivec2 coord = ivec2(gl_FragCoord.xy);			\n"
"  highp vec4 depth = imageLoad(depth_image,coord);		\n"
"  highp float bufZ = depth.r;							\n"
"  highp int iZ = int(gl_FragCoord.z*262143.0);				\n"
"  int y0 = iZ / 512;						\n"
"  int x0 = iZ - 512*y0;					\n"
"  unsigned int icurZ = imageLoad(zlut_image,ivec2(x0,y0)).r;\n"
"  highp float curZ = float(icurZ)/262143.0  - depthPolygonOffset;	\n"
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

void InitZlutTexture()
{
	u16 * zLUT = new u16[0x40000];
	for(int i=0; i<0x40000; i++) {
		u32 exponent = 0;
		u32 testbit = 1 << 17;
		while((i & testbit) && (exponent < 7)) {
			exponent++;
			testbit = 1 << (17 - exponent);
		}

		u32 mantissa = (i >> (6 - (6 < exponent ? 6 : exponent))) & 0x7ff;
		zLUT[i] = (u16)(((exponent << 11) | mantissa) << 2);
	}
	glGenTextures(1, &g_zlut_tex);
	glBindTexture(GL_TEXTURE_2D, g_zlut_tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R16,
		512, 512, 0, GL_RED, GL_UNSIGNED_SHORT,
		zLUT);
	delete[] zLUT;
}

void DestroyZlutTexture()
{
	if (g_zlut_tex > 0)
		glDeleteTextures(1, &g_zlut_tex);
}

void InitGLSLCombiner()
{
	glActiveTextureARB(GL_TEXTURE0_ARB);
	glEnable(GL_TEXTURE_2D);
	glActiveTextureARB(GL_TEXTURE1_ARB);
	glEnable(GL_TEXTURE_2D);

	g_vertex_shader_object = glCreateShaderObjectARB(GL_VERTEX_SHADER_ARB);
	glShaderSourceARB(g_vertex_shader_object, 1, &vertex_shader, NULL);
	glCompileShaderARB(g_vertex_shader_object);

	g_calc_light_shader_object = glCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);
	glShaderSourceARB(g_calc_light_shader_object, 1, &fragment_shader_calc_light, NULL);
	glCompileShaderARB(g_calc_light_shader_object);

	g_calc_lod_shader_object = glCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);
	glShaderSourceARB(g_calc_lod_shader_object, 1, &fragment_shader_calc_lod, NULL);
	glCompileShaderARB(g_calc_lod_shader_object);

	g_calc_noise_shader_object = glCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);
	glShaderSourceARB(g_calc_noise_shader_object, 1, &noise_fragment_shader, NULL);
	glCompileShaderARB(g_calc_noise_shader_object);

	g_test_alpha_shader_object = glCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);
	glShaderSourceARB(g_test_alpha_shader_object, 1, &alpha_test_fragment_shader, NULL);
	glCompileShaderARB(g_test_alpha_shader_object);

	g_calc_depth_shader_object = glCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);
	if (g_bUseFloatDepthTexture)
		glShaderSourceARB(g_calc_depth_shader_object, 1, &depth_compare_shader_float, NULL);
	else
		glShaderSourceARB(g_calc_depth_shader_object, 1, &depth_compare_shader_int, NULL);
	glCompileShaderARB(g_calc_depth_shader_object);

	InitZlutTexture();

/*
const char* base_vertex_shader =
"void main()                                                    \n"
"{                                                              \n"
"  gl_Position = ftransform();                                  \n"
"}                                                              \n"
;

	GLhandleARB base_vertex_shader_object = glCreateShaderObjectARB(GL_VERTEX_SHADER_ARB);
	glShaderSourceARB(base_vertex_shader_object, 1, &base_vertex_shader, NULL);
	glCompileShaderARB(base_vertex_shader_object);
	GLhandleARB test_program = glCreateProgramObjectARB();
	glAttachObjectARB(test_program, base_vertex_shader_object);
	glAttachObjectARB(test_program, g_calc_depth_shader_object);
	glLinkProgramARB(test_program);

	int log_length;
	glGetObjectParameterivARB(test_program, GL_OBJECT_LINK_STATUS_ARB , &log_length);
	if(!log_length)
	{
		const int nLogSize = 1024;
		char shader_log[nLogSize];
		glGetInfoLogARB(g_calc_depth_shader_object, 
			nLogSize, &log_length, shader_log);
		if(log_length) 
			display_warning(shader_log);
		glGetInfoLogARB(base_vertex_shader_object, nLogSize, &log_length, shader_log);
		if(log_length) 
			display_warning(shader_log);
		glGetInfoLogARB(test_program, 
			nLogSize, &log_length, shader_log);
		if(log_length) 
			display_warning(shader_log);
	}
*/	
}

void DestroyGLSLCombiner() {
	ogl_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	DestroyZlutTexture();
}

static
int CompileCombiner(const CombinerStage & _stage, const char** _Input, char * _fragment_shader) {
	char buf[128];
	bool bBracketOpen = false;
	int nRes = 0;
	for (int i = 0; i < _stage.numOps; ++i) {
		switch(_stage.op[i].op) {
			case LOAD:
				sprintf(buf, "(%s ", _Input[_stage.op[i].param1]);
				strcat(_fragment_shader, buf);
				bBracketOpen = true;
				nRes |= 1 << _stage.op[i].param1;
				break;
			case SUB:
				if (bBracketOpen) {
					sprintf(buf, "- %s)", _Input[_stage.op[i].param1]);
					bBracketOpen = false;
				} else
					sprintf(buf, "- %s", _Input[_stage.op[i].param1]);
				strcat(_fragment_shader, buf);
				nRes |= 1 << _stage.op[i].param1;
				break;
			case ADD:
				if (bBracketOpen) {
					sprintf(buf, "+ %s)", _Input[_stage.op[i].param1]);
					bBracketOpen = false;
				} else
					sprintf(buf, "+ %s", _Input[_stage.op[i].param1]);
				strcat(_fragment_shader, buf);
				nRes |= 1 << _stage.op[i].param1;
				break;
			case MUL:
				if (bBracketOpen) {
					sprintf(buf, ")*%s", _Input[_stage.op[i].param1]);
					bBracketOpen = false;
				} else
					sprintf(buf, "*%s", _Input[_stage.op[i].param1]);
				strcat(_fragment_shader, buf);
				nRes |= 1 << _stage.op[i].param1;
				break;
			case INTER:
				sprintf(buf, "mix(%s, %s, %s)", _Input[_stage.op[0].param2], _Input[_stage.op[0].param1], _Input[_stage.op[0].param3]);
				strcat(_fragment_shader, buf);
				nRes |= 1 << _stage.op[i].param1;
				nRes |= 1 << _stage.op[i].param2;
				nRes |= 1 << _stage.op[i].param3;
				break;

				//			default:
				//				assert(false);
		}
	}
	if (bBracketOpen)
		strcat(_fragment_shader, ")");
	strcat(_fragment_shader, "; \n");
	return nRes;
}

GLSLCombiner::GLSLCombiner(Combiner *_color, Combiner *_alpha) {
	m_vertexShaderObject = g_vertex_shader_object;

	char *fragment_shader = (char*)malloc(8192);
	strcpy(fragment_shader, fragment_shader_header_common_variables);

	char strCombiner[512];
	strcpy(strCombiner, "  alpha1 = ");
	m_nInputs = CompileCombiner(_alpha->stage[0], AlphaInput_1cycle, strCombiner);
	strcat(strCombiner, "  color1 = ");
	m_nInputs |= CompileCombiner(_color->stage[0], ColorInput_1cycle, strCombiner);
	strcat(strCombiner, "  combined_color = vec4(color1, alpha1); \n");
	if (_alpha->numStages == 2) {
		strcat(strCombiner, "  alpha2 = ");
		m_nInputs |= CompileCombiner(_alpha->stage[1], AlphaInput_2cycle, strCombiner);
	} else
		strcat(strCombiner, "  alpha2 = alpha1; \n");
	if (_color->numStages == 2) {
		strcat(strCombiner, "  color2 = ");
		m_nInputs |= CompileCombiner(_color->stage[1], ColorInput_2cycle, strCombiner);
	} else
		strcat(strCombiner, "  color2 = color1; \n");

	strcat(fragment_shader, fragment_shader_header_common_functions);
	strcat(fragment_shader, fragment_shader_header_main);
	const bool bUseLod = (m_nInputs & (1<<LOD_FRACTION)) > 0;
	if (bUseLod)
		strcat(fragment_shader, "  float lod_frac = calc_lod(prim_lod, 255.0*vec2(secondary_color.g, secondary_color.b));	\n");
	if ((m_nInputs & ((1<<TEXEL0)|(1<<TEXEL1)|(1<<TEXEL0_ALPHA)|(1<<TEXEL1_ALPHA))) > 0) {
		strcat(fragment_shader, fragment_shader_readtex0color);
		strcat(fragment_shader, fragment_shader_readtex1color);
	} else {
		assert(strstr(strCombiner, "readtex") == 0);
	}
	if (bHWLightingCalculation)
		strcat(fragment_shader, "  float intensity = calc_light(int(secondary_color.r), input_color); \n");
	else
		strcat(fragment_shader, "  input_color = gl_Color.rgb;\n");
	strcat(fragment_shader, "  vec_color = vec4(input_color, gl_Color.a); \n");
	strcat(fragment_shader, strCombiner);
	strcat(fragment_shader, "  gl_FragColor = vec4(color2, alpha2); \n");

	strcat(fragment_shader, "  if (!alpha_test(gl_FragColor.a)) discard;	\n");
	strcat(fragment_shader, "  bool bDC = depth_compare(); \n");
//	strcat(fragment_shader, "  if (!depth_compare()) discard; \n");

#ifdef USE_TOONIFY
	strcat(fragment_shader, "  toonify(intensity); \n");
#endif
	strcat(fragment_shader, "  if (fog_enabled > 0) \n");
	strcat(fragment_shader, "    gl_FragColor = vec4(mix(gl_Fog.color.rgb, gl_FragColor.rgb, gl_FogFragCoord), gl_FragColor.a); \n");
	strcat(fragment_shader, fragment_shader_end);

#ifdef USE_TOONIFY
	strcat(fragment_shader, fragment_shader_toonify);
#endif

	m_fragmentShaderObject = glCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);
	glShaderSourceARB(m_fragmentShaderObject, 1, (const GLcharARB**)&fragment_shader, NULL);
	free(fragment_shader);

	glCompileShaderARB(m_fragmentShaderObject);

	m_programObject = glCreateProgramObjectARB();
	glAttachObjectARB(m_programObject, m_fragmentShaderObject);
	glAttachObjectARB(m_programObject, g_calc_depth_shader_object);
	if (bHWLightingCalculation)
		glAttachObjectARB(m_programObject, g_calc_light_shader_object);
	if (bUseLod)
		glAttachObjectARB(m_programObject, g_calc_lod_shader_object);
	glAttachObjectARB(m_programObject, g_test_alpha_shader_object);
	glAttachObjectARB(m_programObject, g_calc_depth_shader_object);
	glAttachObjectARB(m_programObject, g_calc_noise_shader_object);
	glAttachObjectARB(m_programObject, m_vertexShaderObject);
	glLinkProgramARB(m_programObject);
}

void GLSLCombiner::Set() {
	combiner.usesT0 = FALSE;
	combiner.usesT1 = FALSE;
	combiner.usesLOD = (m_nInputs & (1<<LOD_FRACTION)) > 0 ? TRUE : FALSE;

	combiner.vertex.color = COMBINED;
	combiner.vertex.alpha = COMBINED;
	combiner.vertex.secondaryColor = LIGHT;

	glUseProgramObjectARB(m_programObject);

	int texture0_location = glGetUniformLocationARB(m_programObject, "texture0");
	if (texture0_location != -1) {
		glUniform1iARB(texture0_location, 0);
		combiner.usesT0 = TRUE;
	}

	int texture1_location = glGetUniformLocationARB(m_programObject, "texture1");
	if (texture1_location != -1) {
		glUniform1iARB(texture1_location, 1);
		combiner.usesT1 = TRUE;
	}

	UpdateColors();
	UpdateAlphaTestInfo();

#ifdef _DEBUG
	int log_length;
	glGetObjectParameterivARB(m_programObject, GL_OBJECT_LINK_STATUS_ARB , &log_length);
	if(!log_length)
	{
		const int nLogSize = 1024;
		char shader_log[nLogSize];
		glGetInfoLogARB(m_fragmentShaderObject, 
			nLogSize, &log_length, shader_log);
		if(log_length) 
			display_warning(shader_log);
		glGetInfoLogARB(m_vertexShaderObject, nLogSize, &log_length, shader_log);
		if(log_length) 
			display_warning(shader_log);
		glGetInfoLogARB(m_programObject, 
			nLogSize, &log_length, shader_log);
		if(log_length) 
			display_warning(shader_log);
	}
#endif
}

void GLSLCombiner::UpdateColors() {
	int prim_color_location = glGetUniformLocationARB(m_programObject, "prim_color");
	glUniform4fARB(prim_color_location, gDP.primColor.r, gDP.primColor.g, gDP.primColor.b, gDP.primColor.a);

	int env_color_location = glGetUniformLocationARB(m_programObject, "env_color");
	glUniform4fARB(env_color_location, gDP.envColor.r, gDP.envColor.g, gDP.envColor.b, gDP.envColor.a);

	int prim_lod_location = glGetUniformLocationARB(m_programObject, "prim_lod");
	glUniform1fARB(prim_lod_location, gDP.primColor.l);

	if (combiner.usesLOD) {
		BOOL bCalcLOD = gDP.otherMode.textureLOD == G_TL_LOD;
		int lod_en_location = glGetUniformLocationARB(m_programObject, "lod_enabled");
		glUniform1iARB(lod_en_location, bCalcLOD);
		if (bCalcLOD) {
			int scale_x_location = glGetUniformLocationARB(m_programObject, "lod_x_scale");
			glUniform1fARB(scale_x_location, OGL.scaleX);
			int scale_y_location = glGetUniformLocationARB(m_programObject, "lod_y_scale");
			glUniform1fARB(scale_y_location, OGL.scaleY);
			int min_lod_location = glGetUniformLocationARB(m_programObject, "min_lod");
			glUniform1fARB(min_lod_location, gDP.primColor.m);
			int max_tile_location = glGetUniformLocationARB(m_programObject, "max_tile");
			glUniform1iARB(max_tile_location, gSP.texture.level);
			int texture_detail_location = glGetUniformLocationARB(m_programObject, "texture_detail");
			glUniform1iARB(texture_detail_location, gDP.otherMode.textureDetail);
		}
	}
	
	int nDither = (gDP.otherMode.alphaCompare == 3 && (gDP.otherMode.colorDither == 2 || gDP.otherMode.alphaDither == 2)) ? 1 : 0;
	int dither_location = glGetUniformLocationARB(m_programObject, "dither_enabled");
	glUniform1iARB(dither_location, nDither);

	if ((m_nInputs & (1<<NOISE)) + nDither > 0) {
		int time_location = glGetUniformLocationARB(m_programObject, "time");
		glUniform1fARB(time_location, (float)(rand()&255));
	}

	int fog_location = glGetUniformLocationARB(m_programObject, "fog_enabled");
	glUniform1iARB(fog_location, (gSP.geometryMode & G_FOG) > 0 ? 1 : 0);

	int fb8bit_location = glGetUniformLocationARB(m_programObject, "fb_8bit_mode");
	glUniform1iARB(fb8bit_location, 0);

}

void GLSLCombiner::UpdateFBInfo() {
	int nFb8bitMode = 0, nFbFixedAlpha = 0;
	if (cache.current[0] != NULL && cache.current[0]->frameBufferTexture == TRUE) {
		if (cache.current[0]->size == G_IM_SIZ_8b) {
			nFb8bitMode |= 1;
			if (gDP.otherMode.imageRead == 0)
				nFbFixedAlpha |= 1;
		}
	}
	if (cache.current[1] != NULL && cache.current[1]->frameBufferTexture == TRUE) {
		if (cache.current[1]->size == G_IM_SIZ_8b) {
			nFb8bitMode |= 2;
			if (gDP.otherMode.imageRead == 0)
				nFbFixedAlpha |= 2;
		}
	}
	int fb8bit_location = glGetUniformLocationARB(m_programObject, "fb_8bit_mode");
	glUniform1iARB(fb8bit_location, nFb8bitMode);
	int fbFixedAlpha_location = glGetUniformLocationARB(m_programObject, "fb_fixed_alpha");
	glUniform1iARB(fbFixedAlpha_location, nFbFixedAlpha);
}

void GLSLCombiner::UpdateDepthInfo() {
	if (frameBuffer.top == NULL || frameBuffer.top->depth_texture == NULL)
		return;
	int nDepthEnabled = (gSP.geometryMode & G_ZBUFFER) > 0 ? 1 : 0;
	int  depth_enabled_location = glGetUniformLocationARB(m_programObject, "depthEnabled");
	glUniform1iARB(depth_enabled_location, nDepthEnabled);
	if (nDepthEnabled == 0)
		return;

	int  depth_compare_location = glGetUniformLocationARB(m_programObject, "depthCompareEnabled");
	glUniform1iARB(depth_compare_location, gDP.otherMode.depthCompare);
	int  depth_update_location = glGetUniformLocationARB(m_programObject, "depthUpdateEnabled");
	glUniform1iARB(depth_update_location, gDP.otherMode.depthUpdate);
	int  depth_polygon_offset = glGetUniformLocationARB(m_programObject, "depthPolygonOffset");
	if (g_bUseFloatDepthTexture) {
		float fPlygonOffset = gDP.otherMode.depthMode == ZMODE_DEC ? 0.005 : 0.0f;
		glUniform1fARB(depth_polygon_offset, fPlygonOffset);
	} else {
		int iPlygonOffset = gDP.otherMode.depthMode == ZMODE_DEC ? 5 : 0;
		glUniform1iARB(depth_polygon_offset, iPlygonOffset);
	}

	const GLuint ZlutImageUnit = 0;
	const GLuint depthImageUnit = 1;
	GLuint texture = frameBuffer.top->depth_texture->glName;
	glBindImageTexture(ZlutImageUnit, g_zlut_tex, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R16UI);
	GLenum depthTexFormat = g_bUseFloatDepthTexture ? GL_R32F : GL_R16UI;
	glBindImageTexture(depthImageUnit, texture, 0, GL_FALSE, 0, GL_READ_WRITE, depthTexFormat);
}

void GLSL_ClearDepthBuffer() {
	if (frameBuffer.top == NULL || frameBuffer.top->depth_texture == NULL)
		return;

	const u32 numTexels = frameBuffer.top->depth_texture->width*frameBuffer.top->depth_texture->height;

	GLenum type = GL_UNSIGNED_SHORT;
	int dataSize = g_bUseFloatDepthTexture ? sizeof(float) : sizeof(unsigned short);
	char * pData = (char*)malloc(numTexels * dataSize);;

	if (g_bUseFloatDepthTexture) {
		type = GL_FLOAT;
		f32 * pDepth = (f32*)pData;
		for (int i = 0; i < numTexels; i++)
			pDepth[i] = 1.0f;
	} else {
		u16 * pDepth = (u16*)pData;
		for (int i = 0; i < numTexels; i++)
			pDepth[i] = 0xfffc;
	}
	glBindTexture(GL_TEXTURE_2D, frameBuffer.top->depth_texture->glName);


	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
		frameBuffer.top->depth_texture->width, frameBuffer.top->depth_texture->height,
		GL_RED, type, pData);
	free(pData);

	gSP.changed |= CHANGED_TEXTURE | CHANGED_VIEWPORT;
	gDP.changed |= CHANGED_COMBINE;

}

void GLSLCombiner::UpdateAlphaTestInfo() {
	int at_enabled_location = glGetUniformLocationARB(m_programObject, "alphaTestEnabled");
	int at_value_location = glGetUniformLocationARB(m_programObject, "alphaTestValue");
	if ((gDP.otherMode.alphaCompare == G_AC_THRESHOLD) && !(gDP.otherMode.alphaCvgSel))	{
		glUniform1iARB(at_enabled_location, 1);
		glUniform1fARB(at_value_location, gDP.blendColor.a);
	} else if (gDP.otherMode.cvgXAlpha)	{
		glUniform1iARB(at_enabled_location, 1);
		glUniform1fARB(at_value_location, 0.5f);
	} else {
		glUniform1iARB(at_enabled_location, 0);
		glUniform1fARB(at_value_location, 0.0f);
	}
}

void GLSL_RenderDepth() {
#if 0
	ogl_glBindFramebuffer(GL_READ_FRAMEBUFFER, g_zbuf_fbo);
	ogl_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glDrawBuffer( GL_FRONT );
	ogl_glBlitFramebuffer(
		0, 0, OGL.width, OGL.height,
		0, OGL.heightOffset, OGL.width, OGL.heightOffset + OGL.height,
		GL_COLOR_BUFFER_BIT, GL_LINEAR
	);
	glDrawBuffer( GL_BACK );
	ogl_glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	ogl_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, frameBuffer.top != NULL ? frameBuffer.top->fbo : 0);
#else
	if (frameBuffer.top == NULL || frameBuffer.top->depth_texture == NULL)
		return;
	glPushAttrib( GL_ENABLE_BIT | GL_VIEWPORT_BIT );

	glActiveTextureARB( GL_TEXTURE0_ARB );
	glBindTexture(GL_TEXTURE_2D, frameBuffer.top->depth_texture->glName);
//	glBindTexture(GL_TEXTURE_2D, g_zlut_tex);

	Combiner_SetCombine( EncodeCombineMode( 0, 0, 0, TEXEL0, 0, 0, 0, 1, 0, 0, 0, TEXEL0, 0, 0, 0, 1 ) );

			glDisable( GL_BLEND );
			glDisable( GL_ALPHA_TEST );
			glDisable( GL_DEPTH_TEST );
			glDisable( GL_CULL_FACE );
			glDisable( GL_POLYGON_OFFSET_FILL );
			glDisable( GL_FOG );

			glMatrixMode( GL_PROJECTION );
			glLoadIdentity();
 			glOrtho( 0, OGL.width, 0, OGL.height, -1.0f, 1.0f );
			glViewport( 0, OGL.heightOffset, OGL.width, OGL.height );
			glDisable( GL_SCISSOR_TEST );

			float u1, v1;

			u1 = 1.0;
			v1 = 1.0;

			ogl_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
#ifndef __LINUX__
			glDrawBuffer( GL_FRONT );
#else
			glDrawBuffer( GL_BACK );
#endif
			glBegin(GL_QUADS);
 				glTexCoord2f( 0.0f, 0.0f );
				glVertex2f( 0.0f, 0.0f );

				glTexCoord2f( 0.0f, v1 );
				glVertex2f( 0.0f, (GLfloat)OGL.height );

 				glTexCoord2f( u1,  v1 );
				glVertex2f( (GLfloat)OGL.width, (GLfloat)OGL.height );

 				glTexCoord2f( u1, 0.0f );
				glVertex2f( (GLfloat)OGL.width, 0.0f );
			glEnd();
#ifndef __LINUX__
			glDrawBuffer( GL_BACK );
#else
			OGL_SwapBuffers();
#endif
			ogl_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, frameBuffer.top->fbo);

			glLoadIdentity();
			glPopAttrib();

			gSP.changed |= CHANGED_TEXTURE | CHANGED_VIEWPORT;
			gDP.changed |= CHANGED_COMBINE;
#endif
}
