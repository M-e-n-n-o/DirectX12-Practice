#include "dxpch.h"
#include "SwapChain.h"

SwapChain::SwapChain(Microsoft::WRL::ComPtr<ID3D12Device2> device, Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue, 
    uint32_t width, uint32_t height, uint32_t bufferCount, HWND windowHandle, bool tearingSupported)
    :   m_device(device),
        m_bufferCount(bufferCount),
        m_isTearingSupported(tearingSupported)
{
    ComPtr<IDXGIFactory4> dxgiFactory4;
    UINT createFactoryFlags = 0;
#if defined(_DEBUG)
    createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

    ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory4)));

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = width;
    swapChainDesc.Height = height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.Stereo = FALSE;
    swapChainDesc.SampleDesc = { 1, 0 };
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = m_bufferCount;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    // It is recommended to always allow tearing if tearing support is available.
    swapChainDesc.Flags = m_isTearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

    ComPtr<IDXGISwapChain1> swapChain1;
    ThrowIfFailed(dxgiFactory4->CreateSwapChainForHwnd(
        commandQueue.Get(),
        windowHandle,
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain1));

    // Disable the Alt+Enter fullscreen toggle feature. Switching to fullscreen
    // will be handled manually.
    ThrowIfFailed(dxgiFactory4->MakeWindowAssociation(windowHandle, DXGI_MWA_NO_ALT_ENTER));

    ThrowIfFailed(swapChain1.As(&m_swapChain));

    m_currentBackBufferIndex = m_swapChain->GetCurrentBackBufferIndex();

    m_RTVDescriptorHeap = createDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, m_bufferCount);
    m_RTVDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    m_backBuffers.reserve(m_bufferCount);
    updateRenterTargetViews();
}

void SwapChain::present()
{
    UINT syncInterval = m_vSync ? 1 : 0;
    UINT presentFlags = m_isTearingSupported && !m_vSync ? DXGI_PRESENT_ALLOW_TEARING : 0;
    ThrowIfFailed(m_swapChain->Present(syncInterval, presentFlags));
    m_currentBackBufferIndex = m_swapChain->GetCurrentBackBufferIndex();
}

void SwapChain::resize(uint32_t width, uint32_t height)
{
    for (uint32_t i = 0; i < m_bufferCount; ++i)
    {
        // Any references to the back buffers must be released
        // before the swap chain can be resized.
        m_backBuffers[i].Reset();
    }

    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    ThrowIfFailed(m_swapChain->GetDesc(&swapChainDesc));
    ThrowIfFailed(m_swapChain->ResizeBuffers(m_bufferCount, width, height,
        swapChainDesc.BufferDesc.Format, swapChainDesc.Flags));

    m_currentBackBufferIndex = m_swapChain->GetCurrentBackBufferIndex();

    updateRenterTargetViews();
}

D3D12_CPU_DESCRIPTOR_HANDLE SwapChain::getCurrentRenderTargetView() const
{
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_RTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
        m_currentBackBufferIndex, m_RTVDescriptorSize);
}

void SwapChain::updateRenterTargetViews()
{
    auto rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_RTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

    for (uint32_t i = 0; i < m_bufferCount; ++i)
    {
        ComPtr<ID3D12Resource> backBuffer;
        ThrowIfFailed(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer)));

        m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, rtvHandle);

        m_backBuffers.insert(m_backBuffers.begin() + i, backBuffer);

        rtvHandle.Offset(rtvDescriptorSize);
    }
}

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> SwapChain::createDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptors)
{
    ComPtr<ID3D12DescriptorHeap> descriptorHeap;

    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.NumDescriptors = numDescriptors;
    desc.Type = type;

    ThrowIfFailed(m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptorHeap)));

    return descriptorHeap;
}
