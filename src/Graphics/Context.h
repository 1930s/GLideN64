#pragma once

#include <memory>
#include <Combiner.h>
#include "ObjectHandle.h"
#include "Parameter.h"
#include "CombinerProgram.h"
#include "ShaderProgram.h"
#include "PixelBuffer.h"
#include "FramebufferTextureFormats.h"

#define GRAPHICS_CONTEXT

struct CachedTexture;

namespace graphics {

	enum class SpecialFeatures {
		Multisampling,
		NearPlaneClipping,
		FragmentDepthWrite,
		BlitFramebuffer,
		WeakBlitFramebuffer,
		DepthFramebufferTextures,
		ShaderProgramBinary,
		ImageTextures
	};

	class ContextImpl;
	class ColorBufferReader;

	class Context
	{
	public:
		Context();
		~Context();

		void init();

		void destroy();

		void enable(Parameter _parameter, bool _enable);

		void cullFace(Parameter _mode);

		void enableDepthWrite(bool _enable);

		void setDepthCompare(Parameter _mode);

		void setViewport(s32 _x, s32 _y, s32 _width, s32 _height);

		void setScissor(s32 _x, s32 _y, s32 _width, s32 _height);

		void setBlending(Parameter _sfactor, Parameter _dfactor);

		void setBlendColor(f32 _red, f32 _green, f32 _blue, f32 _alpha);

		void clearColorBuffer(f32 _red, f32 _green, f32 _blue, f32 _alpha);

		void clearDepthBuffer();

		void setPolygonOffset(f32 _factor, f32 _units);

		/*---------------Texture-------------*/

		ObjectHandle createTexture(Parameter _target);

		void deleteTexture(ObjectHandle _name);

		struct InitTextureParams {
			ObjectHandle handle;
			Parameter ImageUnit;
			u32 msaaLevel = 0;
			u32 width = 0;
			u32 height = 0;
			u32 mipMapLevel = 0;
			u32 mipMapLevels = 1;
			Parameter format;
			Parameter internalFormat;
			Parameter dataType;
			const void * data = nullptr;
		};

		void init2DTexture(const InitTextureParams & _params);

		struct UpdateTextureDataParams {
			ObjectHandle handle;
			Parameter ImageUnit;
			Parameter textureUnitIndex = Parameter(0U);
			u32 x = 0;
			u32 y = 0;
			u32 width = 0;
			u32 height = 0;
			u32 mipMapLevel = 0;
			Parameter format;
			Parameter internalFormat;
			Parameter dataType;
			const void * data = nullptr;
		};

		void update2DTexture(const UpdateTextureDataParams & _params);

		struct TexParameters {
			ObjectHandle handle;
			Parameter textureUnitIndex = Parameter(0U);
			Parameter target;
			Parameter magFilter;
			Parameter minFilter;
			Parameter wrapS;
			Parameter wrapT;
			Parameter maxMipmapLevel;
			Parameter maxAnisotropy;
		};

		void setTextureParameters(const TexParameters & _parameters);

		struct BindTextureParameters {
			ObjectHandle texture;
			Parameter textureUnitIndex;
			Parameter target;
		};

		void bindTexture(const BindTextureParameters & _params);

		void setTextureUnpackAlignment(s32 _param);

		s32 getTextureUnpackAlignment() const;

		s32 getMaxTextureSize() const;

		struct BindImageTextureParameters {
			Parameter imageUnit;
			ObjectHandle texture;
			Parameter accessMode;
			Parameter textureFormat;
		};

		void bindImageTexture(const BindImageTextureParameters & _params);

		/*---------------Framebuffer-------------*/

		const FramebufferTextureFormats & getFramebufferTextureFormats();

		ObjectHandle createFramebuffer();

		void deleteFramebuffer(ObjectHandle _name);

		void bindFramebuffer(Parameter _target, ObjectHandle _name);

		ObjectHandle createRenderbuffer();

		struct InitRenderbufferParams {
			ObjectHandle handle;
			Parameter target;
			Parameter format;
			u32 width = 0;
			u32 height = 0;
		};

		void initRenderbuffer(const InitRenderbufferParams & _params);

		struct FrameBufferRenderTarget {
			ObjectHandle bufferHandle;
			Parameter bufferTarget;
			Parameter attachment;
			Parameter textureTarget;
			ObjectHandle textureHandle;
		};

		void addFrameBufferRenderTarget(const FrameBufferRenderTarget & _params);

		struct BlitFramebuffersParams
		{
			ObjectHandle readBuffer;
			ObjectHandle drawBuffer;
			s32 srcX0;
			s32 srcY0;
			s32 srcX1;
			s32 srcY1;
			s32 dstX0;
			s32 dstY0;
			s32 dstX1;
			s32 dstY1;
			Parameter mask;
			Parameter filter;
		};

		bool blitFramebuffers(const BlitFramebuffersParams & _params);

		/*---------------Pixelbuffer-------------*/

		PixelWriteBuffer * createPixelWriteBuffer(size_t _sizeInBytes);

		PixelReadBuffer * createPixelReadBuffer(size_t _sizeInBytes);

		ColorBufferReader * createColorBufferReader(CachedTexture * _pTexture);

		/*---------------Shaders-------------*/

		CombinerProgram * createCombinerProgram(Combiner & _color, Combiner & _alpha, const CombinerKey & _key);

		bool saveShadersStorage(const Combiners & _combiners);

		bool loadShadersStorage(Combiners & _combiners);

		ShaderProgram * createDepthFogShader();

		ShaderProgram * createMonochromeShader();

		TexDrawerShaderProgram * createTexDrawerDrawShader();

		ShaderProgram * createTexDrawerClearShader();

		ShaderProgram * createTexrectCopyShader();

		ShaderProgram * createGammaCorrectionShader();

		ShaderProgram * createOrientationCorrectionShader();

		void resetShaderProgram();

		/*---------------Draw-------------*/

		struct DrawTriangleParameters
		{
			Parameter mode;
			Parameter elementsType;
			u32 verticesCount = 0;
			u32 elementsCount = 0;
			bool flatColors = false;
			SPVertex * vertices = nullptr;
			void * elements = nullptr;
			const CombinerProgram * combiner = nullptr;
		};

		void drawTriangles(const DrawTriangleParameters & _params);

		struct DrawRectParameters
		{
			Parameter mode;
			u32 verticesCount = 0;
			std::array<f32, 4> rectColor;
			RectVertex * vertices = nullptr;
			const CombinerProgram * combiner = nullptr;
		};

		void drawRects(const DrawRectParameters & _params);

		void drawLine(f32 _width, SPVertex * _vertices);

		f32 getMaxLineWidth();

		void drawText(const char *_pText, float _x, float _y);

		void getTextSize(const char *_pText, float & _w, float & _h);

		/*---------------Misc-------------*/

		bool isSupported(SpecialFeatures _feature) const;

		bool isError() const;

		bool isFramebufferError() const;

		static bool imageTextures;
		static bool multisampling;

	private:
		std::unique_ptr<ContextImpl> m_impl;
		std::unique_ptr<FramebufferTextureFormats> m_fbTexFormats;
	};

}

extern graphics::Context gfxContext;
