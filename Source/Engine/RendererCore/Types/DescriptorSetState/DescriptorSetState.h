#pragma once

#include "RendererCoreMacros.h"
#include "SculptorCoreTypes.h"
#include "RendererUtils.h"
#include "RHIBridge/RHIDescriptorSetImpl.h"
#include "ShaderMetaDataTypes.h"
#include "DescriptorSetStateTypes.h"
#include "Common/DescriptorSetCompilation/DescriptorSetCompilationDefRegistration.h"


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
	Persistent		= BIT(0),

	Default = None
};


class RENDERER_CORE_API DescriptorSetUpdateContext
{
public:

	DescriptorSetUpdateContext(rhi::RHIDescriptorSet descriptorSet, DescriptorSetWriter& writer, const lib::SharedRef<smd::ShaderMetaData>& metaData);

	void UpdateBuffer(const lib::HashedString& name, const BufferView& buffer) const;
	void UpdateBuffer(const lib::HashedString& name, const BufferView& buffer, const BufferView& countBuffer) const;
	void UpdateTexture(const lib::HashedString& name, const lib::SharedRef<TextureView>& texture, Uint32 arrayIndex = 0) const;

	const smd::ShaderMetaData& GetMetaData() const;

private:

	rhi::RHIDescriptorSet				m_descriptorSet;
	DescriptorSetWriter&				m_writer;
	lib::SharedRef<smd::ShaderMetaData> m_metaData;
};


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
	static constexpr smd::EBindingFlags GetBindingFlags() { return smd::EBindingFlags::None; }
	static constexpr Uint32 GetBindingIdxDelta() { return 1; }

	void SetOwningDSState(DescriptorSetState& state);

protected:

	void MarkAsDirty();

	// Helpers =========================================
	static constexpr lib::String BuildBindingVariableCode(lib::StringView bindingVariable, Uint32 bindingIdx)
	{
		SPT_CHECK(bindingIdx <= 9);
		const char bindingIdxChar = '0' + static_cast<char>(bindingIdx);
		return lib::String("[[vk::binding(") + bindingIdxChar + ", X)]] " + bindingVariable.data() + ";\n";
	}

private:

	lib::HashedString m_name;

	DescriptorSetState* m_owningState;
};


/**
 * Base class for all descriptor set states
 */
class RENDERER_CORE_API DescriptorSetState abstract
{
public:

	DescriptorSetState(const RendererResourceName& name, EDescriptorSetStateFlags flags);
	virtual ~DescriptorSetState() = default;

	virtual void UpdateDescriptors(DescriptorSetUpdateContext& context) const = 0;

	DSStateID GetID() const;

	Bool IsDirty() const;
	void SetDirty();
	void ClearDirtyFlag();

	EDescriptorSetStateFlags GetFlags() const;

	SizeType GetDescriptorSetHash() const;

	Uint32*								AddDynamicOffset();
	const lib::DynamicArray<Uint32>&	GetDynamicOffsets() const;
	SizeType							GetDynamicOffsetsNum() const;

	lib::DynamicArray<Uint32>			GetDynamicOffsetsForShader(const lib::DynamicArray<Uint32>& cachedOffsets, const smd::ShaderMetaData& shaderMetaData, Uint32 dsIdx) const;

	const lib::HashedString& GetName() const;

protected:

	void SetDescriptorSetHash(SizeType hash);

	void AddDynamicOffsetDefinition(Uint32 bindingIdx);
	void InitDynamicOffsetsArray();

private:

	const DSStateID	m_id;

	Bool m_isDirty;

	EDescriptorSetStateFlags m_flags;

	struct DynamicOffsetDef
	{
		Uint32 bindingIdx;
		SizeType offsetIdx;
	};

	lib::DynamicArray<Uint32>			m_dynamicOffsets;
	lib::DynamicArray<DynamicOffsetDef>	m_dynamicOffsetDefs;

	SizeType m_descriptorSetHash;

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
	className(const rdr::RendererResourceName& name, rdr::EDescriptorSetStateFlags flags = rdr::EDescriptorSetStateFlags::None)	\
		: Super(name, flags)																									\
	{																															\
		SetDescriptorSetHash(GetTypeHash());																					\
		rdr::bindings_refl::ForEachBindingWithDynamicOffset<ReflHeadBindingType>([this](Uint32 bindingIdx)						\
																				  {												\
																				      AddDynamicOffsetDefinition(bindingIdx);	\
																				  });											\
		InitDynamicOffsetsArray();																								\
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
	static SizeType GetTypeHash()																								\
	{																															\
		static lib::HashedString staticName(#className);																		\
		return staticName.GetKey();																								\
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

#define DS_END()	rdr::bindings_refl::HeadBindingUnderlyingType, "head"> ReflHeadBindingType;														\
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

template<typename TBindingHandle, typename TCallable>
void ForEachBindingWithDynamicOffset(TCallable callable)
{
	SPT_PROFILER_FUNCTION();

	ForEachBinding<TBindingHandle>([&callable, bindingIdx = Uint32(0)]<typename TCurrentBindingHandle>() mutable
	{
		using CurrentBindingType = typename TCurrentBindingHandle::BindingType;
	
		if (lib::HasAnyFlag(CurrentBindingType::GetBindingFlags(), smd::EBindingFlags::DynamicOffset))
		{
			callable(bindingIdx);
		}

		bindingIdx += CurrentBindingType::GetBindingIdxDelta();
	});
}

template<typename TBindingHandle>
void InitializeBindings(TBindingHandle& bindingHandle, DescriptorSetState& owningState)
{
	SPT_PROFILER_FUNCTION();

	ForEachBinding(bindingHandle,
				   [&owningState](auto& binding)
				   {
					   binding.SetOwningDSState(owningState);
					   binding.Initialize(owningState);
				   });
}

template<typename TBindingHandle>
constexpr lib::String BuildBindingsShaderCode(Uint32 bindingIdx = 0)
{
	lib::String result;

	ForEachBinding<TBindingHandle>([&result, bindingIdx = Uint32(0)]<typename TCurrentBindingHandle>() mutable
	{
		using CurrentBindingType = typename TCurrentBindingHandle::BindingType;
		result += CurrentBindingType::BuildBindingCode(TCurrentBindingHandle::GetName(), bindingIdx);
		bindingIdx += CurrentBindingType::GetBindingIdxDelta();
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
sc::DescriptorSetCompilationMetaData BuildCompilationMetaData()
{
	SPT_PROFILER_FUNCTION();

	sc::DescriptorSetCompilationMetaData metaData;

	using HeadBindingHandle = typename TDSType::ReflHeadBindingType;

	ForEachBinding<HeadBindingHandle>([&metaData]<typename TCurrentBindingHandle>()
	{
		using CurrentBindingType = typename TCurrentBindingHandle::BindingType;
		metaData.bindingsFlags.emplace_back(CurrentBindingType::GetBindingFlags());
	});

	metaData.hash = TDSType::GetTypeHash();

	return metaData;
}

} // bindings_refl


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

} // spt::rdr
