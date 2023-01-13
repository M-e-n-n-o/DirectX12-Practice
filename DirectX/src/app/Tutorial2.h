#pragma once

#include "dxpch.h"

#include <chrono>
#include "Application.h"
#include "SwapChain.h"
#include "CommandQueue.h"
#include "VertexArray.h"
#include "RootSignature.h"
#include "DescriptorAllocator.h"
#include "DynamicDescriptorHeap.h"
#include "UploadBuffer.h"
#include "CommandList.h"

#define SWAPCHAIN_BUFFER_COUNT 3

class Tutorial2 : public Game
{
public:
    Tutorial2() = default;
    ~Tutorial2() = default;

    std::shared_ptr<Window> Initialize(const WindowSettings& settings) override;
    void Destory() override;

    void onUpdate(float delta) override;
    void onRender() override;

    void onKeyPressed(KeyEvent& event) override;
    void onResize(ResizeEvent& event) override;

private:
    // Transition a resource
    void transitionResource(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList,
        Microsoft::WRL::ComPtr<ID3D12Resource> resource,
        D3D12_RESOURCE_STATES beforeState, D3D12_RESOURCE_STATES afterState);

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
    std::shared_ptr<SwapChain> swapChain;
    std::shared_ptr<CommandQueue> commandQueueCopy;
    std::shared_ptr<CommandQueue> commandQueueDirect;

    // Descriptor heap depth buffer
    std::shared_ptr<DescriptorAllocator> dsvDescAllocator;
    DescriptorAllocation dsvTable;

    std::shared_ptr<RootSignature> rootSignature;

    // Vertex buffer cube
    std::shared_ptr<VertexArray> vao;


    std::shared_ptr<UploadBuffer> uploadBuffer;
    //std::shared_ptr<DynamicDescriptorHeap> cbvDescriptorHeap;

    //Microsoft::WRL::ComPtr<ID3D12Resource> vertexPosBuffer;
    //D3D12_VERTEX_BUFFER_VIEW vertexPosBufferView;
    //Microsoft::WRL::ComPtr<ID3D12Resource> vertexColorBuffer;
    //D3D12_VERTEX_BUFFER_VIEW vertexColorBufferView;
    //// Index buffer cube
    //Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer;
    //D3D12_INDEX_BUFFER_VIEW indexBufferView;

    // Depth buffer
    Microsoft::WRL::ComPtr<ID3D12Resource> depthBuffer;
    // Descriptor heap depth buffer
    //Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvHeap;

    // Root signature
    //Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
    // Pipeline state object.
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
    // Used to initialize the raterizer stage of the pipeline
    D3D12_VIEWPORT viewport;
    D3D12_RECT scissorRect;

    float fov;

    DirectX::XMMATRIX modelMatrix = DirectX::XMMatrixIdentity();
    DirectX::XMMATRIX viewMatrix = DirectX::XMMatrixIdentity();
    DirectX::XMMATRIX projectionMatrix = DirectX::XMMatrixIdentity();

    bool contentLoaded;
};