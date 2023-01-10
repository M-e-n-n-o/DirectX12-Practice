#include "dxpch.h"
#include "DescriptorAllocator.h"

#include "DescriptorAllocatorPage.h"

DescriptorAllocator::DescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptorsPerHeap)
	:	m_heapType(type),
		m_numDescriptorsPerHeap(numDescriptorsPerHeap)
{
}

DescriptorAllocation DescriptorAllocator::allocate(uint32_t numDescriptors)
{
	std::lock_guard<std::mutex> lock(m_allocationMutex);

	DescriptorAllocation allocation;

	for (auto itr = m_availableHeaps.begin(); itr != m_availableHeaps.end(); ++itr)
	{
		auto allocatorPage = m_heapPool[*itr];

		// Try allocating the descriptors
		allocation = allocatorPage->allocate(numDescriptors);

		// Remove full pages (heaps) from available pages
		if (allocatorPage->numFreeHandles() == 0)
		{
			itr = m_availableHeaps.erase(itr);
		}

		// The allocation was valid
		if (!allocation.isNull())
		{
			break;
		}
	}

	// No available page could satisfy the requested number of descriptors
	if (allocation.isNull())
	{
		// Create a new page which can
		m_numDescriptorsPerHeap = std::max(m_numDescriptorsPerHeap, numDescriptors);
		auto newPage = createAllocatorPage();
		allocation = newPage->allocate(numDescriptors);
	}

	return allocation;
}

void DescriptorAllocator::releaseStaleDescriptors(uint32_t frameNumber)
{
	std::lock_guard<std::mutex> lock(m_allocationMutex);

	for (size_t i = 0; i < m_heapPool.size(); ++i)
	{
		auto page = m_heapPool[i];

		// Release any descriptors that are not in use anymore
		page->releaseStaleDescriptors(frameNumber);

		// Is there any space for new descriptors?
		if (page->numFreeHandles() > 0)
		{
			// Set is unique, so can't contain same heap twice
			m_availableHeaps.insert(i);
		}
	}
}

std::shared_ptr<DescriptorAllocatorPage> DescriptorAllocator::createAllocatorPage()
{
	auto newPage = std::make_shared<DescriptorAllocatorPage>(m_heapType, m_numDescriptorsPerHeap);

	m_heapPool.emplace_back(newPage);
	m_availableHeaps.insert(m_heapPool.size() - 1);

	return newPage;
}


