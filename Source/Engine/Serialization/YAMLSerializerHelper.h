#pragma once

#include "SculptorCoreTypes.h"
#include "YAML.h"
#include "FileSystem/File.h"


namespace spt::srl
{

using Binary = YAML::Binary;

enum class ESerializationFlags : Flags32
{
	None			= 0,
	Flow			= BIT(1)
};


class YAMLWriter
{
public:

	explicit YAMLWriter(YAML::Node& node)
		: m_node(node)
	{ }

	static constexpr Bool IsSaving()
	{
		return true;
	}

	static constexpr Bool IsLoading()
	{
		return !IsSaving();
	}

	template<typename TDataType>
	Bool Serialize(const TDataType& data, ESerializationFlags flags = ESerializationFlags::None) const
	{
		m_node.push_back(data);
		return true;
	}

	template<typename TDataType>
	Bool Serialize(lib::StringView key, const TDataType& data, ESerializationFlags flags = ESerializationFlags::None) const
	{
		m_node[key.data()] = data;
		return true;
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

	static constexpr Bool IsSaving()
	{
		return false;
	}

	static constexpr Bool IsLoading()
	{
		return !IsSaving();
	}

	Bool DoesKeyExist(lib::StringView key) const
	{
		return m_node[key.data()].IsDefined();
	}

	template<typename TDataType>
	Bool Serialize(TDataType& data, ESerializationFlags flags = ESerializationFlags::None)
	{
		const YAML::Node child = m_node[m_currentIdx++];
		if (child.IsDefined())
		{
			data = child.as<TDataType>();
			return true;
		}

		return false;
	}

	template<typename TDataType>
	Bool Serialize(lib::StringView key, TDataType& data, ESerializationFlags flags = ESerializationFlags::None)
	{
		const YAML::Node child = m_node[key.data()];
		if (child.IsDefined())
		{
			data = child.as<TDataType>();
			return true;
		}

		return false;
	}

	const YAML::Node& GetNode() const
	{
		return m_node;
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

	static constexpr Bool IsSaving()
	{
		return true;
	}

	static constexpr Bool IsLoading()
	{
		return !IsSaving();
	}

	template<typename TDataType>
	void Serialize(const TDataType& data, ESerializationFlags flags = ESerializationFlags::None)
	{
		if (!m_emittingSequence)
		{
			if (lib::HasAnyFlag(flags, ESerializationFlags::Flow))
			{
				m_emitter << YAML::Flow;
			}

			m_emitter << YAML::BeginSeq;
			m_emittingSequence = true;
		}

		m_emitter << data;
	}

	template<typename TDataType>
	void Serialize(lib::StringView key, const TDataType& data, ESerializationFlags flags = ESerializationFlags::None)
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

	void BeginMap()
	{
		m_emitter << YAML::BeginMap;
	}

	void EndMap()
	{
		m_emitter << YAML::EndMap;
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

	Bool DoesKeyExist(lib::StringView key) const
	{
		return m_serializer.DoesKeyExist(key);
	}

	template<typename TDataType>
	std::enable_if_t<!lib::isPair<TDataType>, void> Serialize(const TDataType& data)
	{
		m_serializer.Serialize(data);
	}

	template<typename TDataType>
	std::enable_if_t<lib::isPair<TDataType>, void> Serialize(const TDataType& data)
	{
		m_serializer.Serialize(data.first, data.second);
	}

	template<typename TDataType>
	std::enable_if_t<!lib::isPair<TDataType>, void> Serialize(TDataType& data)
	{
		m_serializer.Serialize(data);
	}

	template<typename TDataType>
	std::enable_if_t<lib::isPair<TDataType>, void> Serialize(TDataType& data)
	{
		m_serializer.Serialize(data.first, data.second);
	}

	template<typename TDataType>
	void Serialize(lib::StringView key, const TDataType& data)
	{
		m_serializer.Serialize(key, data);
	}

	template<typename TDataType>
	void Serialize(lib::StringView key, TDataType& data)
	{
		m_serializer.Serialize(key, data);
	}

	// Fix bugged chars serialization
	template<>
	void Serialize<Uint8>(lib::StringView key, Uint8& data)
	{
		Uint32 val = 0;
		m_serializer.Serialize(key, val);
		data = static_cast<Uint8>(val);
	}

	// Fix bugged chars serialization
	template<>
	void Serialize<Uint8>(lib::StringView key, const Uint8& data)
	{
		Uint32 val = static_cast<Uint32>(data);
		m_serializer.Serialize(key, val);
	}

	template<>
	void Serialize<lib::Path>(lib::StringView key, lib::Path& data)
	{
		lib::String val;
		m_serializer.Serialize(key, val);
		data = lib::Path(val);
	}

	template<>
	void Serialize<lib::Path>(lib::StringView key, const lib::Path& data)
	{
		lib::String val = data.generic_string();
		m_serializer.Serialize(key, val);
	}

	template<typename TEnumType>
	void SerializeEnum(lib::StringView key, TEnumType& data)
	{
		static_assert(std::is_enum_v<TEnumType>, "Type must be an enum");

		using UnderlyingType = std::underlying_type_t<TEnumType>;
		
		UnderlyingType asUnderlying = idxNone<UnderlyingType>;
		m_serializer.Serialize(key, asUnderlying);

		data = static_cast<TEnumType>(asUnderlying);
	}

	template<typename TEnumType>
	void SerializeEnum(lib::StringView key, const TEnumType& data)
	{
		static_assert(std::is_enum_v<TEnumType>, "Type must be an enum");

		using UnderlyingType = std::underlying_type_t<TEnumType>;
		
		const UnderlyingType asUnderlying = static_cast<UnderlyingType>(data);
		m_serializer.Serialize(key, asUnderlying);
	}

	template<typename TType, int Rows, int Cols>
	void Serialize(lib::StringView key, math::Matrix<TType, Rows, Cols>& data)
	{
		std::vector<TType> asArray(Rows * Cols);

		if (m_serializer.Serialize(key, asArray))
		{
			for (int i = 0; i < Cols; ++i)
			{
				for (int j = 0; j < Rows; ++j)
				{
					data(j, i) = asArray[i * Rows + j];
				}
			}
		}
	}

	template<typename TType, int Rows, int Cols>
	void Serialize(lib::StringView key, const math::Matrix<TType, Rows, Cols>& data)
	{
		std::vector<TType> asArray(Rows * Cols);
		for (int i = 0; i < Cols; ++i)
		{
			for (int j = 0; j < Rows; ++j)
			{
				asArray[i * Rows + j] = data(j, i);
			}
		}

		m_serializer.Serialize(key, asArray);
	}

	TSerializer& Get()
	{
		return m_serializer;
	}

private:

	TSerializer m_serializer;
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
	TypeSerializer<SerializedType>::Serialize(serializer, data);
}


class YAMLSerializeHelper
{
public:

	template<typename Type>
	static YAML::Node Write(const Type& data)
	{
		YAML::Node node;
		SerializerWrapper<YAMLWriter> serializer(node);
		SerializeType<Type>(serializer, data);
		return node;
	}

	template<typename Type>
	static Bool Load(const YAML::Node& node, Type& data)
	{
		SerializerWrapper<YAMLLoader> serializer(node);
		SerializeType<Type>(serializer, data);
		return true;
	}

	template<typename Type>
	static void Emit(YAML::Emitter& emitter, const Type& data)
	{
		emitter << YAML::BeginMap;
		SerializerWrapper<YAMLEmitter> serializer(emitter);
		SerializeType<Type>(serializer, data);
		serializer.Get().EndEmitting();
		emitter << YAML::EndMap;
	}
};

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
}
