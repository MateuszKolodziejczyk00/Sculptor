#pragma once

#include "RendererCoreMacros.h"
#include "SculptorCoreTypes.h"
#include "RendererUtils.h"
#include "DescriptorSetStateTypes.h"
#include "Common/DescriptorSetCompilation/DescriptorSetCompilationDefRegistration.h"


namespace spt::rdr
{

class DescriptorSetState;
class DescriptorSetStackAllocator;


class RENDERER_CORE_API DescriptorSetBinding abstract
{
public:

	explicit DescriptorSetBinding(const lib::HashedString& name);
	virtual ~DescriptorSetBinding() = default;

	const lib::HashedString& GetName() const;

	virtual void UpdateDescriptors(DescriptorSetUpdateContext& context) const = 0;

	// These functions are not virtual, but can be reimplemented in child classes
	void Initialize(DescriptorSetState& owningState) {}

	static constexpr lib::String BuildBindingCode(const char* name, Uint32 bindingIdx) { SPT_CHECK_NO_ENTRY(); return lib::String{}; };

	static void BuildAdditionalShaderCompilationArgs(ShaderCompilationAdditionalArgsBuilder& builder) {}

	// Children classes MUST also define this function (with arbitrary N)
	//static constexpr std::array<ShaderBindingMetaData, N> GetShaderBindingsMetaData();

	void PreInit(DescriptorSetState& state, Uint32 baseBindingIdx);

protected:

	void MarkAsDirty();

	Uint32 GetBaseBindingIdx() const;

	// Helpers =========================================
	static constexpr lib::String BuildBindingVariableCode(lib::StringView bindingVariable, Uint32 bindingIdx)
	{
		SPT_CHECK(bindingIdx <= 99);
		char bindingIdxChars[2];
		bindingIdxChars[0] = '0' + static_cast<char>(bindingIdx / 10);
		bindingIdxChars[1] = '0' + static_cast<char>(bindingIdx % 10);
		return bindingIdxChars[0] == '0'
			? lib::String("[[vk::binding(") + bindingIdxChars[1] + ", XX)]] " + bindingVariable.data() + ";\n"
			: lib::String("[[vk::binding(") + bindingIdxChars[0] + bindingIdxChars[1] + ", XX)]] " + bindingVariable.data() + ";\n";
	}

private:

	lib::HashedString m_name;

	DescriptorSetState* m_owningState;

	Uint32 m_baseBindingIdx;
};


struct DescriptorSetStateParams
{
	lib::SharedPtr<DescriptorSetStackAllocator> stackAllocator;
	EDescriptorSetStateFlags                    flags = EDescriptorSetStateFlags::Default;
};


/**
 * Base class for all descriptor set states
 */
class RENDERER_CORE_API DescriptorSetState abstract : public lib::MTRefCounted
{
public:

	DescriptorSetState(const RendererResourceName& name, const DescriptorSetStateParams& params);
	virtual ~DescriptorSetState();

	virtual void UpdateDescriptors(DescriptorSetUpdateContext& context) const = 0;

	DSStateID     GetID() const;
	DSStateTypeID GetTypeID() const;

	Bool IsDirty() const;
	void SetDirty();

	const rhi::RHIDescriptorSet& Flush();
	const rhi::RHIDescriptorSet& GetDescriptorSet() const;

	EDescriptorSetStateFlags GetFlags() const;

	const lib::SharedPtr<DescriptorSetLayout>& GetLayout() const;

	Uint32*                          AddDynamicOffset();
	const lib::DynamicArray<Uint32>& GetDynamicOffsets() const;

	const lib::HashedString& GetName() const;

protected:

	void SetTypeID(DSStateTypeID id, const DescriptorSetStateParams& params);

	void InitDynamicOffsetsArray(SizeType dynamicOffsetsNum);

private:

	rhi::RHIDescriptorSet AllocateDescriptorSet() const;
	void                  SwapDescriptorSet(rhi::RHIDescriptorSet newDescriptorSet);

	const DSStateID	m_id;

	DSStateTypeID m_typeID;

	Bool m_dirty;

	EDescriptorSetStateFlags m_flags;

	lib::SharedPtr<DescriptorSetStackAllocator> m_stackAllocator;

	lib::DynamicArray<Uint32> m_dynamicOffsets;

	lib::SharedPtr<DescriptorSetLayout> m_layout;
	rhi::RHIDescriptorSet m_descriptorSet;

	RendererResourceName m_name;
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


#define DS_BEGIN(className, parentClass)																						\
	class className : public parentClass																						\
	{																															\
	public:																														\
	using ThisClass = className;																								\
	using Super = parentClass;																									\
	className(const rdr::RendererResourceName& name, const rdr::DescriptorSetStateParams& params)								\
		: Super(name, params)																									\
	{																															\
		SetTypeID(GetStaticTypeID(), params);																					\
		const SizeType dynamicOffsetsNum = rdr::bindings_refl::GetDynamicOffsetBindingsNum<ReflHeadBindingType>();				\
		InitDynamicOffsetsArray(dynamicOffsetsNum);																				\
		rdr::bindings_refl::InitializeBindings(GetBindingsBegin(), *this);														\
	}																															\
	static lib::HashedString GetDescriptorSetName()																				\
	{																															\
		return #className;																										\
	}																															\
	template<typename TBindingType>																								\
	TBindingType* ReflGetBindingImpl()																							\
	{																															\
		return nullptr;																											\
	}																															\
	static rdr::DSStateTypeID GetStaticTypeID()																					\
	{																															\
		static lib::HashedString staticName(#className);																		\
		return rdr::DSStateTypeID(staticName.GetKey());																			\
	}																															\
	typedef rdr::bindings_refl::BindingHandle<void,	/*line ended in next macros */

#define DS_BINDING(Type, Name, ...)	Type, #Name> Refl##Name##BindingType;  /* finish line from prev macros */																						\
									Type Name = Type(#Name, __VA_ARGS__);																															\
									Refl##Name##BindingType refl##Name = Refl##Name##BindingType(ReflGetBindingImpl<typename Refl##Name##BindingType::NextBindingHandleType>(), &Name);				\
									template<>																																						\
									Refl##Name##BindingType* ReflGetBindingImpl<Refl##Name##BindingType>()																							\
									{																																								\
										return &refl##Name;																																			\
									}																																								\
									typedef rdr::bindings_refl::BindingHandle<Refl##Name##BindingType,

#define DS_END()																												\
rdr::bindings_refl::HeadBindingUnderlyingType, "head"> ReflHeadBindingType;														\
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
private:																														\
inline static rdr::DescriptorSetStateCompilationDefRegistration<ThisClass> CompilationRegistration;								\
inline static rdr::DescriptorSetStateLayoutRegistration<ThisClass> layoutFactoryRegistration;									\
};

#define DS_STAGES(...) lib::Flags<rhi::EShaderStageFlags>(__VA_ARGS__)

#define BINDING_TYPE(...) __VA_ARGS__

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

template<typename TBindingType>
constexpr Uint32 GetShaderBindingsNumForBinding()
{
	// use helper lambda to get nicer compile errors
	const auto helper = [] { return std::decay_t<TBindingType>::GetShaderBindingsMetaData(); };
	using TBindingsMetaDataType = std::invoke_result_t<decltype(helper)>;
	return (Uint32)lib::StaticArrayTraits<TBindingsMetaDataType>::Size;
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

template<typename TBindingHandle, typename TCallable>
constexpr void ForEachBinding(TCallable callable)
{
	if constexpr (!IsHeadBinding<TBindingHandle>())
	{
		callable.operator()<TBindingHandle>();
	}

	if constexpr (!IsTailBinding<TBindingHandle>())
	{
		ForEachBinding<typename TBindingHandle::NextBindingHandleType>(callable);
	}
}

template<typename TBindingHandle>
constexpr SizeType GetDynamicOffsetBindingsNum()
{
	SizeType dynamicOffsetBindingsNum = 0u;

	ForEachBinding<TBindingHandle>([&dynamicOffsetBindingsNum]<typename TCurrentBindingHandle>() mutable
	{
		using CurrentBindingType = typename TCurrentBindingHandle::BindingType;

		auto shaderBindingsMetaData = CurrentBindingType::GetShaderBindingsMetaData();

		for (const ShaderBindingMetaData& shaderBindingMetaData : shaderBindingsMetaData)
		{
			if (lib::HasAnyFlag(shaderBindingMetaData.flags, smd::EBindingFlags::DynamicOffset))
			{
				++dynamicOffsetBindingsNum;
			}
		}
	});

	return dynamicOffsetBindingsNum;
}

template<typename TDescriptorSetType>
constexpr SizeType GetShaderBindingsNum()
{
	SizeType bindingsNum = 0u;

	using HeadBindingHandle = typename TDescriptorSetType::ReflHeadBindingType;

	ForEachBinding<HeadBindingHandle>([&bindingsNum]<typename TCurrentBindingHandle>()
	{
		using CurrentBindingType = typename TCurrentBindingHandle::BindingType;

		bindingsNum += GetShaderBindingsNumForBinding<CurrentBindingType>();
	});

	return bindingsNum;
}

template<typename TDescriptorSetType>
constexpr auto GetShaderBindingsMetaData() -> lib::StaticArray<ShaderBindingMetaData, GetShaderBindingsNum<TDescriptorSetType>()>
{
	using TResultType = lib::StaticArray<ShaderBindingMetaData, GetShaderBindingsNum<TDescriptorSetType>()>;

	using HeadBindingHandle = typename TDescriptorSetType::ReflHeadBindingType;

	TResultType result;

	ForEachBinding<HeadBindingHandle>([&result, currentIdx = SizeType(0)]<typename TCurrentBindingHandle>() mutable
	{
		using CurrentBindingType = typename TCurrentBindingHandle::BindingType;

		const auto shaderBindingsMetaData = CurrentBindingType::GetShaderBindingsMetaData();

		for (const ShaderBindingMetaData& shaderBindingMetaData : shaderBindingsMetaData)
		{
			result[currentIdx++] = shaderBindingMetaData;
		}
	});

	return result;
}

template<typename TBindingHandle>
void InitializeBindings(TBindingHandle& bindingHandle, DescriptorSetState& owningState)
{
	SPT_PROFILER_FUNCTION();

	ForEachBinding(bindingHandle,
				   [&owningState, bindingIdx = 0u](auto& binding) mutable
				   {
					   binding.PreInit(owningState, bindingIdx);
					   binding.Initialize(owningState);

					   bindingIdx += GetShaderBindingsNumForBinding<decltype(binding)>();
				   });
}

template<typename TBindingHandle>
constexpr lib::String BuildBindingsShaderCode(Uint32 bindingIdxshaderBindingIdx = 0)
{
	lib::String result;

	ForEachBinding<TBindingHandle>([&result, &bindingIdxshaderBindingIdx]<typename TCurrentBindingHandle>() mutable
	{
		using CurrentBindingType = typename TCurrentBindingHandle::BindingType;
		result += CurrentBindingType::BuildBindingCode(TCurrentBindingHandle::GetName(), bindingIdxshaderBindingIdx);
		bindingIdxshaderBindingIdx += GetShaderBindingsNumForBinding<CurrentBindingType>();
	});

	return result;
}

template<typename TDSType>
consteval SizeType GetDescriptorSetShaderCodeSize()
{
	using HeadBindingType = typename TDSType::ReflHeadBindingType;
	const lib::String code = BuildBindingsShaderCode<HeadBindingType>();
	return code.size();
}

template<typename TDSType>
consteval auto BuildDescriptorSetShaderCode()
{
	using HeadBindingType = typename TDSType::ReflHeadBindingType;

	lib::String code = BuildBindingsShaderCode<HeadBindingType>();

	lib::StaticArray<char, GetDescriptorSetShaderCodeSize<TDSType>()> result;
	SPT_CHECK(code.size() == result.size());
	std::copy(std::cbegin(code), std::cend(code), std::begin(result));

	return result;
}

template<typename TDSType>
void BuildAdditionalShaderCompilationArgs(ShaderCompilationAdditionalArgsBuilder& additionalArgsBuilder)
{
	using HeadBindingHandle = typename TDSType::ReflHeadBindingType;
	ForEachBinding<HeadBindingHandle>([&additionalArgsBuilder]<typename TCurrentBindingHandle>() mutable
	{
		using CurrentBindingType = typename TCurrentBindingHandle::BindingType;

		CurrentBindingType::BuildAdditionalShaderCompilationArgs(additionalArgsBuilder);
	});
}

template<typename TDSType>
sc::DescriptorSetCompilationMetaData BuildCompilationMetaData()
{
	SPT_PROFILER_FUNCTION();

	sc::DescriptorSetCompilationMetaData dsMetaData;

	ShaderCompilationAdditionalArgsBuilder additionalArgsBuilder(dsMetaData.additionalMacros);
	additionalArgsBuilder.AddDescriptorSet(TDSType::GetDescriptorSetName());

	using HeadBindingHandle = typename TDSType::ReflHeadBindingType;

	ForEachBinding<HeadBindingHandle>([shaderBindingIdx = 0u, &dsMetaData, &additionalArgsBuilder]<typename TCurrentBindingHandle>() mutable
	{
		using CurrentBindingType = typename TCurrentBindingHandle::BindingType;

		CurrentBindingType::BuildAdditionalShaderCompilationArgs(additionalArgsBuilder);

		const auto shaderBindingsMetaData = CurrentBindingType::GetShaderBindingsMetaData();

		for (const ShaderBindingMetaData& shaderBindingMetaData : shaderBindingsMetaData)
		{
			if (shaderBindingMetaData.immutableSamplerDef)
			{
				dsMetaData.bindingToImmutableSampler.emplace(shaderBindingIdx, *shaderBindingMetaData.immutableSamplerDef);
			}

			++shaderBindingIdx;
		}
	});

	dsMetaData.typeID = TDSType::GetStaticTypeID();

	return dsMetaData;
}

} // bindings_refl


template<typename TDSStateType>
class DescriptorSetStateLayoutRegistration
{
public:

	DescriptorSetStateLayoutRegistration()
	{
		DescriptorSetStateLayoutsRegistry::Get().RegisterFactoryMethod(TDSStateType::GetStaticTypeID(), &DescriptorSetStateLayoutRegistration<TDSStateType>::CreateDescriptorSetStateLayout);
	}

private:

	static void CreateDescriptorSetStateLayout(DescriptorSetStateLayoutsRegistry& layoutsRegistry)
	{
		const auto bindingsMetaData = bindings_refl::GetShaderBindingsMetaData<TDSStateType>();
	
		DescriptorSetStateLayoutDefinition layoutDef;
		layoutDef.bindings = { bindingsMetaData };
	
		layoutsRegistry.RegisterLayout(TDSStateType::GetStaticTypeID(), RENDERER_RESOURCE_NAME(TDSStateType::GetDescriptorSetName()), layoutDef);
	}
};


template<typename TDSStateType>
class DescriptorSetStateCompilationDefRegistration : public sc::DescriptorSetCompilationDefRegistration
{
public:

	DescriptorSetStateCompilationDefRegistration()
		: sc::DescriptorSetCompilationDefRegistration(TDSStateType::GetDescriptorSetName(), BuildDSCode(), bindings_refl::BuildCompilationMetaData<TDSStateType>())
	{ }

private:

	static lib::String BuildDSCode()
	{
		constexpr auto codeArray = bindings_refl::BuildDescriptorSetShaderCode<TDSStateType>();
		return lib::String(std::cbegin(codeArray), std::cend(codeArray));
	}
};

} // spt::rdr(SizeType)
