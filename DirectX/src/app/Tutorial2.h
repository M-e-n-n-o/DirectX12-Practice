#pragma once

#include "dxpch.h"

#include <chrono>
#include "Application.h"
#include "CommandQueue.h"

#define SWAPCHAIN_BUFFER_COUNT 3

class Tutorial2 : public Game
{
public:
    Tutorial2() = default;
    ~Tutorial2() = default;

    std::shared_ptr<Window> Initialize(const WindowSettings& settings) override;
    void Destory() override;

    void onUpdate() override;
    void onRender() override;

    void onKeyPressed(KeyEvent& event) override;
    void onResize(ResizeEvent& event) override;

private:
    // Transition a resource
    void transitionResource(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList,
        Microsoft::WRL::ComPtr<ID3D12Resource> resource,
        D3D12_RESOURCE_STATES beforeState, D3D12_RESOURCE_STATES afterState);

    // Clear a render target view.
    void clearRTV(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList,
        D3D12_CPU_DESCRIPTOR_HANDLE rtv, FLOAT* clearColor);

    // Clear the depth of a depth-stencil view.
    void clearDepth(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList,
        D3D12_CPU_DESCRIPTOR_HANDLE dsv, FLOAT depth = 1.0f);

    // Create a GPU buffer.
    void updateBufferResource(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList,
        ID3D12Resource** pDestinationResource, ID3D12Resource** pIntermediateResource,
        size_t numElements, size_t elementSize, const void* bufferData,
        D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);

    // Resize the depth buffer to match the size of the client area.
    void resizeDepthBuffer(int width, int height);

private:
    uint64_t frameFenceValues[SWAPCHAIN_BUFFER_COUNT] = {};

    std::shared_ptr<Window> window;
    std::shared_ptr<CommandQueue> commandQueue;
};