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
	requires std::is_same_v<TType, lib::SharedPtr<rdr::BindableBufferView>> || std::is_same_v<TType, rg::RGBufferViewHandle>;
};


class BoundBufferVariant
{
public:
	
	BoundBufferVariant() = default;
	
	const lib::SharedPtr<rdr::BindableBufferView>& GetBufferToBind() const
	{
		return std::visit(lib::Overload
						  {
							  [](const lib::SharedPtr<rdr::BindableBufferView>& buffer) -> const lib::SharedPtr<rdr::BindableBufferView>&
							  {
							      return buffer;
							  },
							  [](const rg::RGBufferViewHandle buffer) -> const lib::SharedPtr<rdr::BindableBufferView>&
							  {
								  return buffer->GetResource(false);
							  }
						  },
						  m_buffer);
	}

	void AddRGDependency(rg::RGDependenciesBuilder& builder, const rg::RGBufferAccessInfo& access) const
	{
		std::visit([&builder, access](const auto& buffer)
				   {
					   builder.AddBufferAccess(buffer, access);
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

	Bool operator==(const lib::SharedPtr<rdr::BindableBufferView>& rhs) const
	{
		return std::visit(lib::Overload
						  {
							  [rhs](const lib::SharedPtr<rdr::BindableBufferView>& buffer)
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
							  [](const lib::SharedPtr<rdr::BindableBufferView>& buffer)
							  {
								  return buffer.get() != nullptr;
							  },
							  [](const rg::RGBufferViewHandle buffer)
							  {
							  	return buffer.IsValid();
							  }
						  },
						  m_buffer);
	}

	Uint64 GetBoundBufferSize() const
	{
		return std::visit(lib::Overload
						  {
							  [](const lib::SharedPtr<rdr::BindableBufferView>& buffer) -> Uint64
							  {
								  return buffer ? buffer->GetSize() : 0u;
							  },
							  [](const rg::RGBufferViewHandle buffer) -> Uint64
							  {
							  	return buffer.IsValid() ? buffer->GetSize() : 0u;
							  }
						  },
						  m_buffer);
	}

	lib::SharedPtr<rdr::BindableBufferView> GetBoundBuffer() const
	{
		SPT_CHECK(IsValid());
		SPT_CHECK(std::holds_alternative<lib::SharedPtr<rdr::BindableBufferView>>(m_buffer));

		return std::get<lib::SharedPtr<rdr::BindableBufferView>>(m_buffer);
	}

	rg::RGBufferViewHandle GetBoundRGBuffer() const
	{
		SPT_CHECK(IsValid());
		SPT_CHECK(std::holds_alternative<rg::RGBufferViewHandle>(m_buffer));

		return std::get<rg::RGBufferViewHandle>(m_buffer);
	}

private:

	using VariantType = std::variant<rg::RGBufferViewHandle,
									 lib::SharedPtr<rdr::BindableBufferView>>;

	VariantType m_buffer;
};

} // priv

} // spt::gfx
