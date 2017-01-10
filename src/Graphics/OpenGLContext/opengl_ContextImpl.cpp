#include <assert.h>
#include <Log.h>
#include "opengl_ContextImpl.h"
#include "opengl_UnbufferedDrawer.h"
#include "GLSL/glsl_CombinerProgramBuilder.h"
#include "GLSL/glsl_SpecialShadersFactory.h"
#include "GLSL/glsl_ShaderStorage.h"

using namespace opengl;

ContextImpl::ContextImpl()
{
	initGLFunctions();
}


ContextImpl::~ContextImpl()
{
}

void ContextImpl::init()
{
	m_glInfo.init();

	if (!m_cachedFunctions)
		m_cachedFunctions.reset(new CachedFunctions(m_glInfo));

	{
		TextureManipulationObjectFactory textureObjectsFactory(m_glInfo, *m_cachedFunctions.get());
		m_createTexture.reset(textureObjectsFactory.getCreate2DTexture());
		m_init2DTexture.reset(textureObjectsFactory.getInit2DTexture());
		m_update2DTexture.reset(textureObjectsFactory.getUpdate2DTexture());
		m_set2DTextureParameters.reset(textureObjectsFactory.getSet2DTextureParameters());
	}

	{
		BufferManipulationObjectFactory bufferObjectFactory(m_glInfo, *m_cachedFunctions.get());
		m_createFramebuffer.reset(bufferObjectFactory.getCreateFramebufferObject());
		m_createRenderbuffer.reset(bufferObjectFactory.getCreateRenderbuffer());
		m_initRenderbuffer.reset(bufferObjectFactory.getInitRenderbuffer());
		m_addFramebufferRenderTarget.reset(bufferObjectFactory.getAddFramebufferRenderTarget());
		m_createPixelWriteBuffer.reset(bufferObjectFactory.createPixelWriteBuffer());
	}

	m_combinerProgramBuilder.reset(new glsl::CombinerProgramBuilder(m_glInfo));
}

void ContextImpl::destroy()
{
	m_cachedFunctions.reset(nullptr);
	m_createTexture.reset(nullptr);
	m_init2DTexture.reset(nullptr);
	m_set2DTextureParameters.reset(nullptr);

	m_createFramebuffer.reset(nullptr);
	m_createRenderbuffer.reset(nullptr);
	m_initRenderbuffer.reset(nullptr);
	m_addFramebufferRenderTarget.reset(nullptr);


	m_combinerProgramBuilder.reset(nullptr);
}

void ContextImpl::enable(graphics::Parameter _parameter, bool _enable)
{
	m_cachedFunctions->getCachedEnable(_parameter)->enable(_enable);
}

void ContextImpl::cullFace(graphics::Parameter _mode)
{
	m_cachedFunctions->getCachedCullFace()->setCullFace(_mode);
}

void ContextImpl::enableDepthWrite(bool _enable)
{
	m_cachedFunctions->getCachedDepthMask()->setDepthMask(_enable);
}

void ContextImpl::setDepthCompare(graphics::Parameter _mode)
{
	m_cachedFunctions->getCachedDepthCompare()->setDepthCompare(_mode);
}

void ContextImpl::setViewport(s32 _x, s32 _y, s32 _width, s32 _height)
{
	m_cachedFunctions->getCachedViewport()->setViewport(_x, _y, _width, _height);
}

void ContextImpl::setScissor(s32 _x, s32 _y, s32 _width, s32 _height)
{
	m_cachedFunctions->getCachedScissor()->setScissor(_x, _y, _width, _height);
}

void ContextImpl::setBlending(graphics::Parameter _sfactor, graphics::Parameter _dfactor)
{
	m_cachedFunctions->getCachedBlending()->setBlending(_sfactor, _dfactor);
}

void ContextImpl::setBlendColor(f32 _red, f32 _green, f32 _blue, f32 _alpha)
{

}

graphics::ObjectHandle ContextImpl::createTexture(graphics::Parameter _target)
{
	return m_createTexture->createTexture(_target);
}

void ContextImpl::deleteTexture(graphics::ObjectHandle _name)
{
	u32 glName(_name);
	glDeleteTextures(1, &glName);
	m_init2DTexture->reset(_name);
}

void ContextImpl::init2DTexture(const graphics::Context::InitTextureParams & _params)
{
	m_init2DTexture->init2DTexture(_params);
}

void ContextImpl::update2DTexture(const graphics::Context::UpdateTextureDataParams & _params)
{
	m_update2DTexture->update2DTexture(_params);
}

void ContextImpl::setTextureParameters(const graphics::Context::TexParameters & _parameters)
{
	m_set2DTextureParameters->setTextureParameters(_parameters);
}

graphics::ObjectHandle ContextImpl::createFramebuffer()
{
	return m_createFramebuffer->createFramebuffer();
}

void ContextImpl::deleteFramebuffer(graphics::ObjectHandle _name)
{
	u32 fbo(_name);
	if (fbo != 0)
		glDeleteFramebuffers(1, &fbo);
}

graphics::ObjectHandle ContextImpl::createRenderbuffer()
{
	return m_createRenderbuffer->createRenderbuffer();
}

void ContextImpl::initRenderbuffer(const graphics::Context::InitRenderbufferParams & _params)
{
	m_initRenderbuffer->initRenderbuffer(_params);
}

void ContextImpl::addFrameBufferRenderTarget(const graphics::Context::FrameBufferRenderTarget & _params)
{
	m_addFramebufferRenderTarget->addFrameBufferRenderTarget(_params);
}

graphics::PixelWriteBuffer * ContextImpl::createPixelWriteBuffer(size_t _sizeInBytes)
{
	return m_createPixelWriteBuffer->createPixelWriteBuffer(_sizeInBytes);
}

graphics::CombinerProgram * ContextImpl::createCombinerProgram(Combiner & _color, Combiner & _alpha, const CombinerKey & _key)
{
	return m_combinerProgramBuilder->buildCombinerProgram(_color, _alpha, _key);
}

bool ContextImpl::saveShadersStorage(const graphics::Combiners & _combiners)
{
	glsl::ShaderStorage storage(m_glInfo);
	return storage.saveShadersStorage(_combiners);
}

bool ContextImpl::loadShadersStorage(graphics::Combiners & _combiners)
{
	glsl::ShaderStorage storage(m_glInfo);
	return storage.loadShadersStorage(_combiners);
}

graphics::ShaderProgram * ContextImpl::createDepthFogShader()
{
	glsl::SpecialShadersFactory shadersFactory(m_glInfo,
		m_combinerProgramBuilder->getVertexShaderHeader(),
		m_combinerProgramBuilder->getFragmentShaderHeader());

	return shadersFactory.createShadowMapShader();
}

graphics::ShaderProgram * ContextImpl::createMonochromeShader()
{
	glsl::SpecialShadersFactory shadersFactory(m_glInfo,
		m_combinerProgramBuilder->getVertexShaderHeader(),
		m_combinerProgramBuilder->getFragmentShaderHeader());

	return shadersFactory.createMonochromeShader();
}

graphics::TexDrawerShaderProgram * ContextImpl::createTexDrawerDrawShader()
{
	glsl::SpecialShadersFactory shadersFactory(m_glInfo,
		m_combinerProgramBuilder->getVertexShaderHeader(),
		m_combinerProgramBuilder->getFragmentShaderHeader());

	return shadersFactory.createTexDrawerDrawShader();
}

graphics::ShaderProgram * ContextImpl::createTexDrawerClearShader()
{
	glsl::SpecialShadersFactory shadersFactory(m_glInfo,
		m_combinerProgramBuilder->getVertexShaderHeader(),
		m_combinerProgramBuilder->getFragmentShaderHeader());

	return shadersFactory.createTexDrawerClearShader();
}

graphics::ShaderProgram * ContextImpl::createTexrectCopyShader()
{
	glsl::SpecialShadersFactory shadersFactory(m_glInfo,
		m_combinerProgramBuilder->getVertexShaderHeader(),
		m_combinerProgramBuilder->getFragmentShaderHeader());

	return shadersFactory.createTexrectCopyShader();
}

graphics::DrawerImpl * ContextImpl::createDrawerImpl()
{
	return new UnbufferedDrawer(m_cachedFunctions->getCachedVertexAttribArray());
}
