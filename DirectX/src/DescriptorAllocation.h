#pragma once

#include "d3dx12.h"

#include <cstdint>
#include <memory>

class DescriptorAllocatorPage;

/*
* Descriptor table wrapper class
*/
class DescriptorAllocation
{
public:
	// Creates  a NULL descriptor
	DescriptorAllocation();
	// Create a valid descriptor allocation
	DescriptorAllocation(D3D12_CPU_DESCRIPTOR_HANDLE descriptor, uint32_t numHandles, uint32_t descriptorSize, std::shared_ptr<DescriptorAllocatorPage> page);
	// The desctructor will automatically free the allocation
	~DescriptorAllocation();

	// Copies are not allowed
	DescriptorAllocation(const DescriptorAllocation&) = delete;
	DescriptorAllocation& operator=(const DescriptorAllocation&) = delete;

	// Move is allowed
	DescriptorAllocation(DescriptorAllocation&& allocation);
	DescriptorAllocation& operator= (DescriptorAllocation && other);

	// Check if this is a valid descriptor
	bool isNull() const;

	// Get a descriptor at a particular offset in the allocation
	D3D12_CPU_DESCRIPTOR_HANDLE getDescriptorHandle(uint32_t offset = 0) const;

	// Get the number of (consecutive) handles for this allocation
	uint32_t getNumHandles() const;

	// Get the heap that this allocation came from
	std::shared_ptr<DescriptorAllocatorPage> getDescriptorAllocatorPage() const;

private:
	// Free the descriptor back to the heap it came from
	void free();

	// The base descriptor
	D3D12_CPU_DESCRIPTOR_HANDLE m_descriptor;
	// The number of descriptors in this allocation
	uint32_t m_numHandles;
	// The offset to the next descriptor
	uint32_t m_descriptorSize;

	// A pointer back to the original page where this allocation came from
	std::shared_ptr<DescriptorAllocatorPage> m_page;
};