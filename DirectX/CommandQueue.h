#pragma once

#include <vector>

// Windows Runtime Library. Needed for Microsoft::WRL::ComPtr<> template class.
#include <wrl.h>

// DirectX 12 specific headers.
#include <d3d12.h>

class CommandQueue
{
public:
	CommandQueue(Microsoft::WRL::ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type, uint8_t numFrames, uint32_t backbufferIndex);
	~CommandQueue();

	Microsoft::WRL::ComPtr<ID3D12CommandQueue> getCommandQueue() const { return m_commandQueue; }

private:
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> createCommandAllocator(Microsoft::WRL::ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type);

	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> createCommandList(Microsoft::WRL::ComPtr<ID3D12Device2> device, Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator, D3D12_COMMAND_LIST_TYPE type);

private:
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue;

	std::vector<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>> m_commandAllocators;

	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;
};