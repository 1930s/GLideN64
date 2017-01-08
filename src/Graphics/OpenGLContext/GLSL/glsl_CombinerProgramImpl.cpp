#include <fstream>
#include <assert.h>
#include <Combiner.h>
#include "glsl_Utils.h"
#include "glsl_CombinerProgramImpl.h"

using namespace glsl;

CombinerProgramImpl::CombinerProgramImpl(const CombinerKey & _key,
	GLuint _program,
	const CombinerInputs & _inputs,
	UniformGroups && _uniforms)
: m_bNeedUpdate(true)
, m_key(_key)
, m_program(_program)
, m_inputs(_inputs)
, m_uniforms(std::move(_uniforms))
{
}


CombinerProgramImpl::~CombinerProgramImpl()
{
}

void CombinerProgramImpl::activate()
{
	glUseProgram(m_program);
}

void CombinerProgramImpl::update(bool _force)
{
	_force |= m_bNeedUpdate;
	m_bNeedUpdate = false;
	glUseProgram(m_program);
	for (auto it = m_uniforms.begin(); it != m_uniforms.end(); ++it)
		(*it)->update(_force);
}

CombinerKey CombinerProgramImpl::getKey() const
{
	return m_key;
}

bool CombinerProgramImpl::usesTexture() const
{
	return m_inputs.usesTexture();
}

bool CombinerProgramImpl::usesTile(u32 _t) const {
	return m_inputs.usesTile(_t);
}

bool CombinerProgramImpl::usesShade() const {
	return m_inputs.usesShade();
}

bool CombinerProgramImpl::usesLOD() const {
	return m_inputs.usesLOD();
}

bool CombinerProgramImpl::getBinaryForm(std::vector<char> & _buffer)
{
	GLint  binaryLength;
	glGetProgramiv(m_program, GL_PROGRAM_BINARY_LENGTH, &binaryLength);

	if (binaryLength < 1)
		return false;

	std::vector<char> binary(binaryLength);

	if (binary.size() == 0)
		return false;

	GLenum binaryFormat;
	glGetProgramBinary(m_program, binaryLength, &binaryLength, &binaryFormat, binary.data());
	if (isGLError())
		return false;

	u64 key = m_key.getMux();
	int inputs(m_inputs);

	int totalSize = sizeof(key)+sizeof(inputs)+sizeof(binaryFormat)+
		sizeof(binaryLength)+binaryLength;
	_buffer.resize(totalSize);

	char* keyData = reinterpret_cast<char*>(&key);
	std::copy_n(keyData, sizeof(key), _buffer.data());
	int offset = sizeof(key);

	char* inputData = reinterpret_cast<char*>(&inputs);
	std::copy_n(inputData, sizeof(inputs), _buffer.data() + offset);
	offset += sizeof(inputs);

	char* binaryFormatData = reinterpret_cast<char*>(&binaryFormat);
	std::copy_n(binaryFormatData, sizeof(binaryFormat), _buffer.data() + offset);
	offset += sizeof(binaryFormat);

	char* binaryLengthData = reinterpret_cast<char*>(&binaryLength);
	std::copy_n(binaryLengthData, sizeof(binaryLength), _buffer.data() + offset);
	offset += sizeof(binaryLength);

	std::copy_n(binary.data(), binaryLength, _buffer.data() + offset);

	return true;
}
