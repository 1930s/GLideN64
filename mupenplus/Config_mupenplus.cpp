#include "GLideN64_MupenPlus.h"
#include <assert.h>
#include <errno.h>
#include <string.h>

#include "../Config.h"
#include "../GLideN64.h"
#include "../RSP.h"
#include "../Textures.h"
#include "../OpenGL.h"
#include "../Log.h"

Config config;

const u32 uMegabyte = 1024U*1024U;

static m64p_handle g_configVideoGeneral = NULL;
static m64p_handle g_configVideoGliden64 = NULL;

static
bool Config_SetDefault()
{
	if (ConfigOpenSection("Video-General", &g_configVideoGeneral) != M64ERR_SUCCESS) {
		LOG(LOG_ERROR, "Unable to open Video-General configuration section");
		return false;
	}
	if (ConfigOpenSection("Video-GLideN64", &g_configVideoGliden64) != M64ERR_SUCCESS) {
		LOG(LOG_ERROR, "Unable to open GLideN64 configuration section");
		return false;
	}

	// Set default values for "Video-General" section, if they are not set yet. Taken from RiceVideo
	m64p_error res = ConfigSetDefaultBool(g_configVideoGeneral, "Fullscreen", 0, "Use fullscreen mode if True, or windowed mode if False ");
	assert(res == M64ERR_SUCCESS);
	res = ConfigSetDefaultInt(g_configVideoGeneral, "ScreenWidth", 640, "Width of output window or fullscreen width");
	assert(res == M64ERR_SUCCESS);
	res = ConfigSetDefaultInt(g_configVideoGeneral, "ScreenHeight", 480, "Height of output window or fullscreen height");
	assert(res == M64ERR_SUCCESS);
	res = ConfigSetDefaultBool(g_configVideoGeneral, "VerticalSync", 0, "If true, activate the SDL_GL_SWAP_CONTROL attribute");
	assert(res == M64ERR_SUCCESS);

	res = ConfigSetDefaultInt(g_configVideoGliden64, "MultiSampling", 0, "Enable/Disable MultiSampling (0=off, 2,4,8,16=quality)");
	assert(res == M64ERR_SUCCESS);
	res = ConfigSetDefaultInt(g_configVideoGliden64, "AspectRatio", 0, "Screen aspect ratio (0=stretch, 1=force 4:3, 2=force 16:9)");
	assert(res == M64ERR_SUCCESS);

	//#Texture Settings
	res = ConfigSetDefaultBool(g_configVideoGliden64, "ForceBilinear", 0, "Force bilinear texture filter");
	assert(res == M64ERR_SUCCESS);
	res = ConfigSetDefaultInt(g_configVideoGliden64, "CacheSize", 192, "Size of texture cache in megabytes. Good value is VRAM*3/4");
	assert(res == M64ERR_SUCCESS);
	res = ConfigSetDefaultInt(g_configVideoGliden64, "TextureBitDepth", 1, "Texture bit depth (0=16bit only, 1=16 and 32 bit, 2=32bit only)");
	assert(res == M64ERR_SUCCESS);
	//#Emulation Settings
	res = ConfigSetDefaultBool(g_configVideoGliden64, "EnableFog", 1, "Enable fog emulation.");
	assert(res == M64ERR_SUCCESS);
	res = ConfigSetDefaultBool(g_configVideoGliden64, "EnableNoise", 1, "Enable color noise emulation.");
	assert(res == M64ERR_SUCCESS);
	res = ConfigSetDefaultBool(g_configVideoGliden64, "EnableLOD", 1, "Enable LOD emulation.");
	assert(res == M64ERR_SUCCESS);
	res = ConfigSetDefaultBool(g_configVideoGliden64, "EnableHWLighting", 1, "Enable hardware per-pixel lighting.");
	assert(res == M64ERR_SUCCESS);
	//#Frame Buffer Settings:"
	res = ConfigSetDefaultBool(g_configVideoGliden64, "EnableFBEmulation", 1, "Enable frame and|or depth buffer emulation.");
	assert(res == M64ERR_SUCCESS);
	res = ConfigSetDefaultBool(g_configVideoGliden64, "EnableCopyColorToRDRAM", 0, "Enable color buffer copy to RDRAM.");
	assert(res == M64ERR_SUCCESS);
	res = ConfigSetDefaultBool(g_configVideoGliden64, "EnableCopyDepthToRDRAM", 0, "Enable depth buffer copy to RDRAM.");
	assert(res == M64ERR_SUCCESS);
	res = ConfigSetDefaultBool(g_configVideoGliden64, "EnableCopyColorFromRDRAM", 0, "Enable color buffer copy from RDRAM.");
	assert(res == M64ERR_SUCCESS);
	res = ConfigSetDefaultBool(g_configVideoGliden64, "EnableIgnoreCFB", 1, "Ignore CPU writes to frame buffer.");
	assert(res == M64ERR_SUCCESS);
	res = ConfigSetDefaultBool(g_configVideoGliden64, "EnableN64DepthCompare", 0, "Enable N64 depth compare instead of OpenGL standard one. Experimental.");
	assert(res == M64ERR_SUCCESS);

	return res == M64ERR_SUCCESS;
}

void Config_LoadConfig()
{
	Config_SetDefault();

	config.video.fullscreen = ConfigGetParamBool(g_configVideoGeneral, "Fullscreen");
	config.video.windowedWidth = ConfigGetParamInt(g_configVideoGeneral, "ScreenWidth");
	config.video.windowedHeight = ConfigGetParamInt(g_configVideoGeneral, "ScreenHeight");
	config.video.verticalSync = ConfigGetParamBool(g_configVideoGeneral, "VerticalSync");

	config.video.multisampling = ConfigGetParamInt(g_configVideoGliden64, "MultiSampling");
	config.video.aspect = ConfigGetParamInt(g_configVideoGliden64, "AspectRatio");

	//#Texture Settings
	config.texture.forceBilinear = ConfigGetParamBool(g_configVideoGliden64, "ForceBilinear");
	config.texture.maxBytes = ConfigGetParamInt(g_configVideoGliden64, "CacheSize") * uMegabyte;
	config.texture.textureBitDepth = ConfigGetParamInt(g_configVideoGliden64, "TextureBitDepth");
	//#Emulation Settings
	config.enableFog = ConfigGetParamBool(g_configVideoGliden64, "EnableFog");
	config.enableNoise = ConfigGetParamBool(g_configVideoGliden64, "EnableNoise");
	config.enableLOD = ConfigGetParamBool(g_configVideoGliden64, "EnableLOD");
	config.enableHWLighting = ConfigGetParamBool(g_configVideoGliden64, "EnableHWLighting");
	//#Frame Buffer Settings:"
	config.frameBufferEmulation.enable = ConfigGetParamBool(g_configVideoGliden64, "EnableFBEmulation");
	config.frameBufferEmulation.copyToRDRAM = ConfigGetParamBool(g_configVideoGliden64, "EnableCopyColorToRDRAM");
	config.frameBufferEmulation.copyDepthToRDRAM = ConfigGetParamBool(g_configVideoGliden64, "EnableCopyDepthToRDRAM");
	config.frameBufferEmulation.copyFromRDRAM = ConfigGetParamBool(g_configVideoGliden64, "EnableCopyColorFromRDRAM");
	config.frameBufferEmulation.ignoreCFB = ConfigGetParamBool(g_configVideoGliden64, "EnableIgnoreCFB");
	config.frameBufferEmulation.N64DepthCompare = ConfigGetParamBool(g_configVideoGliden64, "EnableN64DepthCompare");
	config.hacks = 0;
}

#if 0
struct Option
{
	const char* name;
	u32* data;
	const int initial;
};

Option configOptions[] =
{
	{"#GLideN64 Graphics Plugin for N64", NULL, 0},
	{"config version", &config.version, 0},
	{"", NULL, 0},

	{"#Texture Settings:", NULL, 0},
	{"force bilinear", &config.texture.forceBilinear, 0},
	{"cache size", &config.texture.maxBytes, 64 * uMegabyte},
	{"texture bit depth", &config.texture.textureBitDepth, 1},
	{"#Emulation Settings:", NULL, 0},
	{"enable fog", &config.enableFog, 1},
	{"enable noise", &config.enableNoise, 1},
	{"enable LOD", &config.enableLOD, 1},
	{"enable HW lighting", &config.enableHWLighting, 0},
	{"#Frame Buffer Settings:", NULL, 0},
	{"enable hardware FB", &config.frameBufferEmulation.enable, 0},
	{"enable copy Color Buffer to RDRAM", &config.frameBufferEmulation.copyToRDRAM, 0},
	{"enable copy Depth Buffer to RDRAM", &config.frameBufferEmulation.copyDepthToRDRAM, 0},
	{"enable copy Color Buffer from RDRAM", &config.frameBufferEmulation.copyFromRDRAM, 0},
	{"enable ignore CFB", &config.frameBufferEmulation.ignoreCFB, 0},
	{"enable N64 depth compare", &config.frameBufferEmulation.N64DepthCompare, 0}
};

const int configOptionsSize = sizeof(configOptions) / sizeof(Option);

void Config_WriteConfig(const char *filename)
{
	//config.version = CONFIG_VERSION;
	FILE* f = fopen(filename, "w");
	if (!f) {
		fprintf(stderr, "[GLideN64]: Could Not Open %s for writing\n", filename);
		return;
	}

	for (int i = 0; i<configOptionsSize; i++) {
		Option *o = &configOptions[i];
		fprintf(f, "%s", o->name);
		if (o->data)
			fprintf(f, "=%i", *(o->data));
		fprintf(f, "\n");
	}

	fclose(f);
}

void Config_SetDefault()
{
	for (int i = 0; i < configOptionsSize; i++) {
		Option *o = &configOptions[i];
		if (o->data) *(o->data) = o->initial;
	}
	config.video.fullscreenWidth = 640;
	config.video.fullscreenHeight = 480;
}

void Config_SetOption(char* line, char* val)
{
	for (int i = 0; i< configOptionsSize; i++) {
		Option *o = &configOptions[i];
#ifndef OS_WINDOWS
		if (strcasecmp(line, o->name) == 0) {
#else
		if (_stricmp(line, o->name) == 0) {
#endif
			if (o->data) {
				int v = atoi(val);
				*(o->data) = v;
			}
			break;
		}
	}
}

void Config_LoadConfig()
{
	static bool loaded = false;
	FILE *f;
	char line[4096];

	if (loaded)
		return;

	loaded = true;

	Config_SetDefault();

	// read configuration
#ifndef GLES2
	const char * pConfigName = "GLideN64.cfg";
	const char * pConfigPath = ConfigGetUserConfigPath();
	const size_t nPathLen = strlen(pConfigPath);
	const size_t configNameLen = nPathLen + strlen(pConfigName) + 2;
	char * pConfigFullName = new char[configNameLen];
	strcpy(pConfigFullName, pConfigPath);
	if (pConfigPath[nPathLen - 1] != '/')
		strcat(pConfigFullName, "/");
	strcat(pConfigFullName, pConfigName);
	f = fopen(pConfigFullName, "r");
	if (!f) {
		fprintf(stderr, "[GLideN64]: (WW) Couldn't open config file '%s' for reading: %s\n", pConfigFullName, strerror(errno));
		fprintf(stderr, "[GLideN64]: Attempting to write new Config \n");
		Config_WriteConfig(pConfigFullName);
		delete[] pConfigFullName;
		return;
	}
	delete[] pConfigFullName;
#else
	const char *filename = ConfigGetSharedDataFilepath("gles2gliden64.conf");
	f = fopen(filename, "r");
	if (!f) {
		LOG(LOG_MINIMAL, "[gles2GlideN64]: Couldn't open config file '%s' for reading: %s\n", filename, strerror(errno));
		LOG(LOG_MINIMAL, "[gles2GlideN64]: Attempting to write new Config \n");
		Config_WriteConfig(filename);
		return;
	}
	LOG(LOG_MINIMAL, "[gles2GlideN64]: Loading Config from %s \n", filename);
#endif
	while (!feof(f)) {
		char *val;
		fgets(line, 4096, f);

		if (line[0] == '#' || line[0] == '\n')
			continue;

		val = strchr(line, '=');
		if (!val) continue;

		*val++ = '\0';

		Config_SetOption(line, val);
	}
	/*
	if (config.version < CONFIG_VERSION)
	{
	LOG(LOG_WARNING, "[gles2N64]: Wrong config version, rewriting config with defaults\n");
	Config_SetDefault();
	Config_WriteConfig(filename);
	}
	*/
	fclose(f);
}

#endif

