#include "dxpch.h"
#include <d3dcompiler.h>
#include "Tutorial2.h"

using namespace Microsoft::WRL;
using namespace DirectX;

// Clamp a value between a min and max range.
template<typename T>
constexpr const T& clamp(const T& val, const T& min, const T& max)
{
    return val < min ? min : val > max ? max : val;
}

// Vertex data for a colored cube.
struct VertexPosColor
{
    XMFLOAT3 Position;
    XMFLOAT3 Color;
};

static VertexPosColor g_Vertices[8] = {
    { XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f) }, // 0
    { XMFLOAT3(-1.0f,  1.0f, -1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) }, // 1
    { XMFLOAT3(1.0f,  1.0f, -1.0f), XMFLOAT3(1.0f, 1.0f, 0.0f) }, // 2
    { XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) }, // 3
    { XMFLOAT3(-1.0f, -1.0f,  1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f) }, // 4
    { XMFLOAT3(-1.0f,  1.0f,  1.0f), XMFLOAT3(0.0f, 1.0f, 1.0f) }, // 5
    { XMFLOAT3(1.0f,  1.0f,  1.0f), XMFLOAT3(1.0f, 1.0f, 1.0f) }, // 6
    { XMFLOAT3(1.0f, -1.0f,  1.0f), XMFLOAT3(1.0f, 0.0f, 1.0f) }  // 7
};

static XMFLOAT3 g_VerticesPos[8] = {
    XMFLOAT3(-1.0f, -1.0f, -1.0f),
    XMFLOAT3(-1.0f,  1.0f, -1.0f),
    XMFLOAT3(1.0f,  1.0f, -1.0f),
    XMFLOAT3(1.0f, -1.0f, -1.0f),
    XMFLOAT3(-1.0f, -1.0f,  1.0f),
    XMFLOAT3(-1.0f,  1.0f,  1.0f),
    XMFLOAT3(1.0f,  1.0f,  1.0f),
    XMFLOAT3(1.0f, -1.0f,  1.0f)
};

static XMFLOAT3 g_VerticesColor[8] = {
     XMFLOAT3(0.0f, 0.0f, 0.0f),
     XMFLOAT3(0.0f, 1.0f, 0.0f),
     XMFLOAT3(1.0f, 1.0f, 0.0f),
     XMFLOAT3(1.0f, 0.0f, 0.0f),
     XMFLOAT3(0.0f, 0.0f, 1.0f),
     XMFLOAT3(0.0f, 1.0f, 1.0f),
     XMFLOAT3(1.0f, 1.0f, 1.0f),
     XMFLOAT3(1.0f, 0.0f, 1.0f)
};

static WORD g_Indicies[36] =
{
    0, 1, 2, 0, 2, 3,
    4, 6, 5, 4, 7, 6,
    4, 5, 1, 4, 1, 0,
    3, 2, 6, 3, 6, 7,
    1, 5, 6, 1, 6, 2,
    4, 0, 3, 4, 3, 7
};

std::shared_ptr<Window> Tutorial2::Initialize(const WindowSettings& settings)
{
    scissorRect = CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX);
    viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(settings.width), static_cast<float>(settings.height));
    fov = 45.0;
    contentLoaded = false;

    auto device = Application::Get()->getDevice();

    window = std::make_shared<Window>(settings, Application::Get());

    commandQueueCopy = std::make_shared<CommandQueue>(device, D3D12_COMMAND_LIST_TYPE_COPY);
    commandQueueDirect = std::make_shared<CommandQueue>(device, D3D12_COMMAND_LIST_TYPE_DIRECT);

    swapChain = std::make_shared<SwapChain>(device, commandQueueDirect->getCommandQueue(), settings.width, settings.height,
        SWAPCHAIN_BUFFER_COUNT, window->getWindowHandle(), settings.tearingSupported);
    window->setSwapChain(swapChain);


    // Upload vertex buffer data
    vao = std::make_shared<VertexArray>();

    auto vboPos = std::make_shared<VertexBuffer>(_countof(g_VerticesPos), sizeof(XMFLOAT3), g_VerticesPos);
    auto vboColor = std::make_shared<VertexBuffer>(_countof(g_VerticesColor), sizeof(XMFLOAT3), g_VerticesColor);
    std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout = vao->setVertexBuffers({
        { vboPos, { { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT } } },
        { vboColor, { { "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT } } }
    });

    auto ibo = std::make_shared<IndexBuffer>(_countof(g_Indicies), sizeof(WORD), g_Indicies);
    vao->setIndexBuffer(ibo);

    vao->uploadDataToGPU(commandQueueCopy);


#if defined(_DEBUG)
    // Enable better shader debugging with the graphics debugging tools.
    UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    UINT compileFlags = 0;
#endif

    // Load the vertex shader
    ComPtr<ID3DBlob> vertexShaderBlob;
    ThrowIfFailed(D3DReadFileToBlob(L"VertexShader.cso", &vertexShaderBlob));
    //ThrowIfFailed(D3DCompileFromFile(L"Shader.hlsl", nullptr, nullptr, "VSMain", "vs_5_1", compileFlags, 0, &vertexShaderBlob, nullptr));
    // Load the pixel shader
    ComPtr<ID3DBlob> pixelShaderBlob;
    ThrowIfFailed(D3DReadFileToBlob(L"PixelShader.cso", &pixelShaderBlob));
    //ThrowIfFailed(D3DCompileFromFile(L"Shader.hlsl", nullptr, nullptr, "PSMain", "ps_5_1", compileFlags, 0, &vertexShaderBlob, nullptr));

    // Create the descriptor heap for the depth-stencil view
    dsvDescAllocator = std::make_shared<DescriptorAllocator>(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1);
    dsvTable = dsvDescAllocator->allocate();


    // Create a root signature
    D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData{};
    featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
    if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
    {
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    }

    // Allow input layout and deny unnecessary access to certain pipeline stages.
    D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;


    CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
    CD3DX12_ROOT_PARAMETER1 rootParameters[1];
    ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);
    //rootParameters[0].InitAsConstants(sizeof(XMMATRIX) / 4, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
    rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_VERTEX);
    //rootParameters[0].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX);

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr, rootSignatureFlags);

    rootSignature = std::make_shared<RootSignature>();
    rootSignature->setRootSignatureDesc(rootSignatureDesc.Desc_1_1, featureData.HighestVersion);

    uploadBuffer = std::make_shared<UploadBuffer>();

    for (int i = 0; i < SWAPCHAIN_BUFFER_COUNT; i++)
    {
        cbvGPUDescriptorHeap[i] = std::make_shared<DynamicDescriptorHeap>(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 9);
    }
    
    cbvCPUDescAllocator = std::make_shared<DescriptorAllocator>(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    struct PipelineStateStream
    {
        CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
        CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
        CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
        CD3DX12_PIPELINE_STATE_STREAM_VS VS;
        CD3DX12_PIPELINE_STATE_STREAM_PS PS;
        CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
        CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
    } pipelineStateStream;

    // Define the render target formats
    D3D12_RT_FORMAT_ARRAY rtvFormats{};
    rtvFormats.NumRenderTargets = 1;
    rtvFormats.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

    // Describe PipeLine State Object using a user defined struct
    pipelineStateStream.pRootSignature = rootSignature->getRootSignature().Get();
    pipelineStateStream.InputLayout = { &inputLayout[0], inputLayout.size() };
    pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipelineStateStream.VS = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
    pipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
    pipelineStateStream.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    pipelineStateStream.RTVFormats = rtvFormats;

    // Create the actual pipeline State Object (PSO)
    D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
        sizeof(PipelineStateStream), &pipelineStateStream
    };
    ThrowIfFailed(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&pipelineState)));

    contentLoaded = true;

    // Resize/Create the depth buffer
    resizeDepthBuffer(settings.width, settings.height);

    return window;
}

void Tutorial2::Destory()
{
    commandQueueCopy->flush();
    commandQueueDirect->flush();
}

void Tutorial2::onUpdate(float delta)
{
    if (!contentLoaded)
    {
        return;
    }

    // Update the model matrix
    static float angle = 0.0f;
    angle += (delta * 30.0f);
    const XMVECTOR rotationAxis = XMVectorSet(0, 1, 1, 0);
    modelMatrix = XMMatrixRotationAxis(rotationAxis, XMConvertToRadians(angle));

    // Update the view matrix
    const XMVECTOR eyePosition = XMVectorSet(0, 0, -10, 1);
    const XMVECTOR focusPoint = XMVectorSet(0, 0, 0, 1);
    const XMVECTOR upDirection = XMVectorSet(0, 1, 0, 0);
    viewMatrix = XMMatrixLookAtLH(eyePosition, focusPoint, upDirection);

    // Update the projection matrix
    auto window = Application::Get()->getWindow();
    float aspectRatio = window->getWidth() / static_cast<float>(window->getHeight());
    projectionMatrix = XMMatrixPerspectiveFovLH(XMConvertToRadians(fov), aspectRatio, 0.1f, 100.0f);
}

void Tutorial2::onRender()
{
    if (!contentLoaded)
    {
        return;
    }

    auto commandList = commandQueueDirect->getCommandList();
    CommandList cl(commandList);

    UINT currentBackBufferIndex = swapChain->getCurrentBackBufferIndex();
    auto backBuffer = swapChain->getCurrentBackBuffer();
    auto rtv = swapChain->getCurrentRenderTargetView();
    auto dsv = dsvTable.getDescriptorHandle();

    cbvGPUDescriptorHeap[currentBackBufferIndex]->reset();
    uploadBuffer->reset();
    cbvCPUDescAllocator->releaseStaleDescriptors(Application::Get()->getFrameCount());
    dsvDescAllocator->releaseStaleDescriptors(Application::Get()->getFrameCount());

    cbvGPUDescriptorHeap[currentBackBufferIndex]->parseRootSignature(*rootSignature);

    // Clear the render targets
    {
        transitionResource(commandList, backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

        FLOAT clearColor[] = { 0.4f, 0.6f, 0.9f, 1.0f };
        commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);

        commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    }

    commandList->SetPipelineState(pipelineState.Get());
    commandList->SetGraphicsRootSignature(rootSignature->getRootSignature().Get());

    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    vao->bind(commandList);

    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissorRect);

    commandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

    // Only create 1 CPU visible descriptor table for all the objects because each object can overwrite 
    // the previous one because the data in the table will be copied over to a GPU visible descriptor 
    // before the next object is submitted to the commandlist.
    DescriptorAllocation cbvCPUTable = cbvCPUDescAllocator->allocate();

    // Render each object
    for (int i = -20; i <= 20; i += 5)
    {
        auto m = modelMatrix * XMMatrixTranslation(i, 0, 30);
        auto mvpMatrix = m * viewMatrix;
        mvpMatrix = mvpMatrix * projectionMatrix;

        //commandList->SetGraphicsRoot32BitConstants(0, sizeof(XMMATRIX) / 4, &mvpMatrix, 0)

        // CB size need to be 256-byte aligned!
        UploadBuffer::Allocation resourceAllocation = uploadBuffer->allocate(sizeof(XMMATRIX), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
        memcpy(resourceAllocation.cpu, &mvpMatrix, sizeof(XMMATRIX));

        // Inline CBV (not in a table)
        //commandList->SetGraphicsRootConstantBufferView(0, heapAllocation.gpu);

        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
        cbvDesc.BufferLocation = resourceAllocation.gpu;
        cbvDesc.SizeInBytes = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
        Application::Get()->getDevice()->CreateConstantBufferView(&cbvDesc, cbvCPUTable.getDescriptorHandle());
        cbvGPUDescriptorHeap[currentBackBufferIndex]->stageDescriptors(0, 0, 1, cbvCPUTable.getDescriptorHandle());
        cbvGPUDescriptorHeap[currentBackBufferIndex]->commitStagedDescriptorsForDraw(cl);

        commandList->DrawIndexedInstanced(_countof(g_Indicies), 1, 0, 0, 0);
    }

    // Present
    {
        transitionResource(commandList, backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

        frameFenceValues[currentBackBufferIndex] = commandQueueDirect->executeCommandList(commandList);

        currentBackBufferIndex = swapChain->present();

        // Wait until the new backbuffer is ready to be used
        commandQueueDirect->waitForFenceValue(frameFenceValues[currentBackBufferIndex]);
    }
}

void Tutorial2::onKeyPressed(KeyEvent& event)
{

}

void Tutorial2::onResize(ResizeEvent& event)
{
    commandQueueCopy->flush();
    commandQueueDirect->flush();

    uint32_t currentBackBufferIndex = swapChain->getCurrentBackBufferIndex();
    for (int i = 0; i < SWAPCHAIN_BUFFER_COUNT; i++)
    {
        frameFenceValues[i] = frameFenceValues[currentBackBufferIndex];
    }

    viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(event.width), static_cast<float>(event.height));

    resizeDepthBuffer(event.width, event.height);
}

void Tutorial2::transitionResource(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList, Microsoft::WRL::ComPtr<ID3D12Resource> resource, D3D12_RESOURCE_STATES beforeState, D3D12_RESOURCE_STATES afterState)
{
    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        resource.Get(),
        beforeState, afterState);

    commandList->ResourceBarrier(1, &barrier);
}

void Tutorial2::updateBufferResource(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList, ID3D12Resource** pDestinationResource, ID3D12Resource** pIntermediateResource, size_t numElements, size_t elementSize, const void* bufferData, D3D12_RESOURCE_FLAGS flags)
{
    auto device = Application::Get()->getDevice();

    size_t bufferSize = numElements * elementSize;

    // Create a committed resource for the GPU resource in a default heap.
    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(bufferSize, flags),
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(pDestinationResource)));


    // Create an committed resource for the upload.
    if (bufferData != nullptr)
    {
        ThrowIfFailed(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(pIntermediateResource)));

        D3D12_SUBRESOURCE_DATA subresourceData = {};
        subresourceData.pData = bufferData;
        subresourceData.RowPitch = bufferSize;
        subresourceData.SlicePitch = subresourceData.RowPitch;

        UpdateSubresources(commandList.Get(),
            *pDestinationResource, *pIntermediateResource,
            0, 0, 1, &subresourceData);
    }
}

void Tutorial2::resizeDepthBuffer(int width, int height)
{
    if (contentLoaded)
    {
        commandQueueCopy->flush();
        commandQueueDirect->flush();

        width = std::max(1, width);
        height = std::max(1, height);

        auto device = Application::Get()->getDevice();

        // Resize screen dependent resources
        // Create a depth buffer
        D3D12_CLEAR_VALUE optimizedClearValue{};
        optimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
        optimizedClearValue.DepthStencil = { 1.0f, 0 };

        ThrowIfFailed(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, width, height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &optimizedClearValue,
            IID_PPV_ARGS(&depthBuffer)
        ));

        // Update the depth-stencil view
        D3D12_DEPTH_STENCIL_VIEW_DESC dsv{};
        dsv.Format = DXGI_FORMAT_D32_FLOAT;
        dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsv.Texture2D.MipSlice = 0;
        dsv.Flags = D3D12_DSV_FLAG_NONE;

        device->CreateDepthStencilView(depthBuffer.Get(), &dsv, dsvTable.getDescriptorHandle());
    }
}
