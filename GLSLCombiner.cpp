#include <assert.h>
#include <stdio.h>
#include "N64.h"
#include "OpenGL.h"
#include "Combiner.h"
#include "GLSLCombiner.h"
#include "Shaders.h"
#include "Noise_shader.h"
#include "FrameBuffer.h"
#include "DepthBuffer.h"

static GLuint  g_vertex_shader_object;
static GLuint  g_calc_light_shader_object;
static GLuint  g_calc_lod_shader_object;
static GLuint  g_calc_noise_shader_object;
static GLuint  g_calc_depth_shader_object;
static GLuint  g_test_alpha_shader_object;
static GLuint g_zlut_tex = 0;

static GLuint  g_shadow_map_vertex_shader_object;
static GLuint  g_shadow_map_fragment_shader_object;
static GLuint  g_draw_shadow_map_program;
GLuint g_tlut_tex = 0;
static u32 g_paletteCRC256 = 0;

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

static const GLsizei nShaderLogSize = 1024;
static
bool check_shader_compile_status(GLuint obj)
{
	GLint status;
	glGetShaderiv(obj, GL_COMPILE_STATUS, &status);
	if(status == GL_FALSE)
	{
		GLchar shader_log[nShaderLogSize];
		GLsizei nLogSize = nShaderLogSize;
		glGetShaderInfoLog(obj, nShaderLogSize, &nLogSize, shader_log);
		display_warning(shader_log);
		return false;
	}
	return true;
}

static
bool check_program_link_status(GLuint obj)
{
	GLint status;
	glGetProgramiv(obj, GL_LINK_STATUS, &status);
	if(status == GL_FALSE)
	{
		GLsizei nLogSize = nShaderLogSize;
		GLchar shader_log[nShaderLogSize];
		glGetProgramInfoLog(obj, nShaderLogSize, &nLogSize, shader_log);
		display_warning(shader_log);
		return false;
	}
	return true;
}

static
void InitZlutTexture()
{
	if (!OGL.bImageTexture)
		return;

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
	glBindImageTexture(ZlutImageUnit, g_zlut_tex, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R16UI);
}

static
void DestroyZlutTexture()
{
	if (!OGL.bImageTexture)
		return;
	glBindImageTexture(ZlutImageUnit, 0, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R16UI);
	if (g_zlut_tex > 0)
		glDeleteTextures(1, &g_zlut_tex);
}

static
void InitShadowMapShader()
{
	if (!OGL.bImageTexture)
		return;

	g_paletteCRC256 = 0;
	glGenTextures(1, &g_tlut_tex);
	glBindTexture(GL_TEXTURE_1D, g_tlut_tex);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexImage1D(GL_TEXTURE_1D, 0, GL_R16, 256, 0, GL_RED, GL_UNSIGNED_SHORT, NULL);

	g_shadow_map_vertex_shader_object = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(g_shadow_map_vertex_shader_object, 1, &shadow_map_vertex_shader, NULL);
	glCompileShader(g_shadow_map_vertex_shader_object);
	assert(check_shader_compile_status(g_shadow_map_vertex_shader_object));

	g_shadow_map_fragment_shader_object = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(g_shadow_map_fragment_shader_object, 1, &shadow_map_fragment_shader_float, NULL);
	glCompileShader(g_shadow_map_fragment_shader_object);
	assert(check_shader_compile_status(g_shadow_map_fragment_shader_object));

	g_draw_shadow_map_program = glCreateProgram();
	glAttachShader(g_draw_shadow_map_program, g_shadow_map_vertex_shader_object);
	glAttachShader(g_draw_shadow_map_program, g_shadow_map_fragment_shader_object);
	glLinkProgram(g_draw_shadow_map_program);
	assert(check_program_link_status(g_draw_shadow_map_program));
}

static
void DestroyShadowMapShader()
{
	if (!OGL.bImageTexture)
		return;

	glBindImageTexture(TlutImageUnit, 0, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R16UI);

	if (g_tlut_tex > 0)
		glDeleteTextures(1, &g_tlut_tex);
	glDetachShader(g_draw_shadow_map_program, g_shadow_map_vertex_shader_object);
	glDetachShader(g_draw_shadow_map_program, g_shadow_map_fragment_shader_object);
	glDeleteShader(g_shadow_map_vertex_shader_object);
	glDeleteShader(g_shadow_map_fragment_shader_object);
	glDeleteProgram(g_draw_shadow_map_program);
}

void InitGLSLCombiner()
{
	glActiveTexture(GL_TEXTURE0);
	glEnable(GL_TEXTURE_2D);
	glActiveTexture(GL_TEXTURE1);
	glEnable(GL_TEXTURE_2D);

	g_vertex_shader_object = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(g_vertex_shader_object, 1, &vertex_shader, NULL);
	glCompileShader(g_vertex_shader_object);
	assert(check_shader_compile_status(g_vertex_shader_object));

	g_calc_light_shader_object = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(g_calc_light_shader_object, 1, &fragment_shader_calc_light, NULL);
	glCompileShader(g_calc_light_shader_object);
	assert(check_shader_compile_status(g_calc_light_shader_object));

	g_calc_lod_shader_object = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(g_calc_lod_shader_object, 1, &fragment_shader_calc_lod, NULL);
	glCompileShader(g_calc_lod_shader_object);
	assert(check_shader_compile_status(g_calc_lod_shader_object));

	g_calc_noise_shader_object = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(g_calc_noise_shader_object, 1, &noise_fragment_shader, NULL);
	glCompileShader(g_calc_noise_shader_object);
	assert(check_shader_compile_status(g_calc_noise_shader_object));

	g_test_alpha_shader_object = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(g_test_alpha_shader_object, 1, &alpha_test_fragment_shader, NULL);
	glCompileShader(g_test_alpha_shader_object);
	assert(check_shader_compile_status(g_test_alpha_shader_object));

	if (OGL.bImageTexture) {
		g_calc_depth_shader_object = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(g_calc_depth_shader_object, 1, &depth_compare_shader_float, NULL);
		glCompileShader(g_calc_depth_shader_object);
		assert(check_shader_compile_status(g_calc_depth_shader_object));
	}

	InitZlutTexture();
	InitShadowMapShader();
}

void DestroyGLSLCombiner() {
	ogl_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	DestroyZlutTexture();
	DestroyShadowMapShader();
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
	char *fragment_shader = (char*)malloc(4096);
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
	if (OGL.bHWLighting)
		strcat(fragment_shader, "  float intensity = calc_light(int(secondary_color.r), input_color); \n");
	else
		strcat(fragment_shader, "  input_color = gl_Color.rgb;\n");
	strcat(fragment_shader, "  vec_color = vec4(input_color, gl_Color.a); \n");
	strcat(fragment_shader, strCombiner);
	strcat(fragment_shader, "  gl_FragColor = vec4(color2, alpha2); \n");

	strcat(fragment_shader, "  if (!alpha_test(gl_FragColor.a)) discard;	\n");
	if (OGL.bImageTexture) {
		if (g_bN64DepthCompare)
			strcat(fragment_shader, "  if (!depth_compare()) discard; \n");
		else
			strcat(fragment_shader, "  depth_compare(); \n");
	}

#ifdef USE_TOONIFY
	strcat(fragment_shader, "  toonify(intensity); \n");
#endif
	strcat(fragment_shader, "  if (fog_enabled > 0) \n");
	strcat(fragment_shader, "    gl_FragColor = vec4(mix(gl_Fog.color.rgb, gl_FragColor.rgb, gl_FogFragCoord), gl_FragColor.a); \n");

	strcat(fragment_shader, fragment_shader_end);

#ifdef USE_TOONIFY
	strcat(fragment_shader, fragment_shader_toonify);
#endif

	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, (const GLchar**)&fragment_shader, NULL);
	free(fragment_shader);
	glCompileShader(fragmentShader);
	assert(check_shader_compile_status(fragmentShader));

	memset(m_aShaders, 0, sizeof(m_aShaders));
	u32 uShaderIdx = 0;

	m_program = glCreateProgram();
	glAttachShader(m_program, g_vertex_shader_object);
	m_aShaders[uShaderIdx++] = g_vertex_shader_object;
	glAttachShader(m_program, fragmentShader);
	m_aShaders[uShaderIdx++] = fragmentShader;
	glAttachShader(m_program, g_calc_depth_shader_object);
	m_aShaders[uShaderIdx++] = g_calc_depth_shader_object;
	if (OGL.bHWLighting) {
		glAttachShader(m_program, g_calc_light_shader_object);
		m_aShaders[uShaderIdx++] = g_calc_light_shader_object;
	}
	if (bUseLod) {
		glAttachShader(m_program, g_calc_lod_shader_object);
		m_aShaders[uShaderIdx++] = g_calc_lod_shader_object;
	}
	glAttachShader(m_program, g_test_alpha_shader_object);
	m_aShaders[uShaderIdx++] = g_test_alpha_shader_object;
	if (OGL.bImageTexture) {
		glAttachShader(m_program, g_calc_depth_shader_object);
		m_aShaders[uShaderIdx++] = g_calc_depth_shader_object;
	}
	glAttachShader(m_program, g_calc_noise_shader_object);
	m_aShaders[uShaderIdx] = g_calc_noise_shader_object;
	assert(uShaderIdx <= sizeof(m_aShaders)/sizeof(m_aShaders[0]));
	glLinkProgram(m_program);
	assert(check_program_link_status(m_program));
}

GLSLCombiner::~GLSLCombiner() {
	u32 shaderIndex = 0;
	const u32 arraySize = sizeof(m_aShaders)/sizeof(m_aShaders[0]);
	while (shaderIndex < arraySize && m_aShaders[shaderIndex] > 0) {
		glDetachShader(m_program, m_aShaders[shaderIndex]);
		glDeleteShader(m_aShaders[shaderIndex++]);
	}
	glDeleteProgram(m_program);
}

void GLSLCombiner::Set() {
	combiner.usesT0 = FALSE;
	combiner.usesT1 = FALSE;
	combiner.usesLOD = (m_nInputs & (1<<LOD_FRACTION)) > 0 ? TRUE : FALSE;

	combiner.vertex.color = COMBINED;
	combiner.vertex.alpha = COMBINED;
	combiner.vertex.secondaryColor = LIGHT;

	glUseProgram(m_program);

	int texture0_location = glGetUniformLocation(m_program, "texture0");
	if (texture0_location != -1) {
		glUniform1i(texture0_location, 0);
		combiner.usesT0 = TRUE;
	}

	int texture1_location = glGetUniformLocation(m_program, "texture1");
	if (texture1_location != -1) {
		glUniform1i(texture1_location, 1);
		combiner.usesT1 = TRUE;
	}

	UpdateColors();
	UpdateAlphaTestInfo();
}

void GLSLCombiner::UpdateColors() {
	int prim_color_location = glGetUniformLocation(m_program, "prim_color");
	glUniform4f(prim_color_location, gDP.primColor.r, gDP.primColor.g, gDP.primColor.b, gDP.primColor.a);

	int env_color_location = glGetUniformLocation(m_program, "env_color");
	glUniform4f(env_color_location, gDP.envColor.r, gDP.envColor.g, gDP.envColor.b, gDP.envColor.a);

	int prim_lod_location = glGetUniformLocation(m_program, "prim_lod");
	glUniform1f(prim_lod_location, gDP.primColor.l);

	if (combiner.usesLOD) {
		BOOL bCalcLOD = gDP.otherMode.textureLOD == G_TL_LOD;
		int lod_en_location = glGetUniformLocation(m_program, "lod_enabled");
		glUniform1i(lod_en_location, bCalcLOD);
		if (bCalcLOD) {
			int scale_x_location = glGetUniformLocation(m_program, "lod_x_scale");
			glUniform1f(scale_x_location, OGL.scaleX);
			int scale_y_location = glGetUniformLocation(m_program, "lod_y_scale");
			glUniform1f(scale_y_location, OGL.scaleY);
			int min_lod_location = glGetUniformLocation(m_program, "min_lod");
			glUniform1f(min_lod_location, gDP.primColor.m);
			int max_tile_location = glGetUniformLocation(m_program, "max_tile");
			glUniform1i(max_tile_location, gSP.texture.level);
			int texture_detail_location = glGetUniformLocation(m_program, "texture_detail");
			glUniform1i(texture_detail_location, gDP.otherMode.textureDetail);
		}
	}
	
	int nDither = (gDP.otherMode.alphaCompare == 3 && (gDP.otherMode.colorDither == 2 || gDP.otherMode.alphaDither == 2)) ? 1 : 0;
	int dither_location = glGetUniformLocation(m_program, "dither_enabled");
	glUniform1i(dither_location, nDither);

	if ((m_nInputs & (1<<NOISE)) + nDither > 0) {
		int time_location = glGetUniformLocation(m_program, "time");
		glUniform1f(time_location, (float)(rand()&255));
	}

	int fog_location = glGetUniformLocation(m_program, "fog_enabled");
	glUniform1i(fog_location, (gSP.geometryMode & G_FOG) > 0 ? 1 : 0);

	int fb8bit_location = glGetUniformLocation(m_program, "fb_8bit_mode");
	glUniform1i(fb8bit_location, 0);

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
	int fb8bit_location = glGetUniformLocation(m_program, "fb_8bit_mode");
	glUniform1i(fb8bit_location, nFb8bitMode);
	int fbFixedAlpha_location = glGetUniformLocation(m_program, "fb_fixed_alpha");
	glUniform1i(fbFixedAlpha_location, nFbFixedAlpha);
}

void GLSLCombiner::UpdateDepthInfo() {
	if (!OGL.bImageTexture)
		return;

	if (frameBuffer.top == NULL || frameBuffer.top->pDepthBuffer == NULL)
		return;

	int nDepthEnabled = (gSP.geometryMode & G_ZBUFFER) > 0 ? 1 : 0;
	int  depth_enabled_location = glGetUniformLocation(m_program, "depthEnabled");
	glUniform1i(depth_enabled_location, nDepthEnabled);
	if (nDepthEnabled == 0)
		return;

	const int  depth_mode_location = glGetUniformLocation(m_program, "depthMode");
	glUniform1i(depth_mode_location, gDP.otherMode.depthMode);
	const int  depth_compare_location = glGetUniformLocation(m_program, "depthCompareEnabled");
	glUniform1i(depth_compare_location, gDP.otherMode.depthCompare);
	const int  depth_update_location = glGetUniformLocation(m_program, "depthUpdateEnabled");
	glUniform1i(depth_update_location, gDP.otherMode.depthUpdate);
	const int  depth_scale_location = glGetUniformLocation(m_program, "depthScale");
	glUniform1f(depth_scale_location, gSP.viewport.vscale[2]*32768);
	const int  depth_trans_location = glGetUniformLocation(m_program, "depthTrans");
	glUniform1f(depth_trans_location, gSP.viewport.vtrans[2]*32768);

	GLuint texture = frameBuffer.top->pDepthBuffer->depth_texture->glName;
}

void GLSLCombiner::UpdateAlphaTestInfo() {
	int at_enabled_location = glGetUniformLocation(m_program, "alphaTestEnabled");
	int at_value_location = glGetUniformLocation(m_program, "alphaTestValue");
	if ((gDP.otherMode.alphaCompare == G_AC_THRESHOLD) && !(gDP.otherMode.alphaCvgSel))	{
		glUniform1i(at_enabled_location, 1);
		glUniform1f(at_value_location, gDP.blendColor.a);
	} else if (gDP.otherMode.cvgXAlpha)	{
		glUniform1i(at_enabled_location, 1);
		glUniform1f(at_value_location, 0.5f);
	} else {
		glUniform1i(at_enabled_location, 0);
		glUniform1f(at_value_location, 0.0f);
	}
}

void GLSL_RenderDepth() {
	if (!OGL.bImageTexture)
		return;
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
	if (frameBuffer.top == NULL || frameBuffer.top->pDepthBuffer == NULL)
		return;
	glBindImageTexture(depthImageUnit, 0, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
	glPushAttrib( GL_ENABLE_BIT | GL_VIEWPORT_BIT );

	glActiveTexture( GL_TEXTURE0 );
	glBindTexture(GL_TEXTURE_2D, frameBuffer.top->pDepthBuffer->depth_texture->glName);
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
#ifdef _WINDOWS
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
#ifdef _WINDOWS
			glDrawBuffer( GL_BACK );
#else
			OGL_SwapBuffers();
#endif
			ogl_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, frameBuffer.top->fbo);
			glBindImageTexture(depthImageUnit, frameBuffer.top->pDepthBuffer->depth_texture->glName, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);


			glLoadIdentity();
			glPopAttrib();

			gSP.changed |= CHANGED_TEXTURE | CHANGED_VIEWPORT;
			gDP.changed |= CHANGED_COMBINE;
#endif
}

void GLS_SetShadowMapCombiner() {

	if (!OGL.bImageTexture)
		return;

	if (g_paletteCRC256 != gDP.paletteCRC256) {
		g_paletteCRC256 = gDP.paletteCRC256;
		u16 palette[256];
		u16 *src = (u16*)&TMEM[256];
		for (int i = 0; i < 256; ++i)
			palette[i] = swapword(src[i*4]);
		glBindImageTexture(TlutImageUnit, 0, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R16UI);
		glBindTexture(GL_TEXTURE_1D, g_tlut_tex);
		glTexSubImage1D(GL_TEXTURE_1D, 0, 0, 256, GL_RED, GL_UNSIGNED_SHORT, palette);
		glBindTexture(GL_TEXTURE_1D, 0);
		glBindImageTexture(TlutImageUnit, g_tlut_tex, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R16UI);
	}

	glUseProgram(g_draw_shadow_map_program);

	gDP.changed |= CHANGED_COMBINE;
}
