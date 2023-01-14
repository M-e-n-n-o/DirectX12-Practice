#pragma once

#include "d3dx12.h"

#include <cstdint>
#include <mutex>
#include <memory>
#include <set>
#include <vector>

#include "DescriptorAllocation.h"

class DescriptorAllocatorPage;

// https://learn.microsoft.com/en-us/windows/win32/direct3d12/resource-binding-flow-of-control

/*
*	Class which allocates CPU visible decriptor heaps and the descriptors themselfs.
*	Uses free list memory allocation scheme.
*/
class DescriptorAllocator
{
public:
	DescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptorsPerHeap = 256);
	virtual ~DescriptorAllocator() = default;

	/*
	* Allocate a number of contiguous descriptors from a CPU visible descriptor heap.
	* 
	* @param numDescriptors The number of contiguous descriptors to allocate.
	* Cannot be more than the number of descriptors per descriptor heap.
	*/
	DescriptorAllocation allocate(uint32_t numDescriptors = 1);

	/*
	* When the frame has completed, the stale descriptors can be released.
	*/
	void releaseStaleDescriptors(uint32_t frameNumber);

private:
	using DescriptorHeapPool = std::vector<std::shared_ptr<DescriptorAllocatorPage>>;

	// Create a new heap with a specific number of descriptors
	std::shared_ptr<DescriptorAllocatorPage> createAllocatorPage();

	D3D12_DESCRIPTOR_HEAP_TYPE m_heapType;
	uint32_t m_numDescriptorsPerHeap;

	DescriptorHeapPool m_heapPool;
	// Indices of available heaps in the heap pool
	std::set<size_t> m_availableHeaps;

	std::mutex m_allocationMutex;
};