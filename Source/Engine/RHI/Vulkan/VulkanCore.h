#pragma once

#include "SculptorAliases.h"
#include "Assertions/Assertions.h"


#pragma warning(push)
#pragma warning(disable: 4100)
#pragma warning(disable: 4127)
#pragma warning(disable: 4189)
#pragma warning(disable: 4459)
#pragma warning(disable: 4324)


#define SPT_VMA_DEBUG 0

#if SPT_VMA_DEBUG

#define VMA_DEBUG_LOG_FORMAT(format, ...) printf(format, __VA_ARGS__)

#endif // SPT_VMA_DEBUG

#include "volk.h"
#include "vk_mem_alloc.h"
#include "VulkanNames.h"
#include "VulkanMacros.h"
#include "VulkanAliases.h"

#pragma warning(pop)


namespace spt::vulkan
{

template<typename TType>
class RHIResourceReleaseTicket
{
public:

	RHIResourceReleaseTicket() = default;
	RHIResourceReleaseTicket(TType val)
		: m_val(val)
	{ }

	RHIResourceReleaseTicket(const RHIResourceReleaseTicket& rhs) = delete;
	RHIResourceReleaseTicket& operator=(const RHIResourceReleaseTicket& rhs) = delete;

	RHIResourceReleaseTicket(RHIResourceReleaseTicket&& rhs)
		: m_val(rhs.m_val)
	{
		rhs.Reset();
	}

	RHIResourceReleaseTicket& operator=(TType rhs)
	{
		SPT_CHECK(!IsValid());

		m_val = std::move(rhs);

		return *this;
	}

	RHIResourceReleaseTicket& operator=(RHIResourceReleaseTicket&& rhs)
	{
		SPT_CHECK(!IsValid());

		m_val = rhs.m_val;
		rhs.Reset();

		return *this;
	}

	~RHIResourceReleaseTicket()
	{
		SPT_CHECK(!IsValid());
	}

	void Reset()
	{
		m_val = VK_NULL_HANDLE;
	}

	Bool IsValid() const
	{
		return m_val != VK_NULL_HANDLE;
	}

	const TType& GetValue() const
	{
		return m_val;
	}

private:

	TType m_val = VK_NULL_HANDLE;
};

} // spt::vulkan
