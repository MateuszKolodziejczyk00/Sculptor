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


class RENDERER_TYPES_API DescriptorSetBinding abstract
{
public:

	explicit DescriptorSetBinding(const lib::HashedString& name);
	virtual ~DescriptorSetBinding() = default;

	const lib::HashedString& GetName() const;

	virtual void UpdateDescriptors(DescriptorSetUpdateContext& context) const = 0;

private:

	lib::HashedString m_name;
};


namespace bindings_refl
{

/**
 * Dummy type for head of the bindings list. Shouldn't be directly used
 */
struct HeadBindingUnderlyingType
{
};

class BindingHandleNode
{
public:

	BindingHandleNode(BindingHandleNode* next, DescriptorSetBinding* binding)
		: m_next(next)
		, m_binding(binding)
	{ }

	BindingHandleNode* GetNext() const
	{
		return m_next;
	}

	DescriptorSetBinding& Get()
	{
		SPT_CHECK(!!m_binding);
		return *m_binding;
	}

private:

	BindingHandleNode*		m_next;
	DescriptorSetBinding*	m_binding;
};


template<typename TBindingType, lib::Literal name>
class BindingDataHandleNode : public BindingHandleNode
{
protected:

	using Super = BindingHandleNode;

public:

	using BindingType			= TBindingType;

	template<typename... TArgs>
	explicit BindingDataHandleNode(BindingHandleNode* next, TArgs&&... args)
		: Super(next, &m_binding)
		, m_binding(name.Get(), std::forward<TArgs>(args)...)
	{ }

	BindingType& Get()
	{
		return m_binding;
	}

	BindingType* operator->()
	{
		return &m_binding;
	}

private:

	TBindingType m_binding;
};


template<typename TNextBindingHandleType,
		 typename TBindingType,
		 lib::Literal name>
requires std::is_base_of_v<DescriptorSetBinding, TBindingType> || std::is_same_v<TBindingType, HeadBindingUnderlyingType>
class BindingHandle : public BindingDataHandleNode<TBindingType, name>
{
protected:

	using Super = BindingDataHandleNode<TBindingType, name>;

public:

	using BindingType			= TBindingType;
	using NextBindingHandleType = TNextBindingHandleType;

	template<typename... TArgs>
	explicit BindingHandle(NextBindingHandleType* next, TArgs&&... args)
		: Super(next, std::forward<TArgs>(args)...)
	{
		SPT_CHECK(!!next);
	}

	NextBindingHandleType* GetNext() const
	{
		return static_cast<NextBindingHandleType*>(Super::GetNext());
	}
};


/**
 * Partial specialization for last element in the list (NextBindingHandleType is void)
 */
template<typename TBindingType,
		lib::Literal name>
requires std::is_base_of_v<DescriptorSetBinding, TBindingType>
class BindingHandle<void, TBindingType, name> : public BindingDataHandleNode<TBindingType, name>
{
protected:

	using Super = BindingDataHandleNode<TBindingType, name>;

public:

	using BindingType			= TBindingType;
	using NextBindingHandleType = void;
	using NextBindingType		= void;

	template<typename... TArgs>
	explicit BindingHandle(NextBindingHandleType*, TArgs&&... args)
		: Super(nullptr, std::forward<TArgs>(args)...)
	{ }
};


template<typename TNextBindingHandleType,
		 lib::Literal name>
class BindingHandle<TNextBindingHandleType, HeadBindingUnderlyingType, name> : public BindingHandleNode
{
protected:

	using Super = BindingHandleNode;

public:

	using BindingType			= HeadBindingUnderlyingType;
	using NextBindingHandleType = TNextBindingHandleType;

	explicit BindingHandle(NextBindingHandleType* next)
		: Super(next, nullptr)
	{ }

	NextBindingHandleType* GetNext() const
	{
		return static_cast<NextBindingHandleType*>(Super::GetNext());
	}
};


#define DS_BINDINGS_BEGIN()	template<typename TBindingType>																	\
							TBindingType* GetBindingImpl()																	\
							{																								\
								return nullptr;																				\
							}																								\
							typedef rdr::bindings_refl::BindingHandle<void,	/*line ended in next macros */

#define DS_BINDING(Type, Name, ...)	Type, #Name> Name##BindingType;  /* finish line from prev macros */													\
									Name##BindingType Name = Name##BindingType(GetBindingImpl<typename Name##BindingType::NextBindingHandleType>());	\
									template<>																											\
									Name##BindingType* GetBindingImpl<Name##BindingType>()																\
									{																													\
										return &Name;																									\
									}																													\
									typedef rdr::bindings_refl::BindingHandle<Name##BindingType,

#define DS_BINDINGS_END()	rdr::bindings_refl::HeadBindingUnderlyingType, "head"> HeadBindingType;										\
							HeadBindingType head = HeadBindingType(GetBindingImpl<typename HeadBindingType::NextBindingHandleType>());	\
							template<>																									\
							HeadBindingType* GetBindingImpl<HeadBindingType>()															\
							{																											\
								return &head;																							\
							}


} // bindings_refl

/**
 * Base class for all descriptor set states
 */
class RENDERER_TYPES_API DescriptorSetState abstract
{
public:

	DescriptorSetState();
	~DescriptorSetState() = default;

	virtual void UpdateDescriptors(DescriptorSetUpdateContext& context) const = 0;

	DSStateID GetID() const;

	Bool IsDirty() const;
	void ClearDirtyFlag();

protected:

	void MarkAsDirty();

private:

	const DSStateID	m_state;
	Bool			m_isDirty;
};


//TEST ////////////////////////////////////////////////////////////////////////////
class RENDERER_TYPES_API TestDescriptorSetBinding : public DescriptorSetBinding
{
public:

	TestDescriptorSetBinding(const lib::HashedString& name)
		: DescriptorSetBinding(name)
	{ }

	virtual void UpdateDescriptors(DescriptorSetUpdateContext& context) const override
	{

	}
};

class RENDERER_TYPES_API TestDescriptorSetState : public DescriptorSetState
{
public:

	DS_BINDINGS_BEGIN()
		DS_BINDING(TestDescriptorSetBinding, Texture)
		DS_BINDING(TestDescriptorSetBinding, SizeX)
		DS_BINDING(TestDescriptorSetBinding, SizeY)
	DS_BINDINGS_END()
};

} // spt::rdr
