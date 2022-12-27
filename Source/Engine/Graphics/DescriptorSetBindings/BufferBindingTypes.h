#pragma once

#include "Types/Buffer.h"
#include "DependenciesBuilder.h"
#include "Utility/Templates/Overload.h"
#include "Utility/Templates/TypeTraits.h"


namespace spt::gfx
{

template<typename TType>
concept CInstanceOrRGBufferView = requires
{
	requires std::is_same_v<TType, lib::SharedPtr<rdr::BufferView>> || std::is_same_v<TType, rg::RGBufferViewHandle>;
};

namespace priv
{

class BoundBufferVariant
{
public:
	
	BoundBufferVariant() = default;
	
	lib::SharedRef<rdr::BufferView> GetBufferToBind() const
	{
		return std::visit(lib::Overload
						  {
							  [](const lib::SharedPtr<rdr::BufferView>& buffer)
							  {
								  SPT_CHECK(!!buffer)
							      return lib::Ref(buffer);
							  },
							  [](const rg::RGBufferViewHandle buffer)
							  {
								  SPT_CHECK(buffer.IsValid());
								  return buffer->GetBufferViewInstance();
							  }
						  },
						  m_buffer);
	}

	void AddRGDependency(rg::RGDependenciesBuilder& builder, rg::ERGBufferAccess access, rhi::EShaderStageFlags shaderStages) const
	{
		std::visit(lib::Overload
				   {
				       [&builder, access, shaderStages](const lib::SharedPtr<rdr::BufferView>& buffer)
				       {
				           builder.AddBufferAccess(lib::Ref(buffer), access, shaderStages);
				       },
				       [&builder, access, shaderStages](rg::RGBufferViewHandle buffer)
				       {
				           builder.AddBufferAccess(buffer, access, shaderStages);
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

	Bool operator==(const lib::SharedPtr<rdr::BufferView>& rhs) const
	{
		return std::visit(lib::Overload
						  {
							  [rhs](const lib::SharedPtr<rdr::BufferView>& buffer)
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
							  [](const lib::SharedPtr<rdr::BufferView>& buffer)
							  {
							      return !!buffer;
							  },
							  [](const rg::RGBufferViewHandle buffer)
							  {
							  	return buffer.IsValid();
							  }
						  },
						  m_buffer);
	}

private:

	using VariantType = std::variant<lib::SharedPtr<rdr::BufferView>,
									 rg::RGBufferViewHandle>;

	VariantType m_buffer;
};

} // priv

} // spt::gfx
