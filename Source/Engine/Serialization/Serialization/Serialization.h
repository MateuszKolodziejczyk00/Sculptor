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

	static Serializer CreateReader(JSON j)
	{
		return Serializer(j);
	}

	Bool IsSaving() const { return m_isSaving; }
	Bool IsLoading() const { return !IsSaving(); }

	void Serialize(const char* name, Bool& value);
	void Serialize(const char* name, Int32& value);
	void Serialize(const char* name, Uint32& value);
	void Serialize(const char* name, Real32& value);

	void Serialize(const char* name, lib::String& value);
	void Serialize(const char* name, lib::HashedString& value);
	void Serialize(const char* name, lib::Path& value);

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

			for (const TDataType& item : dataArray)
			{
				Serializer itemSerializer = Serializer::CreateWriter();
				itemSerializer.Serialize("Elem", const_cast<TDataType&>(item));
				jsonArray.push_back(itemSerializer.GetJson());
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
				TDataType item;
				Serializer itemSerializer = Serializer::CreateReader(itemJson);
				itemSerializer.Serialize("Elem", item);
				dataArray.emplace_back(std::move(item));
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

			for (const auto& [key, value] : dataMap)
			{
				Serializer itemSerializer = Serializer::CreateWriter();
				itemSerializer.Serialize("Key",   const_cast<TKeyType&>(key));
				itemSerializer.Serialize("Value", const_cast<TDataType&>(value));

				jsonArray.push_back(itemSerializer.GetJson());
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
				TKeyType key;
				TDataType value;
				Serializer itemSerializer = Serializer::CreateReader(itemJson);
				itemSerializer.Serialize("Key",   key);
				itemSerializer.Serialize("Value", value);
				dataMap.emplace(std::move(key), std::move(value));
			}
		}
	}

	lib::String ToString() const
	{
		return m_json.dump();
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

	explicit Serializer(JSON j)
		: m_json(j)
		, m_isSaving(false)
	{
	}

	JSON& GetJson()
	{
		return m_json;
	}

	JSON m_json;

	Bool m_isSaving = true;
};

} // spt::srl
