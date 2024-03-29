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
	explicit VulkanStructsLinkedList(TStruct& structure)
		: m_next(&structure.pNext)
	{ }

	template<typename TStruct>
	void Append(TStruct& structure)
	{
		if (m_next)
		{
			*m_next = &structure;
		}


		m_next = (const void**)&structure.pNext;
	}

private:

	const void** m_next;
};

} // spt::vulkan
