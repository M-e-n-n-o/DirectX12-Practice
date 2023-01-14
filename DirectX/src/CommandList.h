#pragma once

#include "d3dx12.h"

#include <wrl.h>

#include <cstdint>

class CommandList
{
public:
	CommandList(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2>& commandList)
		:	m_commandList(commandList)
	{}

	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> getCommandList() const { return m_commandList; }

	void setDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, ID3D12DescriptorHeap* descriptorHeap)
	{
		if (m_descriptorHeaps[type] != descriptorHeap)
		{
			m_descriptorHeaps[type] = descriptorHeap;
			bindDescriptorHeaps();
		}
	}

private:
	void bindDescriptorHeaps()
	{
		UINT numDescriptorHeaps = 0;
		ID3D12DescriptorHeap* descriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES] = {};

		for (uint32_t i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
		{
			ID3D12DescriptorHeap* descriptorHeap = m_descriptorHeaps[i];
			if (descriptorHeap != nullptr)
			{
				descriptorHeaps[numDescriptorHeaps++] = descriptorHeap;
			}
		}

		m_commandList->SetDescriptorHeaps(numDescriptorHeaps, descriptorHeaps);
	}

	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> m_commandList;

	ID3D12DescriptorHeap* m_descriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
};