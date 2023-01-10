#pragma once

class DescriptorAllocation
{
public:

	bool isNull();

	D3D12_CPU_DESCRIPTOR_HANDLE getDescriptorHandle();

	uint32_t getNumHandles();
};