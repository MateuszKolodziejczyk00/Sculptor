#pragma once

#include "RendererTypesMacros.h"
#include "SculptorCoreTypes.h"
#include "RHIBridge/RHIDescriptorSetImpl.h"
#include "ShaderMetaDataTypes.h"


namespace spt::smd
{
class ShaderMetaData;
struct GenericShaderBinding;
} // spt::smd


namespace spt::rdr
{

class DescriptorSetWriter;
class BufferView;
class TextureView;


using DSStateID = Uint64;


enum class EDescriptorSetStateFlags
{
	None			= 0,
	Persistent		= BIT(0)
};


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

	explicit DescriptorSetBinding(const lib::HashedString& name, Bool& descriptorDirtyFlag);
	virtual ~DescriptorSetBinding() = default;

	const lib::HashedString& GetName() const;

	virtual void UpdateDescriptors(DescriptorSetUpdateContext& context) const = 0;
	virtual void CreateBindingMetaData(OUT smd::GenericShaderBinding& binding) const = 0;

protected:

	void MarkAsDirty();

private:

	lib::HashedString m_name;

	/**
	 * Reference to dirty flag of owning descriptor set instance
	 * Allows setting this flag as dirty when binding is modified
	 */
	Bool& m_descriptorDirtyFlag;
};


/**
 * Base class for all descriptor set states
 */
class RENDERER_TYPES_API DescriptorSetState abstract
{
public:

	DescriptorSetState(EDescriptorSetStateFlags flags);
	~DescriptorSetState() = default;

	virtual void UpdateDescriptors(DescriptorSetUpdateContext& context) const = 0;

	DSStateID GetID() const;

	Bool IsDirty() const;
	void ClearDirtyFlag();

	EDescriptorSetStateFlags GetFlags() const;

	const lib::DynamicArray<lib::HashedString>& GetBindingNames() const;

	SizeType GetDescriptorSetHash() const;

protected:

	void		SetBindingNames(lib::DynamicArray<lib::HashedString> inBindingNames);
	void		SetDescriptorSetHash(SizeType hash);

	Bool		m_isDirty;

private:

	const DSStateID	m_id;

	EDescriptorSetStateFlags m_flags;

	lib::DynamicArray<lib::HashedString>	m_bindingNames;
	SizeType								m_descriptorSetHash;
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

	/** Head node is just an entry. It doesn't provide any binding */
	Bool IsHeadNode() const
	{
		return !m_binding;
	}

	const DescriptorSetBinding& GetBinding() const
	{
		SPT_CHECK(!IsHeadNode());
		SPT_CHECK(!!m_binding);
		return *m_binding;
	}

	DescriptorSetBinding& GetBinding()
	{
		SPT_CHECK(!IsHeadNode());
		SPT_CHECK(!!m_binding);
		return *m_binding;
	}

private:

	BindingHandleNode*		m_next;
	DescriptorSetBinding*	m_binding;
};


template<typename TNextBindingHandleType,
		 typename TBindingType>
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

	const BindingType& GetBinding() const
	{
		return static_cast<const BindingType&>(Super::GetBinding());
	}
	
	BindingType& GetBinding()
	{
		return static_cast<BindingType&>(Super::GetBinding());
	}
};


/**
 * Partial specialization for last element in the list (NextBindingHandleType is void)
 */
template<typename TBindingType>
requires std::is_base_of_v<DescriptorSetBinding, TBindingType>
class BindingHandle<void, TBindingType> : public BindingHandleNode
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

	const BindingType& GetBinding() const
	{
		return static_cast<const BindingType&>(Super::GetBinding());
	}

	BindingType& GetBinding()
	{
		return static_cast<BindingType&>(Super::GetBinding());
	}
};


/**
 * Partial specialization for first element in the list (head)
 */
template<typename TNextBindingHandleType>
class BindingHandle<TNextBindingHandleType, HeadBindingUnderlyingType> : public BindingHandleNode
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


#define DS_BEGIN(api, className, stages)	class api className : public rdr::DescriptorSetState																		\
											{																															\
											public:																														\
											using ThisClass = className;																								\
											using Super = rdr::DescriptorSetState;																						\
											className(rdr::EDescriptorSetStateFlags flags = rdr::EDescriptorSetStateFlags::None)										\
												: Super(flags)																											\
											{																															\
												SetBindingNames(rdr::bindings_refl::GetBindingNames(GetBindingsBegin()));												\
												SetDescriptorSetHash(rdr::bindings_refl::HashDescriptorSetState(GetBindingsBegin(), GetBindingNames(), stages));		\
											}																															\
											template<typename TBindingType>																								\
											TBindingType* ReflGetBindingImpl()																							\
											{																															\
												return nullptr;																											\
											}																															\
											typedef rdr::bindings_refl::BindingHandle<void,	/*line ended in next macros */

#define DS_BINDING(Type, Name, ...)	Type> Refl##Name##BindingType;  /* finish line from prev macros */																								\
									Type Name = Type(#Name, m_isDirty);																																\
									Refl##Name##BindingType refl##Name = Refl##Name##BindingType(ReflGetBindingImpl<typename Refl##Name##BindingType::NextBindingHandleType>(), &Name);	\
									template<>																																						\
									Refl##Name##BindingType* ReflGetBindingImpl<Refl##Name##BindingType>()																							\
									{																																								\
										return &refl##Name;																																			\
									}																																								\
									typedef rdr::bindings_refl::BindingHandle<Refl##Name##BindingType,

#define DS_END()	rdr::bindings_refl::HeadBindingUnderlyingType> ReflHeadBindingType;																\
					ReflHeadBindingType reflHead = ReflHeadBindingType(ReflGetBindingImpl<typename ReflHeadBindingType::NextBindingHandleType>());	\
					template<>																														\
					ReflHeadBindingType* ReflGetBindingImpl<ReflHeadBindingType>()																	\
					{																																\
						return &reflHead;																											\
					}																																\
					const ReflHeadBindingType& GetBindingsBegin() const																				\
					{																																\
						return reflHead;																											\
					}																																\
					virtual void UpdateDescriptors(rdr::DescriptorSetUpdateContext& context) const final											\
					{																																\
						rdr::bindings_refl::ForEachBinding(GetBindingsBegin(), [&context](const auto& binding)										\
														   {																						\
															   binding.UpdateDescriptors(context);													\
														   });																						\
					}																																\
					};

#define DS_STAGES(...) lib::Flags<rhi::EShaderStageFlags>(__VA_ARGS__)

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

template<typename TBindingHandle, typename TCallable>
void ForEachBinding(const TBindingHandle& currentBindingHandle, TCallable callable)
{
	if constexpr (!IsHeadBinding<TBindingHandle>())
	{
		auto& binding = currentBindingHandle.GetBinding();
		callable(binding);
	}

	if constexpr (!IsTailBinding<TBindingHandle>())
	{
		const auto* nextBindingHandle = currentBindingHandle.GetNext();
		SPT_CHECK(!!nextBindingHandle);
		ForEachBinding(*nextBindingHandle, callable);
	}
}

template<typename TBindingHandle>
lib::DynamicArray<lib::HashedString> GetBindingNames(const TBindingHandle& bindingHandle)
{
	constexpr SizeType bindingsNum = GetBindingsNum<TBindingHandle>();

	lib::DynamicArray<lib::HashedString> bindingNames;
	bindingNames.reserve(bindingsNum);

	ForEachBinding(bindingHandle, [&bindingNames](const DescriptorSetBinding& binding)
	{
		bindingNames.emplace_back(binding.GetName());
	});

	return bindingNames;
}

template<typename TBindingHandle>
SizeType HashDescriptorSetState(const TBindingHandle& bindingHandle, const lib::DynamicArray<lib::HashedString>& bindingNames, rhi::EShaderStageFlags stageFlags)
{
	const SizeType bindingsNum = bindingNames.size();
	
	lib::DynamicArray<smd::GenericShaderBinding> bindings;
	bindings.reserve(bindingsNum);

	ForEachBinding(bindingHandle,
				   [&bindings, &bindingNames, stageFlags](const auto& binding)
				   {
					   smd::GenericShaderBinding newBinding;
					   binding.CreateBindingMetaData(OUT newBinding);

					   newBinding.AddShaderStages(stageFlags);
					   newBinding.PostInitialize();

					   bindings.emplace_back(newBinding);
				   });

	SizeType hash = 0;
	for (SizeType bindingIdx = 0; bindingIdx < bindingsNum; ++bindingIdx)
	{
		lib::HashCombine(hash, smd::HashDescriptorSetBinding(bindings[bindingIdx], bindingNames[bindingIdx]));
	}

	return hash;
}

} // bindings_refl

} // spt::rdr
