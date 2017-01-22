#pragma once
#include "Parameter.h"

namespace graphics {

	namespace color {
		extern Parameter RGBA;
		extern Parameter RG;
		extern Parameter RED;
		extern Parameter DEPTH;
	}

	namespace internalcolor {
		extern Parameter RGB8;
		extern Parameter RGBA;
		extern Parameter RGBA4;
		extern Parameter RGB5_A1;
		extern Parameter RG;
		extern Parameter RED;
		extern Parameter DEPTH;
		extern Parameter RG32F;
	}

	namespace datatype {
		extern Parameter UNSIGNED_BYTE;
		extern Parameter UNSIGNED_SHORT;
		extern Parameter UNSIGNED_INT;
		extern Parameter FLOAT;
		extern Parameter UNSIGNED_SHORT_5_5_5_1;
		extern Parameter UNSIGNED_SHORT_4_4_4_4;
	}

	namespace target {
		extern Parameter TEXTURE_2D;
		extern Parameter TEXTURE_2D_MULTISAMPLE;
		extern Parameter RENDERBUFFER;
	}

	namespace bufferTarget {
		extern Parameter FRAMEBUFFER;
		extern Parameter DRAW_FRAMEBUFFER;
		extern Parameter READ_FRAMEBUFFER;
	}

	namespace bufferAttachment {
		extern Parameter COLOR_ATTACHMENT0;
		extern Parameter DEPTH_ATTACHMENT;
	}

	namespace enable {
		extern Parameter BLEND;
		extern Parameter CULL_FACE;
		extern Parameter DEPTH_TEST;
		extern Parameter DEPTH_CLAMP;
		extern Parameter CLIP_DISTANCE0;
		extern Parameter DITHER;
		extern Parameter POLYGON_OFFSET_FILL;
		extern Parameter SCISSOR_TEST;
	}

	namespace textureIndices {
		extern Parameter Tex[2];
		extern Parameter NoiseTex;
		extern Parameter DepthTex;
		extern Parameter ZLUTTex;
		extern Parameter PaletteTex;
		extern Parameter MSTex[2];
	}

	namespace textureImageUnits {
		extern Parameter Zlut;
		extern Parameter Tlut;
		extern Parameter Depth;
	}

	namespace textureImageAccessMode {
		extern Parameter READ_ONLY;
		extern Parameter WRITE_ONLY;
		extern Parameter READ_WRITE;
	}

	namespace textureParameters {
		extern Parameter FILTER_NEAREST;
		extern Parameter FILTER_LINEAR;
		extern Parameter FILTER_NEAREST_MIPMAP_NEAREST;
		extern Parameter FILTER_LINEAR_MIPMAP_NEAREST;
		extern Parameter WRAP_CLAMP_TO_EDGE;
		extern Parameter WRAP_REPEAT;
		extern Parameter WRAP_MIRRORED_REPEAT;
	}

	namespace cullMode {
		extern Parameter FRONT;
		extern Parameter BACK;
	}

	namespace compare {
		extern Parameter LEQUAL;
		extern Parameter LESS;
		extern Parameter ALWAYS;
	}

	namespace blend {
		extern Parameter ZERO;
		extern Parameter ONE;
		extern Parameter SRC_ALPHA;
		extern Parameter DST_ALPHA;
		extern Parameter ONE_MINUS_SRC_ALPHA;
		extern Parameter CONSTANT_ALPHA;
		extern Parameter ONE_MINUS_CONSTANT_ALPHA;
	}

	namespace drawmode {
		extern Parameter TRIANGLES;
		extern Parameter TRIANGLE_STRIP;
		extern Parameter LINES;
	}

	namespace blitMask {
		extern Parameter COLOR_BUFFER;
		extern Parameter DEPTH_BUFFER;
		extern Parameter STENCIL_BUFFER;
	}
}
