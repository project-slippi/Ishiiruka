// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.
// Added for Ishiiruka By Tino

#pragma once
#include <vector>
#include "CommonTypes.h"
#include "BitHelpers.h"

namespace Common
{
// Circular allocation map to reduce cpu usage
// while controling resource usage in a circular buffer
class AllocationMap
{
	static const size_t bit_count = sizeof(size_t) * 8;
	static const size_t bit_mask = bit_count - 1;
	std::vector<size_t> m_slot_groups;
	size_t m_current_slot = size_t(-1LL);
	size_t m_slot_count = 0;
	size_t m_group_count = 0;
public:
	AllocationMap(size_t size)
		: m_slot_groups((size / bit_count) + ((size & bit_mask) != 0)),
		m_slot_count(size)
	{
		m_group_count = m_slot_groups.size();
		std::fill(m_slot_groups.begin(), m_slot_groups.end(), size_t(-1LL));
	}

	int AllocateSlot()
	{
		// Go to the next valid slot
		m_current_slot++;
		bool restarted = false;
		if (m_current_slot >= m_slot_count)
		{
			// If we are at the end go back to the start
			// to search for free slots
			m_current_slot = 0;
			restarted = true;
		}

		size_t group = m_current_slot / bit_count;
		int bit = static_cast<int>(m_current_slot & bit_mask);
		while (group < m_group_count)
		{
			// work only with the bits after the current one
			size_t bs = m_slot_groups[group] >> bit;
			if (bs)
			{
				// Group with available slots
				// Search for the first available slot
				// add the first available bit to the current to get the real bit
				bit += LeastSignificantSetBit(bs);
				// Calculate the final slot id
				m_current_slot = group * bit_count + bit;
				// Disable the used slot
				m_slot_groups[group] &= (~(size_t(1) << bit));
				return static_cast<int>(m_current_slot);
			}
			// no free slots in the current group
			bit = 0;
			++group;
			if (group >= m_group_count && !restarted)
			{
				// if we reached the end and is the first try retry from the start
				restarted = true;
				group = m_current_slot = 0;
			}
		}
		// No free slot found return invalid one
		return -1;
	}

	void ReleaseSlot(int index)
	{
		size_t group = index / bit_count;
		size_t bit = index & bit_mask;
		size_t& bs = m_slot_groups[group];
		bs = bs | (size_t(1) << bit);
	}

};
}
