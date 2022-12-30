#include "dxpch.h"
#include "CommandQueue.h"

CommandQueue::CommandQueue(Microsoft::WRL::ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type)
    :   m_fenceValue(0),
        m_commandListType(type),
        m_device(device)
{
    D3D12_COMMAND_QUEUE_DESC desc = {};
    desc.Type = type;
    desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    desc.NodeMask = 0;

    ThrowIfFailed(m_device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_commandQueue)));
    ThrowIfFailed(m_device->CreateFence(m_fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));

    m_fenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
}

CommandQueue::~CommandQueue()
{
    ::CloseHandle(m_fenceEvent);
}


Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CommandQueue::createCommandAllocator()
{
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
    ThrowIfFailed(m_device->CreateCommandAllocator(m_commandListType, IID_PPV_ARGS(&commandAllocator)));

    return commandAllocator;
}


Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> CommandQueue::createCommandList(Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator)
{
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList;
    ThrowIfFailed(m_device->CreateCommandList(0, m_commandListType, allocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));

    return commandList;
}


Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> CommandQueue::getCommandList()
{
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList;

    if (!m_commandAllocatorQueue.empty() && isFenceComplete(m_commandAllocatorQueue.front().fenceValue))
    {
        commandAllocator = m_commandAllocatorQueue.front().commandAllocator;
        m_commandAllocatorQueue.pop();

        ThrowIfFailed(commandAllocator->Reset());
    }
    else
    {
        commandAllocator = createCommandAllocator();
    }


    if (!m_commandListQueue.empty())
    {
        commandList = m_commandListQueue.front();
        m_commandListQueue.pop();

        ThrowIfFailed(commandList->Reset(commandAllocator.Get(), nullptr));
    }
    else
    {
        commandList = createCommandList(commandAllocator);
    }

    // Associate the command allocator with the command list so that it can be
    // retrieved when the command list is executed.
    ThrowIfFailed(commandList->SetPrivateDataInterface(__uuidof(ID3D12CommandAllocator), commandAllocator.Get()));

    return commandList;
}


uint64_t CommandQueue::executeCommandList(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList)
{
    commandList->Close();

    ID3D12CommandAllocator* commandAllocator;
    UINT dataSize = sizeof(commandAllocator);
    ThrowIfFailed(commandList->GetPrivateData(__uuidof(ID3D12CommandAllocator), &dataSize, &commandAllocator));

    ID3D12CommandList* const ppCommandLists[] = {
        commandList.Get()
    };

    m_commandQueue->ExecuteCommandLists(1, ppCommandLists);
    uint64_t fenceValue = signal();

    m_commandAllocatorQueue.emplace(CommandAllocatorEntry{ fenceValue, commandAllocator });
    m_commandListQueue.push(commandList);

    // The ownership of the command allocator has been transferred to the ComPtr
    // in the command allocator queue. It is safe to release the reference 
    // in this temporary COM pointer here.
    commandAllocator->Release();

    return fenceValue;
}

uint64_t CommandQueue::signal()
{
    uint64_t fenceValueForSignal = ++m_fenceValue;
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fenceValueForSignal));

    return fenceValueForSignal;
}

bool CommandQueue::isFenceComplete(uint64_t fenceValue)
{
    return m_fence->GetCompletedValue() >= fenceValue;
}

void CommandQueue::waitForFenceValue(uint64_t fenceValue, std::chrono::milliseconds duration)
{
    if (m_fence->GetCompletedValue() < fenceValue)
    {
        ThrowIfFailed(m_fence->SetEventOnCompletion(fenceValue, m_fenceEvent));
        ::WaitForSingleObject(m_fenceEvent, static_cast<DWORD>(duration.count()));
    }
}

void CommandQueue::flush()
{
    uint64_t fenceValueForSignal = signal();
    waitForFenceValue(fenceValueForSignal);
}


