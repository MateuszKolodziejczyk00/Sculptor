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


template<typename TNextBindingHandleType,
		 typename TBindingType,
		 lib::Literal name>
requires std::is_base_of_v<DescriptorSetBinding, TBindingType> || std::is_same_v<TBindingType, HeadBindingUnderlyingType>
class BindingHandle : public BindingHandleNode
{
protected:

	using Super = BindingHandleNode;

public:

	using BindingType			= TBindingType;
	using NextBindingHandleType = TNextBindingHandleType;

	explicit BindingHandle(NextBindingHandleType* next, BindingType* binding)
		: Super(next, binding)
	{
		SPT_CHECK(!!next);
	}

	NextBindingHandleType* GetNext() const
	{
		return static_cast<NextBindingHandleType*>(Super::GetNext());
	}

	BindingType* GetBinding() const
	{
		return static_cast<BindingType*>(Super::Get());
	}
};


/**
 * Partial specialization for last element in the list (NextBindingHandleType is void)
 */
template<typename TBindingType,
		lib::Literal name>
requires std::is_base_of_v<DescriptorSetBinding, TBindingType>
class BindingHandle<void, TBindingType, name> : public BindingHandleNode
{
protected:

	using Super = BindingHandleNode;

public:

	using BindingType			= TBindingType;
	using NextBindingHandleType = void;
	using NextBindingType		= void;

	explicit BindingHandle(NextBindingHandleType*, BindingType* binding)
		: Super(nullptr, binding)
	{ }

	BindingType* GetBinding() const
	{
		return static_cast<BindingType*>(Super::Get());
	}
};


/**
 * Partial specialization for first element in the list (head)
 */
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
							TBindingType* ReflGetBindingImpl()																\
							{																								\
								return nullptr;																				\
							}																								\
							typedef rdr::bindings_refl::BindingHandle<void,	/*line ended in next macros */

#define DS_BINDING(Type, Name, ...)	Type, #Name> Refl##Name##BindingType;  /* finish line from prev macros */																		\
									Type Name = Type(#Name);																														\
									Refl##Name##BindingType refl##Name = Refl##Name##BindingType(ReflGetBindingImpl<typename Refl##Name##BindingType::NextBindingHandleType>(), &Name);	\
									template<>																																		\
									Refl##Name##BindingType* ReflGetBindingImpl<Refl##Name##BindingType>()																			\
									{																																				\
										return &refl##Name;																															\
									}																																				\
									typedef rdr::bindings_refl::BindingHandle<Refl##Name##BindingType,

#define DS_BINDINGS_END()	rdr::bindings_refl::HeadBindingUnderlyingType, "head"> ReflHeadBindingType;														\
							ReflHeadBindingType reflHead = ReflHeadBindingType(ReflGetBindingImpl<typename ReflHeadBindingType::NextBindingHandleType>());	\
							template<>																														\
							ReflHeadBindingType* ReflGetBindingImpl<ReflHeadBindingType>()																	\
							{																																\
								return &reflHead;																											\
							}


template<typename TBindingHandle>
constexpr Bool IsHeadBinding()
{
	using BindingType = typename TBindingHandle::BindingType;
	return std::is_same_v<BindingType, HeadBindingUnderlyingType>;
}

template<typename TBindingHandle>
constexpr Bool IsTailBinding()
{
	using NextBindingHandleType = typename TBindingHandle::NextBindingHandleType;
	return std::is_same_v<NextBindingHandleType, void>;
}

template<typename TBindingHandle>
constexpr SizeType GetBindingsNum()
{
	if constexpr (IsHeadBinding<TBindingHandle>())
	{
		return GetBindingsNum<typename TBindingHandle::NextBindingHandleType>();
	}
	else if constexpr (IsTailBinding<TBindingHandle>())
	{
		return 1;
	}
	else
	{
		return 1 + GetBindingsNum<typename TBindingHandle::NextBindingHandleType>();
	}
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

} // spt::rdr
