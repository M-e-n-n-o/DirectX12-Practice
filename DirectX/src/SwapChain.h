#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// Windows Runtime Library. Needed for Microsoft::WRL::ComPtr<> template class.
#include <wrl.h>

// DirectX 12 specific headers.
#include <d3d12.h>

// STL Headers
#include <algorithm>
#include <vector>

class SwapChain
{
public:
	SwapChain(Microsoft::WRL::ComPtr<ID3D12Device2> device, Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue, 
		uint32_t width, uint32_t height, uint32_t bufferCount, HWND windowHandle, bool tearingSupported);
	~SwapChain() = default;

	void present();

	void resize(uint32_t width, uint32_t height);

	bool isVSync() { return m_vSync; }
	void setVSync(bool vSync) { m_vSync = vSync; }

	D3D12_CPU_DESCRIPTOR_HANDLE getCurrentRenderTargetView() const;

	Microsoft::WRL::ComPtr<ID3D12Resource> getCurrentBackBuffer() const { return m_backBuffers[m_currentBackBufferIndex]; }
	uint32_t getCurrentBackBufferIndex() const { return m_currentBackBufferIndex; }
	
private:
	void updateRenterTargetViews();

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> createDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptors);

private:
	Microsoft::WRL::ComPtr<IDXGISwapChain4> m_swapChain;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_RTVDescriptorHeap;
	UINT m_RTVDescriptorSize;

	UINT m_currentBackBufferIndex;

	bool m_vSync = true;
	bool m_isTearingSupported;

	uint32_t m_bufferCount;
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_backBuffers;

	Microsoft::WRL::ComPtr<ID3D12Device2> m_device;
};