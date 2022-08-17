#pragma once

#include "SculptorCoreTypes.h"
#include "YAML.h"


namespace spt::srl
{

class YAMLWriter
{
public:

	explicit YAMLWriter(YAML::Node& node)
		: m_node(node)
	{ }

	static constexpr Bool IsWriting()
	{
		return true;
	}

	template<typename TDataType>
	void Serialize(const TDataType& data) const
	{
		m_node.push_back(data);
	}

	template<typename TDataType>
	void Serialize(lib::StringView key, const TDataType& data) const
	{
		m_node[key.data()] = data;
	}

private:

	YAML::Node& m_node;
};


class YAMLLoader
{
public:

	explicit YAMLLoader(const YAML::Node& node)
		: m_node(node)
		, m_currentIdx(0)
	{ }

	static constexpr Bool IsWriting()
	{
		return false;
	}

	template<typename TDataType>
	void Serialize(TDataType& data)
	{
		data = m_node[m_currentIdx++].as<TDataType>();
	}

	template<typename TDataType>
	void Serialize(lib::StringView key, TDataType& data)
	{
		data = m_node[key.data()].as<TDataType>();
	}

private:

	const YAML::Node&	m_node;
	SizeType			m_currentIdx;
};


class YAMLEmitter
{
public:

	explicit YAMLEmitter(YAML::Emitter& emitter)
		: m_emitter(emitter)
		, m_emittingSequence(false)
	{ }

	static constexpr Bool IsWriting()
	{
		return true;
	}

	template<typename TDataType>
	void Serialize(const TDataType& data)
	{
		if (!m_emittingSequence)
		{
			m_emitter << YAML::BeginSeq;
			m_emittingSequence = true;
		}

		m_emitter << data;
	}

	template<typename TDataType>
	void Serialize(lib::StringView key, const TDataType& data)
	{
		if (m_emittingSequence)
		{
			m_emitter << YAML::EndSeq;
			m_emittingSequence = false;
		}

		m_emitter << YAML::Key << key.data() << YAML::Value << data;
	}

	void EndEmitting() const
	{
		if (m_emittingSequence)
		{
			m_emitter << YAML::EndSeq;
		}
	}

private:

	YAML::Emitter&	m_emitter;
	Bool			m_emittingSequence;
};


template<typename TSerializer>
class SerializerWrapper
{
public:

	template<typename... TArgs>
	SerializerWrapper(TArgs&&... arguments)
		: m_serializer(std::forward<TArgs>(arguments)...)
	{ }

	template<typename TDataType>
	void Serialize(const TDataType& data)
	{
		m_serializer.Serialize(data);
	}

	template<typename TDataType>
	void Serialize(lib::StringView key, const TDataType& data)
	{
		m_serializer.Serialize(key, data);
	}

	template<typename TDataType>
	void Serialize(TDataType& data)
	{
		m_serializer.Serialize(data);
	}

	template<typename TDataType>
	void Serialize(lib::StringView key, TDataType& data)
	{
		m_serializer.Serialize(key, data);
	}

	TSerializer& Get()
	{
		return m_serializer;
	}

private:

	TSerializer m_serializer;
};


class YAMLSerializeHelper
{
public:

	template<typename Type>
	static YAML::Node Write(const Type& data)
	{
		YAML::Node node;
		SerializerWrapper<YAMLWriter> serializer(node);
		TypeSerializer<Type>::Serialize(serializer, data);
		return node;
	}

	template<typename Type>
	static bool Load(const YAML::Node& node, Type& data)
	{
		SerializerWrapper<YAMLLoader> serializer(node);
		TypeSerializer<Type>::Serialize(serializer, data);
		return true;
	}

	template<typename Type>
	static void Emit(YAML::Emitter& emitter, const Type& data)
	{
		SerializerWrapper<YAMLEmitter> serializer(emitter);
		TypeSerializer<Type>::Serialize(serializer, data);
		serializer.Get().EndEmitting();
	}
};


template<typename Type>
struct TypeSerializer
{
	// Param is always Type, but may be const or not
	template<typename Serializer, typename Param>
	static void Serialize(SerializerWrapper<Serializer>& serializer, Param& data)
	{
		// ...
	}
};


template<typename SerializedType, typename Serializer, typename Param>
void SerializeType(SerializerWrapper<Serializer>& serializer, Param& data)
{
	if constexpr (std::is_const_v<Param>)
	{
		TypeSerializer<const SerializedType>::Serialize(serializer, data);
	}
	else
	{
		TypeSerializer<std::remove_const_t<SerializedType>>::Serialize(serializer, data);
	}
}

} // spt::srl

#define SPT_YAML_SERIALIZATION_TEMPLATES(type)										\
namespace YAML																		\
{																					\
inline Emitter& operator<<(Emitter& emitter, const type& data)						\
{																					\
	spt::srl::YAMLSerializeHelper::Emit(emitter, data);								\
	return emitter;																	\
}																					\
																					\
template<>																			\
struct convert<type>																\
{																					\
	static Node encode(const type& data)											\
	{																				\
		return spt::srl::YAMLSerializeHelper::Write(data);							\
	}																				\
																					\
	static bool decode(const Node& node, type& data)								\
	{																				\
		return spt::srl::YAMLSerializeHelper::Load(node, data);						\
	}																				\
};																					\
} 																					\
