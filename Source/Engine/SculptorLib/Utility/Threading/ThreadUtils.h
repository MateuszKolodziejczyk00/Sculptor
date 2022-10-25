#pragma once

#include <new>


namespace spt::lib
{

struct InterferenceProps
{
	// Minimum offset between two objects to avoid false sharing
	static constexpr SizeType destructiveInterferenceSize	= std::hardware_destructive_interference_size;

	// Maximum size of contiguous memory to promote true sharing.
	static constexpr SizeType constructiveInterferenceSize	= std::hardware_constructive_interference_size;
};

} // spt::lib


#define ALIGNAS_CACHE_LINE alignas(spt::lib::InterferenceProps::destructiveInterferenceSize)