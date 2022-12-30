#pragma once

// Windows Runtime Library. Needed for Microsoft::WRL::ComPtr<> template class.
#include <wrl.h>

// DirectX 12 specific headers.
#include <d3d12.h>

// STL Headers
#include <algorithm>
#include <queue>

class CommandQueue
{
public:
	CommandQueue(Microsoft::WRL::ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type);
	~CommandQueue();

	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> getCommandList();

	// Returns the fence value to wait for this command list
	uint64_t executeCommandList(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList);

	uint64_t signal();
	bool isFenceComplete(uint64_t fenceValue);
	void waitForFenceValue(uint64_t fenceValue, std::chrono::milliseconds duration = std::chrono::milliseconds::max());
	void flush();

    Microsoft::WRL::ComPtr<ID3D12CommandQueue> getCommandQueue() const { return m_commandQueue; }

private:
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> createCommandAllocator();
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> createCommandList(Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator);

private:
    // Keep track of command allocators that are "in-flight"
    struct CommandAllocatorEntry
    {
        uint64_t fenceValue;
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
    };

    using CommandAllocatorQueue = std::queue<CommandAllocatorEntry>;
    using CommandListQueue = std::queue<Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2>>;

    D3D12_COMMAND_LIST_TYPE                     m_commandListType;
    Microsoft::WRL::ComPtr<ID3D12Device2>       m_device;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue>  m_commandQueue;
    Microsoft::WRL::ComPtr<ID3D12Fence>         m_fence;
    HANDLE                                      m_fenceEvent;
    uint64_t                                    m_fenceValue;

    CommandAllocatorQueue                       m_commandAllocatorQueue;
    CommandListQueue                            m_commandListQueue;
};