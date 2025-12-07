#pragma once

#include "SculptorCoreTypes.h"
#include "SculptorLib/FileSystem/File.h"
#include "nlohmann/json.hpp"
#include "Utility/Base64.h"

#pragma optimize("", off)
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
		return Serializer(j);
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

	template<detail::CSerializableIntrusive TDataType>
	void Serialize(const char* name, TDataType& value)
	{
		if (IsSaving())
		{
			Serializer subWriter = Serializer::CreateWriter();
			value.Serialize(subWriter);
			m_json[name] = subWriter.GetJson();
		}
		else
		{
			JSON subJson = m_json[name];
			Serializer subReader = Serializer::CreateReader(subJson);
			value.Serialize(subReader);
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
		else
		{
			JSON jsonArray = m_json[name];
			for (int i = 0; i < Cols; ++i)
			{
				for (int j = 0; j < Rows; ++j)
				{
					data(j, i) = jsonArray[i * Rows + j].get<TType>();
				}
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

			m_json[name] = jsonArray;
		}
		else
		{
			const JSON jsonArray = m_json[name];
			dataArray.clear();
			dataArray.reserve(jsonArray.size());

			for (const auto& itemJson : jsonArray)
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
		else
		{
			dataArray = lib::DecodeBase64(m_json[name].get<lib::String>());
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

			m_json[name] = jsonArray;
		}
		else
		{
			const JSON jsonArray = m_json[name];

			dataMap.clear();
			dataMap.reserve(jsonArray.size());

			for (const auto& itemJson : jsonArray)
			{
				JSON keyJson   = itemJson[0];
				JSON valueJson = itemJson[1];

				TKeyType key = FromJSON<TKeyType>(keyJson);
				TDataType value = FromJSON<TDataType>(valueJson);

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
		: m_json(j)
		, m_isSaving(false)
	{
	}

	JSON& GetJson()
	{
		return m_json;
	}

	template<typename TDataType>
	JSON ToJSON(const TDataType& data)
	{
		Serializer writer = Serializer::CreateWriter();

		if constexpr (detail::CSerializableIntrusive<TDataType>)
		{
			const_cast<TDataType&>(data).Serialize(writer);
		}
		else
		{
			writer.Serialize(nullptr, const_cast<TDataType&>(data));
		}

		return writer.GetJson();
	}

	template<typename TDataType>
	TDataType FromJSON(const JSON& json)
	{
		TDataType data;
		Serializer reader = Serializer::CreateReader(json);

		if constexpr (detail::CSerializableIntrusive<TDataType>)
		{
			data.Serialize(reader);
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
	TData GetImpl(const char* name) const
	{
		if (name)
		{
			return m_json[name].get<TData>();
		}
		else
		{
			return m_json.get<TData>();
		}
	}

	JSON m_json;

	Bool m_isSaving = true;
};

} // spt::srl
