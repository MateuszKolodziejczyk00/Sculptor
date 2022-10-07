#pragma once

#include "RendererCoreMacros.h"
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
class DescriptorSetState;
class BufferView;
class TextureView;


using DSStateID = Uint64;


enum class EDescriptorSetStateFlags
{
	None			= 0,
	Persistent		= BIT(0)
};


class RENDERER_CORE_API DescriptorSetUpdateContext
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


class RENDERER_CORE_API DescriptorSetBinding abstract
{
public:

	explicit DescriptorSetBinding(const lib::HashedString& name, Bool& descriptorDirtyFlag);
	virtual ~DescriptorSetBinding() = default;

	const lib::HashedString& GetName() const;

	virtual void UpdateDescriptors(DescriptorSetUpdateContext& context) const = 0;

	// These functions are not virtual, but can be reimplemented in child classes
	void CreateBindingMetaData(OUT smd::GenericShaderBinding& binding) const {}
	void Initialize(DescriptorSetState& owningState) {}

	static constexpr lib::String BuildBindingCode(const char* name, Uint32 bindingIdx) { SPT_CHECK_NO_ENTRY(); return lib::String{}; };

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
class RENDERER_CORE_API DescriptorSetState abstract
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

	Uint32* AddDynamicOffset();
	const lib::DynamicArray<Uint32>& GetDynamicOffsets() const;

protected:

	void		SetBindingNames(lib::DynamicArray<lib::HashedString> inBindingNames);
	void		SetDescriptorSetHash(SizeType hash);

	void		InitDynamicOffsetsArray(SizeType dynamicOffsetsNum);

	Bool		m_isDirty;

private:

	const DSStateID	m_id;

	EDescriptorSetStateFlags m_flags;

	lib::DynamicArray<Uint32> m_dynamicOffsets;

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

	const BindingType& GetBinding() const
	{
		return static_cast<const BindingType&>(Super::GetBinding());
	}
	
	BindingType& GetBinding()
	{
		return static_cast<BindingType&>(Super::GetBinding());
	}

	static constexpr const char* GetName()
	{
		return name.Get();
	}
};


/**
 * Partial specialization for last element in the list (NextBindingHandleType is void)
 */
template<typename TBindingType, lib::Literal name>
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

	const BindingType& GetBinding() const
	{
		return static_cast<const BindingType&>(Super::GetBinding());
	}

	BindingType& GetBinding()
	{
		return static_cast<BindingType&>(Super::GetBinding());
	}

	static constexpr const char* GetName()
	{
		return name.Get();
	}
};


/**
 * Partial specialization for first element in the list (head)
 */
template<typename TNextBindingHandleType, lib::Literal name>
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


#define DS_BEGIN(api, className, stages)	class api className : public rdr::DescriptorSetState																		\
											{																															\
											public:																														\
											using ThisClass = className;																								\
											using Super = rdr::DescriptorSetState;																						\
											className(rdr::EDescriptorSetStateFlags flags = rdr::EDescriptorSetStateFlags::None)										\
												: Super(flags)																											\
											{																															\
												SetBindingNames(rdr::bindings_refl::GetBindingNames(GetBindingsBegin()));												\
												const auto bindingsDef = rdr::bindings_refl::CreateDescriptorSetBindingsDef(GetBindingsBegin(), stages);				\
												SetDescriptorSetHash(rdr::bindings_refl::HashDescriptorSetState(bindingsDef, GetBindingNames()));						\
												InitDynamicOffsetsArray(rdr::bindings_refl::GetDynamicOffsetsNum(bindingsDef));											\
												rdr::bindings_refl::InitializeBindings(GetBindingsBegin(), *this);														\
											}																															\
											template<typename TBindingType>																								\
											TBindingType* ReflGetBindingImpl()																							\
											{																															\
												return nullptr;																											\
											}																															\
											typedef rdr::bindings_refl::BindingHandle<void,	/*line ended in next macros */

#define DS_BINDING(Type, Name, ...)	Type, #Name> Refl##Name##BindingType;  /* finish line from prev macros */																								\
									Type Name = Type(#Name, m_isDirty);																																\
									Refl##Name##BindingType refl##Name = Refl##Name##BindingType(ReflGetBindingImpl<typename Refl##Name##BindingType::NextBindingHandleType>(), &Name);				\
									template<>																																						\
									Refl##Name##BindingType* ReflGetBindingImpl<Refl##Name##BindingType>()																							\
									{																																								\
										return &refl##Name;																																			\
									}																																								\
									typedef rdr::bindings_refl::BindingHandle<Refl##Name##BindingType,

#define DS_END()	rdr::bindings_refl::HeadBindingUnderlyingType, "head"> ReflHeadBindingType;																\
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
void ForEachBinding(TBindingHandle& currentBindingHandle, TCallable callable)
{
	if constexpr (!IsHeadBinding<TBindingHandle>())
	{
		auto& binding = currentBindingHandle.GetBinding();
		callable(binding);
	}

	if constexpr (!IsTailBinding<TBindingHandle>())
	{
		auto* nextBindingHandle = currentBindingHandle.GetNext();
		SPT_CHECK(!!nextBindingHandle);
		ForEachBinding(*nextBindingHandle, callable);
	}
}

template<typename TBindingHandle>
lib::DynamicArray<lib::HashedString> GetBindingNames(const TBindingHandle& bindingHandle)
{
	SPT_PROFILER_FUNCTION();

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
lib::DynamicArray<smd::GenericShaderBinding> CreateDescriptorSetBindingsDef(const TBindingHandle& bindingHandle, rhi::EShaderStageFlags stageFlags)
{
	SPT_PROFILER_FUNCTION();
	
	lib::DynamicArray<smd::GenericShaderBinding> bindings;
	bindings.reserve(GetBindingsNum<TBindingHandle>());

	ForEachBinding(bindingHandle,
				   [&bindings, stageFlags](const auto& binding)
				   {
					   smd::GenericShaderBinding newBinding;
					   binding.CreateBindingMetaData(OUT newBinding);

					   newBinding.AddShaderStages(stageFlags);
					   newBinding.PostInitialize();

					   bindings.emplace_back(newBinding);
				   });

	return bindings;
}

inline SizeType HashDescriptorSetState(const lib::DynamicArray<smd::GenericShaderBinding>& bindingsDef, const lib::DynamicArray<lib::HashedString>& bindingNames)
{
	SPT_PROFILER_FUNCTION();

	SizeType hash = 0;
	for (SizeType bindingIdx = 0; bindingIdx < bindingsDef.size(); ++bindingIdx)
	{
		lib::HashCombine(hash, smd::HashDescriptorSetBinding(bindingsDef[bindingIdx], bindingNames[bindingIdx]));
	}

	return hash;
}

inline SizeType GetDynamicOffsetsNum(const lib::DynamicArray<smd::GenericShaderBinding>& bindingsDef)
{
	SPT_PROFILER_FUNCTION();

	return std::count_if(std::cbegin(bindingsDef), std::cend(bindingsDef),
						 [](const smd::GenericShaderBinding& binding)
						 {
							 return lib::HasAnyFlag(binding.GetFlags(), smd::EBindingFlags::DynamicOffset);
						 });
}

template<typename TBindingHandle>
void InitializeBindings(TBindingHandle& bindingHandle, DescriptorSetState& owningState)
{
	SPT_PROFILER_FUNCTION();

	ForEachBinding(bindingHandle,
				   [&owningState](auto& binding)
				   {
					   binding.Initialize(owningState);
				   });
}

template<typename TBindingHandle>
constexpr lib::String BuildBindingsShaderCode(Uint32 bindingIdx = 0)
{
	lib::String result;

	Uint32 nextBindingIdx = bindingIdx;
	if constexpr (!IsHeadBinding<TBindingHandle>())
	{
		using BindingType = typename TBindingHandle::BindingType;
		result = BindingType::BuildBindingCode(TBindingHandle::GetName(), bindingIdx);
		++nextBindingIdx;
	}

	if constexpr (!IsTailBinding<TBindingHandle>())
	{
		result += BuildBindingsShaderCode<typename TBindingHandle::NextBindingHandleType>(nextBindingIdx);
	}

	return result;
}

template<typename TDSType>
constexpr SizeType GetDescriptorSetShaderCodeSize()
{
	using HeadBindingType = typename TDSType::ReflHeadBindingType;
	const lib::String code = BuildBindingsShaderCode<HeadBindingType>();
	return code.size();
}
template<typename TDSType, SizeType Size>
constexpr lib::StaticArray<char, Size> BuildDescriptorSetShaderCode()
{
	using HeadBindingType = typename TDSType::ReflHeadBindingType;

	lib::String code = BuildBindingsShaderCode<HeadBindingType>();
	SPT_CHECK(code.size() == Size);

	lib::StaticArray<char, Size> result;
	std::copy(std::cbegin(code), std::cend(code), std::begin(result));

	return result;
}

} // bindings_refl

} // spt::rdr
