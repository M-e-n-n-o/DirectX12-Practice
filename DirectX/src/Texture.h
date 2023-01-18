#pragma once

#include "CommandQueue.h"

#include "d3dx12.h"
#include <wrl.h>

#include <string>

class Texture
{
public:
	Texture();
	virtual ~Texture();

	void loadTextureFromFile(std::shared_ptr<CommandQueue>& copyCommandQueue, const std::string& fileName);

private:
	void copyTextureSubResource(ID3D12GraphicsCommandList1* commandList, uint32_t firstSubresource, uint32_t numSubresources, 
		D3D12_SUBRESOURCE_DATA* subresourceData, Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource);

	Microsoft::WRL::ComPtr<ID3D12Resource> m_textureResource;
};