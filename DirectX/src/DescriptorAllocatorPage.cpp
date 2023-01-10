#include "dxpch.h"
#include "DescriptorAllocatorPage.h"

#include "Application.h"

DescriptorAllocatorPage::DescriptorAllocatorPage(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptors)
	:	m_heapType(type),
		m_numDescriptorsInHeap(numDescriptors)
{
	auto device = Application::Get()->getDevice();

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
	heapDesc.Type = m_heapType;
	heapDesc.NumDescriptors = m_numDescriptorsInHeap;

	ThrowIfFailed(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_descriptorHeap)));

	m_baseDescriptor = m_descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	m_numDescriptorHandleIncrementSize = device->GetDescriptorHandleIncrementSize(m_heapType);
	m_numFreeHandles = m_numDescriptorsInHeap;

	// Initialize the free lists
	addNewBlock(0, m_numFreeHandles);
}

D3D12_DESCRIPTOR_HEAP_TYPE DescriptorAllocatorPage::getHeapType() const
{
	return m_heapType;
}

bool DescriptorAllocatorPage::hasSpace(uint32_t numDescriptors) const
{
	return m_freeListBySize.lower_bound(numDescriptors) != m_freeListBySize.end();
}

uint32_t DescriptorAllocatorPage::numFreeHandles() const
{
	return m_numFreeHandles;
}

DescriptorAllocation DescriptorAllocatorPage::allocate(uint32_t numDescriptors)
{
	std::lock_guard<std::mutex> lock(m_allocationMutex);

	// There are less than the requested number of descriptors left in the heap.
	// Return a NULL descriptor and try another heap.
	if (numDescriptors > m_numFreeHandles)
	{
		return DescriptorAllocation();
	}

	// Get the first block that is large enough to satify the request.
	auto smallestBlockItr = m_freeListBySize.lower_bound(numDescriptors);
	if (smallestBlockItr == m_freeListBySize.end())
	{
		// There was no free block that could satisfy the request.
		return DescriptorAllocation();
	}

	// The size of the smallest block that satisfies the request.
	auto blockSize = smallestBlockItr->first;

	// The pointer to the same entry in the FreeListByOffset map.
	auto offsetItr = smallestBlockItr->second;

	// The offset in the descriptor heap.
	auto offset = offsetItr->first;

	// Remove the existing free block from the free list.
	m_freeListBySize.erase(smallestBlockItr);
	m_freeListByOffset.erase(offsetItr);

	// Compute the new free block that results from splitting this block.
	auto newOffset = offset + numDescriptors;
	auto newSize = blockSize - numDescriptors;

	if (newSize > 0)
	{
		// If the allocation did not exactly math the requested size,
		// return the left-over to the free list.
		addNewBlock(newOffset, newSize);
	}

	// Dexrement free handles.
	m_numFreeHandles -= numDescriptors;

	return DescriptorAllocation(CD3DX12_CPU_DESCRIPTOR_HANDLE(m_baseDescriptor, offset, m_numDescriptorHandleIncrementSize),
		numDescriptors, m_numDescriptorHandleIncrementSize, shared_from_this());
}

void DescriptorAllocatorPage::free(DescriptorAllocation&& descriptorAllocation, uint64_t frameNumber)
{
	// Compute the offset of the descriptor within the descriptor heap.
	auto offset = computeOffset(descriptorAllocation.getDescriptorHandle());

	std::lock_guard<std::mutex> lock(m_allocationMutex);

	// Don't add the block directly to the free list until the frame has completed.
	m_staleDescriptors.emplace(StaleDescriptorInfo(offset, descriptorAllocation.getNumHandles(), frameNumber));
}

void DescriptorAllocatorPage::releaseStaleDescriptors(uint32_t frameNumber)
{
	std::lock_guard<std::mutex> lock(m_allocationMutex);

	while (!m_staleDescriptors.empty() && m_staleDescriptors.front().frameNumber <= frameNumber)
	{
		auto& staleDescriptor = m_staleDescriptors.front();

		// The offset of the descriptor in the heap.
		auto offset = staleDescriptor.offset;
		// The number of descriptors that were allocated.
		auto numDescriptors = staleDescriptor.size;

		freeBlock(offset, numDescriptors);

		m_staleDescriptors.pop();
	}
}

uint32_t DescriptorAllocatorPage::computeOffset(D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
	return static_cast<uint32_t>(handle.ptr - m_baseDescriptor.ptr) / m_numDescriptorHandleIncrementSize;
}

void DescriptorAllocatorPage::addNewBlock(uint32_t offset, uint32_t numDescriptors)
{
	auto offsetItr = m_freeListByOffset.emplace(offset, FreeBlockInfo(numDescriptors));
	auto sizeItr = m_freeListBySize.emplace(numDescriptors, offsetItr.first);
	offsetItr.first->second.freeListBySizeItr = sizeItr;
}

void DescriptorAllocatorPage::freeBlock(uint32_t offset, uint32_t numDescriptors)
{
	// Find the first element whose offset is greater than the specified offset.
	// This is the block that should appear after the block that is being freed.
	auto nextBlockItr = m_freeListByOffset.upper_bound(offset);

	// Find the block that appears before the block being freed.
	auto prevBlockItr = nextBlockItr;

	// If it is not the first block in the list.
	if (prevBlockItr != m_freeListByOffset.begin())
	{
		// Go to the previous block in the list.
		--prevBlockItr;
	}
	else
	{
		// Otherwise, just set it to the end of the list to indicate that no
		// block comes before the one being freed.
		prevBlockItr = m_freeListByOffset.end();
	}

	// Add the number of free handles back to the heap. 
	// This needs to be done before merging any blocks since merging 
	// blocks modifies the numDescriptors variable.
	m_numFreeHandles += numDescriptors;


	// Add any free blocks before or after the block to the block to be freed
	// to minimize fragmentation in the free list


	if (prevBlockItr != m_freeListByOffset.end() &&
		offset == prevBlockItr->first + prevBlockItr->second.size)
	{
		// The previous block is exactly behind the block that is to be freed.
		//
		// PrevBlock.Offset           Offset
		// |                          |
		// |<-----PrevBlock.Size----->|<------Size-------->|
		//

		// Increase the block size by the size of merging with the previous block.
		offset = prevBlockItr->first;
		numDescriptors += prevBlockItr->second.size;

		// Remove the previous block from the free list.
		m_freeListBySize.erase(prevBlockItr->second.freeListBySizeItr);
		m_freeListByOffset.erase(prevBlockItr);
	}

	if (nextBlockItr != m_freeListByOffset.end() &&
		offset + numDescriptors == nextBlockItr->first)
	{
		// The next block is exactly in front of the block that is to be freed.
		//
		// Offset               NextBlock.Offset 
		// |                    |
		// |<------Size-------->|<-----NextBlock.Size----->|

		// Increase the block size by the size of merging with the next block.
		numDescriptors += nextBlockItr->second.size;

		// Remove the next block from the free list.
		m_freeListBySize.erase(nextBlockItr->second.freeListBySizeItr);
		m_freeListByOffset.erase(nextBlockItr);
	}

	// Add the freed block to the free list.
	addNewBlock(offset, numDescriptors);
}
