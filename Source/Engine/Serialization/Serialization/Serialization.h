#pragma once

#include "SculptorCoreTypes.h"
#include "SculptorLib/FileSystem/File.h"
#include "nlohmann/json.hpp"
#include "Utility/Base64.h"


namespace spt::srl
{

class Serializer;


namespace detail
{

template<typename TType>
concept CSerializableIntrusive = requires(Serializer& serializer, TType& data)
{
	{ data.Serialize(serializer) } -> std::same_as<void>;
};

template<typename TType>
concept CSerializableExternal = requires(Serializer& serializer, TType& data)
{
	{ SerializeType(serializer, data) } -> std::same_as<void>;
};

template<typename TType>
concept CSerializableStruct = CSerializableIntrusive<TType> || CSerializableExternal<TType>;

template<typename TType>
concept CSerializableEnum = std::is_enum_v<TType>;

} // detail


using JSON =  nlohmann::json;


class Serializer
{
public:

	static Serializer CreateWriter()
	{
		return Serializer();
	}

	static Serializer CreateReader(const lib::String& data)
	{
		return Serializer(data);
	}

	static Serializer CreateReader(lib::StringView data)
	{
		return Serializer(data);
	}

	static Serializer CreateReader(JSON j)
	{
		return Serializer(std::move(j));
	}

	Bool IsSaving() const { return m_isSaving; }
	Bool IsLoading() const { return !IsSaving(); }

	void Serialize(const char* name, Bool& value);
	void Serialize(const char* name, Int32& value);
	void Serialize(const char* name, Uint32& value);
	void Serialize(const char* name, SizeType& value);
	void Serialize(const char* name, Real32& value);

	void Serialize(const char* name, lib::String& value);
	void Serialize(const char* name, lib::HashedString& value);
	void Serialize(const char* name, lib::Path& value);
	void Serialize(const char* name, lib::RuntimeTypeInfo& value);

	template<detail::CSerializableStruct TDataType>
	void SerializeObject(TDataType& value)
	{
		if constexpr (detail::CSerializableIntrusive<TDataType>)
		{
			value.Serialize(*this);
		}
		else
		{
			SerializeType(*this, value);
		}
	}

	template<detail::CSerializableStruct TDataType>
	void Serialize(const char* name, TDataType& value)
	{
		if (IsSaving())
		{
			Serializer subWriter = Serializer::CreateWriter();
			subWriter.SerializeObject(value);
			m_json[name] = std::move(subWriter.GetJson());
		}
		else if (const JSON* subJson = FindJson(name))
		{
			Serializer subReader = Serializer::CreateReader(*subJson);
			subReader.SerializeObject(value);
		}
	}

	template<detail::CSerializableEnum TDataType>
	void Serialize(const char* name, TDataType& value)
	{
		using UnderlyingType = std::underlying_type_t<TDataType>;
		Serialize(name, reinterpret_cast<UnderlyingType&>(value));
	}

	template<typename TType, int Rows, int Cols>
	void Serialize(const char* name, math::Matrix<TType, Rows, Cols>& data)
	{
		if (IsSaving())
		{
			nlohmann::json jsonArray = nlohmann::json::array();
			for (int i = 0; i < Cols; ++i)
			{
				for (int j = 0; j < Rows; ++j)
				{
					jsonArray.push_back(data(j, i));
				}
			}
			m_json[name] = jsonArray;
		}
		else if (const JSON* jsonArray = FindJson(name))
		{
			if (jsonArray->size() == Cols * Rows)
			{
				for (int i = 0; i < Cols; ++i)
				{
					for (int j = 0; j < Rows; ++j)
					{
						data(j, i) = (*jsonArray)[i * Rows + j].get<TType>();
					}
				}
			}
			else
			{
				data = {};
			}
		}
	}

	template<typename TDataType, SizeType N>
	void Serialize(const char* name, lib::InlineDynamicArray<TDataType, N>& dataArray)
	{
		if (IsSaving())
		{
			JSON jsonArray = JSON::array();

			for (TDataType& item : dataArray)
			{
				jsonArray.push_back(ToJSON(item));
			}

			m_json[name] = std::move(jsonArray);
		}
		else if (const JSON* jsonArray = FindJson(name))
		{
			dataArray.Clear();

			for (const auto& itemJson : *jsonArray)
			{
				dataArray.EmplaceBack(FromJSON<TDataType>(itemJson));
			}
		}
	}

	template<typename TDataType, SizeType N>
	void Serialize(const char* name, lib::StaticArray<TDataType, N>& dataArray)
	{
		if (IsSaving())
		{
			JSON jsonArray = JSON::array();

			for (TDataType& item : dataArray)
			{
				jsonArray.push_back(ToJSON(item));
			}

			m_json[name] = std::move(jsonArray);
		}
		else if (const JSON* jsonArray = FindJson(name))
		{
			for (SizeType i = 0; i < N && i < jsonArray->size(); ++i)
			{
				dataArray[i] = FromJSON<TDataType>((*jsonArray)[i]);
			}
		}
	}

	template<typename TDataType>
	void Serialize(const char* name, lib::DynamicArray<TDataType>& dataArray)
	{
		if (IsSaving())
		{
			JSON jsonArray = JSON::array();

			for (TDataType& item : dataArray)
			{
				jsonArray.push_back(ToJSON(item));
			}

			m_json[name] = std::move(jsonArray);
		}
		else if (const JSON* jsonArray = FindJson(name))
		{
			dataArray.clear();
			dataArray.reserve(jsonArray->size());

			for (const auto& itemJson : *jsonArray)
			{
				dataArray.emplace_back(FromJSON<TDataType>(itemJson));
			}
		}
	}

	template<>
	void Serialize(const char* name, lib::DynamicArray<Byte>& dataArray)
	{
		if (IsSaving())
		{
			m_json[name] = lib::EncodeBase64(dataArray);
		}
		else if (const JSON* json = FindJson(name))
		{
			dataArray = lib::DecodeBase64(json->get_ref<const lib::String&>());
		}
	}

	template<typename TKeyType, typename TDataType>
	void Serialize(const char* name, lib::HashMap<TKeyType, TDataType>& dataMap)
	{
		if (IsSaving())
		{
			JSON jsonArray = JSON::array();

			for (auto& [key, value] : dataMap)
			{
				JSON elem = JSON::array();

				elem.emplace_back(ToJSON<TKeyType>(const_cast<TKeyType&>(key)));
				elem.emplace_back(ToJSON<TDataType>(value));

				jsonArray.push_back(elem);
			}

			m_json[name] = std::move(jsonArray);
		}
		else if (const JSON* jsonArray = FindJson(name))
		{
			dataMap.clear();
			dataMap.reserve(jsonArray->size());

			for (const auto& itemJson : *jsonArray)
			{
				TKeyType key = FromJSON<TKeyType>(itemJson[0]);
				TDataType value = FromJSON<TDataType>(itemJson[1]);

				dataMap.emplace(std::move(key), std::move(value));
			}
		}
	}

	lib::String ToCompactString() const
	{
		return m_json.dump();
	}

	lib::String ToString() const
	{
		return m_json.dump(4);
	}

protected:

	explicit Serializer()
		: m_isSaving(true)
	{ }

	explicit Serializer(const lib::String& data)
		: m_json(JSON::parse(data))
		, m_isSaving(false)
	{
	}

	explicit Serializer(lib::StringView data)
		: m_json(JSON::parse(data.cbegin(), data.cend()))
		, m_isSaving(false)
	{
	}

	explicit Serializer(JSON j)
		: m_json(std::move(j))
		, m_isSaving(false)
	{
	}

	JSON& GetJson()
	{
		return m_json;
	}

	const JSON* FindJson(const char* name) const
	{
		if (!name)
		{
			return &m_json;
		}

		const auto it = m_json.find(name);
		return it != m_json.end() ? &*it : nullptr;
	}

	template<typename TDataType>
	JSON ToJSON(const TDataType& data)
	{
		Serializer writer = Serializer::CreateWriter();

		if constexpr (detail::CSerializableStruct<TDataType>)
		{
			writer.SerializeObject(const_cast<TDataType&>(data));
		}
		else
		{
			writer.Serialize(nullptr, const_cast<TDataType&>(data));
		}

		return std::move(writer.GetJson());
	}

	template<typename TDataType>
	TDataType FromJSON(const JSON& json)
	{
		TDataType data;
		Serializer reader = Serializer::CreateReader(json);

		if constexpr (detail::CSerializableStruct<TDataType>)
		{
			reader.SerializeObject(data);
		}
		else
		{
			reader.Serialize(nullptr, data);
		}

		return data;
	}

	template<typename TData>
	void SetImpl(const char* name, const TData& data)
	{
		if (name)
		{
			m_json[name] = data;
		}
		else
		{
			m_json = data;
		}
	}

	template<typename TData>
	void GetImpl(const char* name, TData& data) const
	{
		if (const JSON* json = FindJson(name))
		{
			json->get_to(data);
		}
	}

	JSON m_json;

	Bool m_isSaving = true;
};

} // spt::srl
