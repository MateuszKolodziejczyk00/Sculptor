#pragma once

#include "Types/Buffer.h"
#include "DependenciesBuilder.h"
#include "Utility/Templates/Overload.h"
#include "Utility/Templates/TypeTraits.h"


namespace spt::gfx
{

namespace priv
{

template<typename TType>
concept CInstanceOrRGBufferView = requires
{
	requires std::is_same_v<TType, rdr::BufferView> || std::is_same_v<TType, rg::RGBufferViewHandle>;
};


class BoundBufferVariant
{
public:
	
	BoundBufferVariant() = default;
	
	const rdr::BufferView& GetBufferToBind() const
	{
		return std::visit(lib::Overload
						  {
							  [](const rdr::BufferView& buffer) -> const rdr::BufferView&
							  {
							      return buffer;
							  },
							  [](const rg::RGBufferViewHandle buffer) -> const rdr::BufferView&
							  {
								  return buffer->GetBufferViewInstance(false);
							  }
						  },
						  m_buffer);
	}

	void AddRGDependency(rg::RGDependenciesBuilder& builder, rg::ERGBufferAccess access) const
	{
		std::visit(lib::Overload
				   {
				       [&builder, access](const rdr::BufferView& buffer)
				       {
				           builder.AddBufferAccess(buffer, access);
				       },
				       [&builder, access](rg::RGBufferViewHandle buffer)
				       {
				           builder.AddBufferAccess(buffer, access);
				       }
				   },
				   m_buffer);
	}

	template<CInstanceOrRGBufferView TType>
	void Set(lib::AsConstParameter<TType> buffer)
	{
		m_buffer = buffer;
	}

	void Reset()
	{
		m_buffer = rg::RGBufferViewHandle{};
	}

	Bool operator==(const rdr::BufferView& rhs) const
	{
		return std::visit(lib::Overload
						  {
							  [rhs](const rdr::BufferView& buffer)
							  {
								  return buffer == rhs;
							  },
							  [](const auto& buffer)
							  {
								  return false;
							  }
						  },
						  m_buffer);
	}
	
	Bool operator==(rg::RGBufferViewHandle rhs) const
	{
		return std::visit(lib::Overload
						  {
							  [rhs](rg::RGBufferViewHandle buffer)
							  {
								  return buffer == rhs;
							  },
							  [](const auto& buffer)
							  {
								  return false;
							  }
						  },
						  m_buffer);
	}

	template<CInstanceOrRGBufferView TType>
	lib::AsConstParameter<TType> GetAs() const
	{
		return std::get<TType>(m_buffer);
	}

	Bool IsValid() const
	{
		return std::visit(lib::Overload
						  {
							  [](const rdr::BufferView& buffer)
							  {
								  return true;
							  },
							  [](const rg::RGBufferViewHandle buffer)
							  {
							  	return buffer.IsValid();
							  }
						  },
						  m_buffer);
	}

private:

	using VariantType = std::variant<rg::RGBufferViewHandle,
									 rdr::BufferView>;

	VariantType m_buffer;
};

} // priv

} // spt::gfx
