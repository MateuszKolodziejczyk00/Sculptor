#pragma once

#include "RendererTypesMacros.h"
#include "SculptorCoreTypes.h"
#include "RHIBridge/RHIDescriptorSetImpl.h"

namespace spt::smd
{
class ShaderMetaData;
} // spt::smd


namespace spt::rdr
{

class DescriptorSetWriter;
class BufferView;
class TextureView;


using DSStateID = Uint64;


class RENDERER_TYPES_API DescriptorSetUpdateContext
{
public:

	DescriptorSetUpdateContext(rhi::RHIDescriptorSet descriptorSet, DescriptorSetWriter& writer, const lib::SharedRef<smd::ShaderMetaData>& metaData);

	void UpdateBuffer(const lib::HashedString& name, const lib::SharedRef<BufferView>& buffer) const;
	void UpdateTexture(const lib::HashedString& name, const lib::SharedRef<TextureView>& texture) const;

	const smd::ShaderMetaData& GetMetaData() const;

private:

	rhi::RHIDescriptorSet				m_descriptorSet;
	DescriptorSetWriter&				m_writer;
	lib::SharedRef<smd::ShaderMetaData> m_metaData;
};


/**
 * Base class for all descriptor set states
 */
class RENDERER_TYPES_API DescriptorSetState abstract
{
public:

	DescriptorSetState();

	virtual void UpdateDescriptors(DescriptorSetUpdateContext& context) const = 0;

	DSStateID	GetID() const;

	Bool		IsDirty() const;
	void		ClearDirtyFlag();

protected:

	void MarkAsDirty();

private:

	const DSStateID	m_state;
	Bool			m_isDirty;
};

} // spt::rdr
