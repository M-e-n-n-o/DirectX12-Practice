#include "CommandQueue.h"

#include "Helpers.h"

CommandQueue::CommandQueue(Microsoft::WRL::ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type, uint8_t numFrames, uint32_t backbufferIndex)
{
    D3D12_COMMAND_QUEUE_DESC desc = {};
    desc.Type = type;
    desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    desc.NodeMask = 0;

    ThrowIfFailed(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_commandQueue)));

    for (int i = 0; i < numFrames; ++i)
    {
        m_commandAllocators.push_back(createCommandAllocator(device, D3D12_COMMAND_LIST_TYPE_DIRECT));
    }

    m_commandList = createCommandList(device, m_commandAllocators[backbufferIndex], D3D12_COMMAND_LIST_TYPE_DIRECT);
}

CommandQueue::~CommandQueue()
{
}

Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CommandQueue::createCommandAllocator(Microsoft::WRL::ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type)
{
    return Microsoft::WRL::ComPtr<ID3D12CommandAllocator>();
}

Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> CommandQueue::createCommandList(Microsoft::WRL::ComPtr<ID3D12Device2> device, Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator, D3D12_COMMAND_LIST_TYPE type)
{
    return Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>();
}
