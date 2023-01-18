#include "dxpch.h"
#include "Texture.h"
#include "Application.h"
#include "ResourceStateTracker.h"

#include "d3dx12.h"
#include "stb_image.h"

void Texture::loadTextureFromFile(std::shared_ptr<CommandQueue>& copyCommandQueue, const std::string& fileName)
{
	unsigned char* imgData = nullptr;
	int width;
	int height;
	int channels;

	imgData = stbi_load(fileName.c_str(), &width, &height, &channels, 0);

	assert(imgData != nullptr && "Could not load image");

	DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
	switch (channels)
	{
	case 3: format = DXGI_FORMAT_R32G32B32_FLOAT; break;
	case 4: format - DXGI_FORMAT_R32G32B32A32_FLOAT; break;
	}

	D3D12_RESOURCE_DESC textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		format,
		static_cast<UINT64>(width),
		static_cast<UINT64>(height));

	auto device = Application::Get()->getDevice();

	// Allocate GPU memory for the texture
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&m_textureResource)
	));

	ResourceStateTracker::AddGlobalResourceState(m_textureResource.Get(), D3D12_RESOURCE_STATE_COMMON);

	// Load texture data
	D3D12_SUBRESOURCE_DATA subresource;
	subresource.pData = imgData;
	subresource.RowPitch = sizeof(imgData);
	subresource.SlicePitch = subresource.RowPitch;

	auto commandList = copyCommandQueue->getCommandList();

	ComPtr<ID3D12Resource> intermediateResource;
	copyTextureSubResource(commandList.Get(), 0, 1, &subresource, intermediateResource);

	//GenerateMips();

	// Upload the image data to the GPU
	copyCommandQueue->executeCommandList(commandList);
	auto fenceValue = copyCommandQueue->executeCommandList(commandList);
	copyCommandQueue->waitForFenceValue(fenceValue);

	stbi_image_free(imgData);
}

void Texture::copyTextureSubResource(ID3D12GraphicsCommandList1* commandList, uint32_t firstSubresource, uint32_t numSubresources, D3D12_SUBRESOURCE_DATA* subresourceData, Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource)
{
	auto device = Application::Get()->getDevice();
	
	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		m_textureResource.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
	commandList->ResourceBarrier(1, &barrier);

	UINT requiredSize = GetRequiredIntermediateSize(m_textureResource.Get(), firstSubresource, numSubresources);

	// Create a temporary (intermediate) resource for uploading the subresources
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(requiredSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&intermediateResource)
	));

	UpdateSubresources(commandList, m_textureResource.Get(), intermediateResource.Get(), 0, firstSubresource, numSubresources, subresourceData);
}
