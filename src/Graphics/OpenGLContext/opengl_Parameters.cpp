#include <Graphics/Parameters.h>
#include "GLFunctions.h"

namespace graphics {

	namespace color {
		Parameter RGBA(GL_RGBA);
		Parameter RG(GL_RG);
		Parameter RED(GL_RED);
		Parameter DEPTH(GL_DEPTH_COMPONENT);
	}

	namespace internalcolor {
		Parameter RGBA(GL_RGBA8);
		Parameter RG(GL_RG8UI);
		Parameter RED(GL_R8UI);
		Parameter DEPTH(GL_DEPTH_COMPONENT);
	}

	namespace type {
		Parameter UNSIGNED_BYTE(GL_UNSIGNED_BYTE);
		Parameter UNSIGNED_SHORT(GL_UNSIGNED_SHORT);
		Parameter UNSIGNED_INT(GL_UNSIGNED_INT);
		Parameter FLOAT(GL_FLOAT);
	}

	namespace target {
		Parameter TEXTURE_2D(GL_TEXTURE_2D);
		Parameter TEXTURE_2D_MULTISAMPLE(GL_TEXTURE_2D_MULTISAMPLE);
	}

	namespace enable {
		Parameter BLEND(GL_BLEND);
		Parameter CULL_FACE(GL_CULL_FACE);
		Parameter DEPTH_TEST(GL_DEPTH_TEST);
		Parameter DEPTH_CLAMP(GL_DEPTH_CLAMP);
		Parameter CLIP_DISTANCE0(GL_CLIP_DISTANCE0);
		Parameter DITHER(GL_DITHER);
		Parameter POLYGON_OFFSET_FILL(GL_POLYGON_OFFSET_FILL);
		Parameter SCISSOR_TEST(GL_SCISSOR_TEST);
	}

}
