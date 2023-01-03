#pragma once

// Windows Runtime Library. Needed for Microsoft::WRL::ComPtr<> template class.
#include <wrl.h>

// DirectX 12 specific headers.
#include <d3d12.h>

// STL Headers
#include <memory>
#include <vector>

// Own Headers
#include "CommandQueue.h"

class VertexBuffer
{
public:
	VertexBuffer(size_t numElements, size_t elementSize, void* bufferData, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE)
		:	m_data(bufferData), m_numElements(numElements), m_elementSize(elementSize), m_flags(flags) {}
	~VertexBuffer() = default;

	D3D12_VERTEX_BUFFER_VIEW& updateBufferResource(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> copyCommandList, ID3D12Resource** intermediateResource);

	Microsoft::WRL::ComPtr<ID3D12Resource> getBuffer() const { return m_buffer; }
	D3D12_VERTEX_BUFFER_VIEW getBufferView() const { return m_bufferView; }
	const void* getData() const { return m_data; }

	size_t getElementSize() { return m_elementSize; }

private:
	Microsoft::WRL::ComPtr<ID3D12Resource>	m_buffer;
	D3D12_VERTEX_BUFFER_VIEW				m_bufferView;
	void*									m_data;
	size_t									m_numElements;
	size_t									m_elementSize;
	D3D12_RESOURCE_FLAGS					m_flags;
};

class IndexBuffer
{
public:
	IndexBuffer(size_t numElements, size_t elementSize, void* bufferData, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE)
		: m_data(bufferData), m_numElements(numElements), m_elementSize(elementSize), m_flags(flags) {}
	~IndexBuffer() = default;

	D3D12_INDEX_BUFFER_VIEW& updateBufferResource(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> copyCommandList, ID3D12Resource** intermediateResource);

	Microsoft::WRL::ComPtr<ID3D12Resource> getBuffer() const { return m_buffer; }
	D3D12_INDEX_BUFFER_VIEW getBufferView() const { return m_bufferView; }
	const void* getData() const { return m_data; }

private:
	Microsoft::WRL::ComPtr<ID3D12Resource>	m_buffer;
	D3D12_INDEX_BUFFER_VIEW					m_bufferView;
	void*									m_data;
	size_t									m_numElements;
	size_t									m_elementSize;
	D3D12_RESOURCE_FLAGS					m_flags;
};

struct VertexBufferDescription
{
	struct VertexElementDescription
	{
		LPCSTR semanticName;
		UINT semanticIndex;
		DXGI_FORMAT format;
	};

	std::shared_ptr<VertexBuffer> vbo;
	std::vector<VertexElementDescription> elements;

	VertexBufferDescription(std::shared_ptr<VertexBuffer>& v, std::initializer_list<VertexElementDescription> e)
		: vbo(v), elements(e) {}
};

class VertexArray
{
public:
	VertexArray() = default;
	~VertexArray();

	void bind(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2>& commandList);

	std::vector<D3D12_INPUT_ELEMENT_DESC> setVertexBuffers(std::initializer_list<VertexBufferDescription> elementDescriptions);
	void setIndexBuffer(std::shared_ptr<IndexBuffer>& indexBuffer) { m_indexBuffer = indexBuffer; }

	void uploadDataToGPU(std::shared_ptr<CommandQueue>& copyCommandQueue);

private:
	std::vector<std::shared_ptr<VertexBuffer>>	m_vertexBuffers;
	std::shared_ptr<IndexBuffer>				m_indexBuffer;

	// Combined vertex buffers
	D3D12_VERTEX_BUFFER_VIEW*					m_vertexBufferViews = nullptr;
	UINT										m_vertexBufferViewCount;
};