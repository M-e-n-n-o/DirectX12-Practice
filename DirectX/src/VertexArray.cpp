#include "dxpch.h"
#include "VertexArray.h"
#include "Application.h"

// --------------------------------------------------------
//                        Buffer
// --------------------------------------------------------

D3D12_VERTEX_BUFFER_VIEW& VertexBuffer::updateBufferResource(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> copyCommandList, ID3D12Resource** intermediateResource)
{
    auto device = Application::Get()->getDevice();

    size_t bufferSize = m_numElements * m_elementSize;

    // Create a committed resource for the GPU resource in a default heap.
    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(bufferSize, m_flags),
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&m_buffer)));

    // Create an committed resource for the upload.
    if (m_data != nullptr)
    {
        ThrowIfFailed(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(intermediateResource)));

        D3D12_SUBRESOURCE_DATA subresourceData = {};
        subresourceData.pData = m_data;
        subresourceData.RowPitch = bufferSize;
        subresourceData.SlicePitch = subresourceData.RowPitch;

        UpdateSubresources(copyCommandList.Get(), m_buffer.Get(), *intermediateResource, 0, 0, 1, &subresourceData);
    }

    // Create the buffer view (tells the input assembler where the vertices are stored in GPU memory)
    m_bufferView.BufferLocation = m_buffer->GetGPUVirtualAddress();
    m_bufferView.SizeInBytes = bufferSize;
    m_bufferView.StrideInBytes = m_elementSize;

    return m_bufferView;
}

D3D12_INDEX_BUFFER_VIEW& IndexBuffer::updateBufferResource(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> copyCommandList, ID3D12Resource** intermediateResource)
{
    auto device = Application::Get()->getDevice();

    size_t bufferSize = m_numElements * m_elementSize;

    // Create a committed resource for the GPU resource in a default heap.
    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(bufferSize, m_flags),
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&m_buffer)));

    // Create an committed resource for the upload.
    if (m_data != nullptr)
    {
        ThrowIfFailed(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(intermediateResource)));

        D3D12_SUBRESOURCE_DATA subresourceData = {};
        subresourceData.pData = m_data;
        subresourceData.RowPitch = bufferSize;
        subresourceData.SlicePitch = subresourceData.RowPitch;

        UpdateSubresources(copyCommandList.Get(), m_buffer.Get(), *intermediateResource, 0, 0, 1, &subresourceData);
    }

    // Create the buffer view (tells the input assembler where the vertices are stored in GPU memory)
    m_bufferView.BufferLocation = m_buffer->GetGPUVirtualAddress();
    m_bufferView.SizeInBytes = bufferSize;
    m_bufferView.Format = DXGI_FORMAT_R16_UINT;

    return m_bufferView;
}

// --------------------------------------------------------
//                      Vertex Array
// --------------------------------------------------------

VertexArray::~VertexArray()
{
    if (m_vertexBuffers.size() != 0)
    {
        free(m_vertexBufferViews);
        m_vertexBuffers.clear();
    }
}

void VertexArray::bind(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2>& commandList)
{
    commandList->IASetVertexBuffers(0, m_vertexBufferViewCount, m_vertexBufferViews);
    
    if (m_indexBuffer != nullptr)
    {
        commandList->IASetIndexBuffer(&m_indexBuffer->getBufferView());
    }
}

std::vector<D3D12_INPUT_ELEMENT_DESC> VertexArray::setVertexBuffers(std::initializer_list<VertexBufferDescription> vertexDescriptions)
{
    if (m_vertexBuffers.size() != 0)
    {
        // Remove the old vertex buffers
        free(m_vertexBufferViews);
        m_vertexBuffers.clear();
    }

    std::vector<VertexBufferDescription> descs = vertexDescriptions;
    m_vertexBufferViews = (D3D12_VERTEX_BUFFER_VIEW*) malloc(sizeof(D3D12_VERTEX_BUFFER_VIEW) * descs.size());

    std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout;
    UINT slot = 0;

    for (int i = 0; i < descs.size(); i++)
    {
        auto desc = descs[i];
        m_vertexBuffers.push_back(desc.vbo);

        for (int j = 0; j < desc.elements.size(); j++)
        {
            const auto& element = desc.elements[j];
            inputLayout.push_back({ element.semanticName, element.semanticIndex, element.format, slot,
                D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 });
        }
        slot++;
    }

    m_vertexBufferViewCount = descs.size();

    return inputLayout;
}

void VertexArray::uploadDataToGPU(std::shared_ptr<CommandQueue>& copyCommandQueue)
{
    auto commandList = copyCommandQueue->getCommandList();

    std::vector<ComPtr<ID3D12Resource>> intermediateBuffers;

    for (int i = 0; i < m_vertexBuffers.size(); i++)
    {
        ComPtr<ID3D12Resource> tempBuffer;
        m_vertexBufferViews[i] = m_vertexBuffers[i]->updateBufferResource(commandList, &tempBuffer);
        intermediateBuffers.push_back(tempBuffer);
    }

    if (m_indexBuffer != nullptr)
    {
        ComPtr<ID3D12Resource> tempBuffer;
        m_indexBuffer->updateBufferResource(commandList, &tempBuffer);
        intermediateBuffers.push_back(tempBuffer);
    }

    // Upload the vertex and index buffers to the GPU resources
    auto fenceValue = copyCommandQueue->executeCommandList(commandList);
    copyCommandQueue->waitForFenceValue(fenceValue);
}
