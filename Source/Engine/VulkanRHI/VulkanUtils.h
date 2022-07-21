#pragma once


namespace spt::vulkan
{

class VulkanStructsLinkedList
{
public:

	VulkanStructsLinkedList()
		: m_next(nullptr)
	{ }

	template<typename TStruct>
	VulkanStructsLinkedList(TStruct& structure)
		: m_next(&structure.pNext)
	{ }

	template<typename TStruct>
	void Append(TStruct& structure)
	{
		if (m_next)
		{
			*m_next = &structure;
		}

		m_next = &static_cast<const void*>(structure.pNext);
	}

private:

	const void** m_next;
};

}
