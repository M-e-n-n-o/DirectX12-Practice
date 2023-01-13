#pragma once

#include "d3dx12.h"
#include <wrl.h>

#include <map>
#include <memory>
#include <mutex>
#include <queue>

#include "DescriptorAllocation.h"

/*
* Descriptor Heap wrapper class
*/
class DescriptorAllocatorPage : public std::enable_shared_from_this<DescriptorAllocatorPage>
{
public:
	DescriptorAllocatorPage(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptors);

	D3D12_DESCRIPTOR_HEAP_TYPE getHeapType() const;

	/*
	* Check to see if this descriptor page has a contiguous block of descriptors
	* large enough to satisfy the request.
	*/
	bool hasSpace(uint32_t numDescriptors) const;

	/*
	* Get the number of available handles in the heap.
	*/
	uint32_t numFreeHandles() const;

	/*
	* Allocate a number of descriptors from this descriptor heap.
	* If the allocation cannot be satisfied, then a NULL desciptor is returned.
	*/
	DescriptorAllocation allocate(uint32_t numDescriptors);

	/*
	* Return a descriptor back to the heap.
	* @param frameNumber State descriptors are not freed directly, but put
	* on a stale allocations queue. State allocations are returned to the heap
	* using the DescriptorAllocatorPage::ReleaseStateAllocations method.
	*/
	void free(DescriptorAllocation&& descriptorAllocation, uint64_t frameNumber);

	/*
	* Returned the state descriptors back to the descriptor heap.
	*/
	void releaseStaleDescriptors(uint32_t frameNumber);

protected:
	// Compute the offset of the descriptor handle from the start of the heap
	uint32_t computeOffset(D3D12_CPU_DESCRIPTOR_HANDLE handle);

	// Adds a new block to the free list
	void addNewBlock(uint32_t offset, uint32_t numDescriptors);

	// Free a block of descriptors. This will also merge free blocks in the free
	// list to form larger blocks that can be reused.
	void freeBlock(uint32_t offset, uint32_t numDescriptors);

private:
	// The offset (in descriptors) within the descriptor heap
	using OffsetType = uint32_t;
	// The number of descriptors that are available
	using SizeType = uint32_t;

	struct FreeBlockInfo;
	// A map that lists the free block by the offset within the descriptor heap
	using FreeListByOffset = std::map<OffsetType, FreeBlockInfo>;

	// A map that lists the free blocks by size. Needs to be multimap since
	// multiple blocks can have the same size.
	using FreeListBySize = std::multimap<SizeType, FreeListByOffset::iterator>;

	struct FreeBlockInfo
	{
		FreeBlockInfo(SizeType size): size(size) {}

		SizeType size;
		FreeListBySize::iterator freeListBySizeItr;
	};

	struct StaleDescriptorInfo
	{
		StaleDescriptorInfo(OffsetType offset, SizeType size, uint64_t frameNumber)
			:	offset(offset),
				size(size),
				frameNumber(frameNumber)
		{}

		// The offset within the descriptor heap
		OffsetType offset;
		// The number of descriptors
		SizeType size;
		// The frame number that the descriptor was freed
		uint64_t frameNumber;
	};

	// Stale descriptors are queued for release until the frame that they
	// were freed in has completed
	using StaleDescriptorQueue = std::queue<StaleDescriptorInfo>;

	FreeListByOffset m_freeListByOffset;
	FreeListBySize m_freeListBySize;
	StaleDescriptorQueue m_staleDescriptors;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_descriptorHeap;
	D3D12_DESCRIPTOR_HEAP_TYPE m_heapType;
	CD3DX12_CPU_DESCRIPTOR_HANDLE m_baseDescriptor;
	uint32_t m_numDescriptorHandleIncrementSize;
	uint32_t m_numDescriptorsInHeap;
	uint32_t m_numFreeHandles;

	std::mutex m_allocationMutex;
};