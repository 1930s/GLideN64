#pragma once
#include <Graphics/OpenGLContext/opengl_GLInfo.h>

namespace graphics {
	class ShaderProgram;
}

namespace glsl {

	class ShaderPart;

	class SpecialShadersFactory
	{
	public:
		SpecialShadersFactory(const opengl::GLInfo & _glinfo,
			const ShaderPart * _vertexHeader,
			const ShaderPart * _fragmentHeader);

		graphics::ShaderProgram * createShadowMapShader() const;

		graphics::ShaderProgram * createMonochromeShader() const;

		graphics::TexDrawerShaderProgram * createTexDrawerDrawShader() const;

		graphics::ShaderProgram * createTexDrawerClearShader() const;

		graphics::ShaderProgram * createTexrectCopyShader() const;

	private:
		const opengl::GLInfo & m_glinfo;
		const ShaderPart * m_vertexHeader;
		const ShaderPart * m_fragmentHeader;
	};

}
